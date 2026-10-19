/**
 * Grow Controller: Sensor-ESP32 mit OLED und ADS1115.
 *
 * - 3x DHT22, DS18B20 (D23), ADS1115 + OLED (D21/D22), BH1750 (D13/D14)
 * - Kamera MLX90640 (optional) am OLED-I2C D21/D22 (0x33)
 * - HTTP-API fuer CYD/Raspberry Pi, Telemetrie und pH-Alarm
 * - Zweites Geraet: ESP32-C3 SuperMini als eigenstaendiger Leistungscontroller.
 *
 * Sensorplatine: 26x30-Lochraster; dieser Sketch bedient KEINE Relaispins.
 * Alte lokale AUTO/MANUAL-Sollwertberechnung bleibt fuer API-Kompatibilitaet,
 * sendet aber NOCH NICHT an das C3-Geraet. Aktorzustaende = NICHT VERBUNDEN.
 * Kein netzspannungsseitiger Aufbau in diesem Firmwareprojekt.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// MLX90640 erst aktivieren, wenn Modul und Adafruit_MLX90640 installiert sind.
#ifndef GROW_ENABLE_MLX90640
#define GROW_ENABLE_MLX90640 0
#endif
#if GROW_ENABLE_MLX90640
#include <Adafruit_MLX90640.h>
#endif

/* Die Buzzer-Bauart ist noch unbekannt. Daher standardmaessig AUS.
 * Bei einem bestaetigten 3.3-V-kompatiblen aktiven Modul mit Logikeingang
 * spaeter auf 1 setzen; GPIO18 ist kein Versorgungspin.
 * Passive Piezoelemente benoetigen eine separate PWM-Anpassung.
 */
#ifndef GROW_ENABLE_PH_BUZZER
#define GROW_ENABLE_PH_BUZZER 0
#endif
/* RGB: gemeinsame Kathode vom Nutzer bestaetigt, GND gemeinsam.
 * R D33 und G D4 = Phasenanzeige; B D17 (TX2) = Filterwechsel.
 * Je Kanal zwei 220-Ohm-Widerstaende in Reihe (440 Ohm).
 * BLAU darf nur bei vom Leistungscontroller bestaetigter Pumpenabschaltung
 * waehrend Filterwartung leuchten. C3-Kommunikation fehlt noch:
 * B bleibt aktuell bewusst AUS; GPIO18-Taster ist noch nicht aktiv.
 */
#ifndef GROW_ENABLE_PHASE_RGB
#define GROW_ENABLE_PHASE_RGB 1 // Gemeinsame Kathode: LED gruen D4, rot D33; DS18B20 D23
#endif
#ifndef GROW_RGB_COMMON_ANODE
#define GROW_RGB_COMMON_ANODE 0
#endif
#ifndef GROW_PH_BUZZER_ACTIVE_HIGH
#define GROW_PH_BUZZER_ACTIVE_HIGH 1
#endif

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
const char *WIFI_SSID = "YOUR_SSID";
const char *WIFI_PASS = "YOUR_PASSWORD";
const char *SERVER_HOST = "192.168.1.50";
const uint16_t SERVER_PORT = 8765;
const char *TELEMETRY_API_KEY = "";
const bool SERVER_USE_HTTPS = false;
const bool SERVER_TLS_INSECURE = false;
#endif

#include "grow_http_client.h"

/* ---------- Pins ---------- */
static constexpr uint8_t PIN_DHT1 = 25;
static constexpr uint8_t PIN_DHT2 = 26;
static constexpr uint8_t PIN_DHT3 = 27;
static constexpr uint8_t PIN_PHASE_BUTTON = 19; // Taster an D19
static constexpr uint8_t PIN_RGB_GREEN = 4; // Gruen an D4; GPIO32 liefert kein Signal
static constexpr uint8_t PIN_RGB_RED = 33;
static constexpr uint8_t PIN_RGB_BLUE = 17; // TX2: Pumpen-Filterwechsel, nur bei bestaetigtem Pumpen-AUS
static constexpr uint8_t PIN_PH_BUZZER = 16; // Buzzer an D16; deaktiviert bis Typ/Treiber geklaert
static constexpr unsigned long PHASE_DEBOUNCE_MS = 50;
static constexpr uint8_t PIN_ONEWIRE = 23; // DS18B20 auf D23; PH4502C DO wird nicht genutzt
// Erst nach verifizierter TO-Ausgangsspannung <= ADS-VDD (3.3 V) einschalten.
static constexpr bool PH_TO_WIRED = false;
static constexpr bool PH_DO_WIRED = false; // PH4502C DO bleibt ungenutzt

static constexpr int PIN_I2C_SDA = 21;
static constexpr int PIN_I2C_SCL = 22;
static constexpr int PIN_BH_SDA = 13;
static constexpr int PIN_BH_SCL = 14;

/* ---------- Intervale ---------- */
static constexpr unsigned long SENSOR_READ_MS = 2000;
static constexpr unsigned long OLED_REFRESH_MS = 1000;
static constexpr unsigned long TELEMETRY_INTERVAL_MS = 10000;
static constexpr unsigned long WIFI_RECONNECT_MS = 5000;

/* ---------- Regelparameter ---------- */
static constexpr float TEMP_HYST = 0.5f;
static constexpr float HUM_HYST = 2.0f;
static constexpr float PH_LOW = 5.5f;
static constexpr float PH_HIGH = 6.2f;
static constexpr float PH_HYST = 0.10f;
static constexpr uint8_t PH_CONFIRM = 3;

/* pH-Kalibrierung von Diagnose-Sketch in ESP32-NVS ("phads_v1").
 * Ohne 2 echte Kalibrierpunkte keine Grenzwertalarm-Entscheidung.
 */
float phVolt4 = NAN, phVolt7 = NAN;
bool phCalibrated = false;
float phCalTemp4C = NAN, phCalTemp7C = NAN;
bool ecCalibrated = false;
float ecCalTempC = NAN;
bool waterSensorRomSelected = false;
uint8_t waterSensorRom[8] = {};
bool toCalibrated = false;
float toVoltage1 = NAN, toVoltage2 = NAN, toTemp1C = NAN, toTemp2C = NAN;
float ecFactor = NAN;
float ecReferenceInputV = NAN;
static void loadEcCalibration() {
  Preferences prefs;
  ecCalibrated = false;
  if (!prefs.begin("ecads_v1", true)) return;
  const bool valid = prefs.getBool("valid", false);
  ecFactor = prefs.getFloat("factor", NAN);
  ecReferenceInputV = prefs.getFloat("ref_v", NAN);
  ecCalTempC = prefs.getFloat("temp_c", NAN);
  prefs.end();
  ecCalibrated = valid && isfinite(ecFactor) &&
      ecFactor >= 0.1f && ecFactor <= 10.0f &&
      isfinite(ecReferenceInputV) && ecReferenceInputV > 0.05f &&
      ecReferenceInputV <= 2.3f;
}
static void loadPhCalibration() {
  Preferences prefs;
  phCalibrated = false;
  if (!prefs.begin("phads_v1", true)) return;
  const bool have4 = prefs.getBool("have4", false);
  const bool have7 = prefs.getBool("have7", false);
  phVolt4 = prefs.getFloat("volt4", NAN);
  phVolt7 = prefs.getFloat("volt7", NAN);
  phCalTemp4C = prefs.getFloat("temp4", NAN);
  phCalTemp7C = prefs.getFloat("temp7", NAN);
  prefs.end();
  phCalibrated = have4 && have7 && isfinite(phVolt4) && isfinite(phVolt7) &&
      phVolt4 >= 0.0f && phVolt4 <= 3.3f &&
      phVolt7 >= 0.0f && phVolt7 <= 3.3f &&
      fabsf(phVolt4 - phVolt7) >= 0.001f;
}

/* ---------- OLED / I2C ---------- */
static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr int OLED_RESET = -1;
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
bool oledOk = false;
Adafruit_ADS1115 ads;

/* ADS1115 + OLED auf Wire D21/D22, BH1750 separat auf zweitem I2C-Controller D13/D14 */
TwoWire bhWire = TwoWire(1);
static uint8_t bh1750Addr = 0;
static bool bh1750Present = false;
static TwoWire* bhSelectedWire = &bhWire;
static uint8_t adsFoundAddr = 0;
static const char* adsFoundBus = "nicht gefunden";
static const char* bhFoundBus = "nicht gefunden";

/* ---------- Sensoren ---------- */
DHT dht1(PIN_DHT1, DHT22);
DHT dht2(PIN_DHT2, DHT22);
DHT dht3(PIN_DHT3, DHT22);
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature ds18b20(&oneWire);

struct AirReading {
  float tempC = NAN;
  float rh = NAN;
  bool ok = false;
};
// Deklaration nach Typdefinition: verhindert ungueltige Arduino-Autoprototypen.
static AirReading readDht(DHT &dht);

struct SensorData {
  AirReading dht[3];
  float ds1C = NAN;
  float ds2C = NAN;
  float waterTempC = NAN;
  float caseTempC = NAN;
  bool phTempCompensated = false;
  uint8_t dsCount = 0;

  float phInputV = NAN;
  float ph = NAN;
  float phToInputV = NAN; // TO raw volts, no temperature conversion yet
  bool phDoHigh = false;
  float tdsInputV = NAN;
  float tdsPpm = NAN;
  float ec = NAN; // mS/cm bei der aktuell gemessenen DS18B20-Wassertemperatur

  float vpd = NAN; // bestehender Luft-VPD (DHT1), unveraendert
  float thermalRoiTempC = NAN; // mittlere Bildregion, NOCH NICHT als Blatt validiert
  float vpdLeafCandidate = NAN; // vorlaeufiger Blatt-VPD: ROI + mind. zwei DHT22
  bool mlx90640Ok = false;
  uint32_t lux = 0;

  bool adsOk = false;
} sensors;

/* ---------- Aktor-Soll / Ist ---------- */
struct ActuatorState {
  bool light = false;
  bool heater = false;
  bool circulation = false;
  bool exhaust = false;
};

ActuatorState targets;
ActuatorState actual;  // C3-Rueckmeldung fehlt noch: alle Aktoren als AUS / unbekannt
uint32_t commandSeq = 0;
char controlMode[8] = "auto";


/* Ein rastfreier Schliesser D19-GND, eine 4-Pin-RGB-LED.
 * Gruen (D4) = Wuchs, Rot+Gruen (D33+D4) = Bluete (Gelb).
 * Jeder Farbkanal hat zwei 220-Ohm-Widerstaende in Reihe (440 Ohm).
 * Gemeinsame Kathode an GND (bestaetigt).
 * Auswahl wird in NVS gespeichert, unabhaengig von pH/EC.
 */
enum class GrowPhase : uint8_t { Wuchs = 0, Bluete = 1 };
struct PhaseProfile {
  float targetAirC;
  float targetHumidityPct;
  bool configured;
};
static const char *phaseName(GrowPhase p);
static const PhaseProfile &phaseProfile();
static void setGrowPhase(GrowPhase next);
static constexpr PhaseProfile WUCHS_PROFILE = {24.0f, 60.0f, true};
static constexpr PhaseProfile BLUETE_PROFILE = {NAN, NAN, false};
static constexpr char PHASE_NVS[] = "growphase_v1";
GrowPhase growPhase = GrowPhase::Wuchs;
// FALLING-Interrupt erfasst auch kurze Tastendruecke waehrend blockierender ADC-Messungen.
// Kein Kontaktprellen als mehrere Eingaben werten (50 ms Mindestabstand).
static volatile uint32_t phasePendingPresses = 0;
static volatile uint32_t phaseLastInterruptUs = 0;
static void IRAM_ATTR onPhaseButtonFalling() {
  const uint32_t us = micros();
  if (static_cast<uint32_t>(us - phaseLastInterruptUs) >= PHASE_DEBOUNCE_MS * 1000UL) {
    phaseLastInterruptUs = us;
    if (phasePendingPresses < 1000000UL) ++phasePendingPresses;
  }
}
static const char *phaseName(GrowPhase p) {
  return p == GrowPhase::Bluete ? "bluete" : "wuchs";
}
static const PhaseProfile &phaseProfile() {
  return growPhase == GrowPhase::Bluete ? BLUETE_PROFILE : WUCHS_PROFILE;
}
static void refreshPhaseRgb() {
#if GROW_ENABLE_PHASE_RGB
  const bool redOn = growPhase == GrowPhase::Bluete;
  const bool greenOn = true;
  digitalWrite(PIN_RGB_RED, redOn == (GROW_RGB_COMMON_ANODE == 0) ? HIGH : LOW);
  digitalWrite(PIN_RGB_GREEN, greenOn == (GROW_RGB_COMMON_ANODE == 0) ? HIGH : LOW);
#endif
}
static void loadGrowPhase() {
  Preferences prefs;
  if (prefs.begin(PHASE_NVS, true)) {
    growPhase = prefs.getUChar("phase", 0) == 1 ? GrowPhase::Bluete : GrowPhase::Wuchs;
    prefs.end();
  } else {
    Serial.println(F("WARNUNG: Phasenspeicher nicht lesbar; starte mit WUCHS."));
  }
  refreshPhaseRgb();
  Serial.printf("PHASE: %s | Sollwertprofil: %s\n",
      phaseName(growPhase), phaseProfile().configured ? "bereit" : "FEHLT");
}
static void setGrowPhase(GrowPhase next) {
  if (next == growPhase) return;
  growPhase = next;
  refreshPhaseRgb();
  Preferences prefs;
  bool saved = false;
  if (prefs.begin(PHASE_NVS, false)) {
    saved = prefs.putUChar("phase", static_cast<uint8_t>(next)) == sizeof(uint8_t);
    prefs.end();
  }
  targets = ActuatorState{};
  ++commandSeq;
  Serial.printf("PHASE -> %s | Profil %s | Speicher %s\n",
      phaseName(growPhase), phaseProfile().configured ? "OK" : "NICHT KONFIGURIERT",
      saved ? "OK" : "FEHLER");
}
static void pollGrowPhaseButtons(unsigned long nowMs) {
  (void)nowMs;
  uint32_t pressedCount;
  noInterrupts();
  pressedCount = phasePendingPresses;
  phasePendingPresses = 0;
  interrupts();
  if (pressedCount == 0) return;
  Serial.printf("TASTER D19: %lu Druck-Flanke(n), GPIO19 aktuell %s\n",
                static_cast<unsigned long>(pressedCount),
                digitalRead(PIN_PHASE_BUTTON) == LOW ? "LOW (gedrueckt)" : "HIGH (offen)");
  // Bei zwei sehr schnellen Druecken wird zweimal umgeschaltet -> Ursprungsphase.
  if (pressedCount & 1UL) {
    setGrowPhase(growPhase == GrowPhase::Wuchs ? GrowPhase::Bluete : GrowPhase::Wuchs);
  }
}
static void setupGrowPhaseButtons() {
  pinMode(PIN_PHASE_BUTTON, INPUT_PULLUP);
#if GROW_ENABLE_PHASE_RGB
  // Logik: OFF bei Kathode LOW, OFF bei Anode HIGH.
  const uint8_t off = GROW_RGB_COMMON_ANODE ? HIGH : LOW;
  digitalWrite(PIN_RGB_RED, off);
  digitalWrite(PIN_RGB_GREEN, off);
  digitalWrite(PIN_RGB_BLUE, off); // keine C3-Rueckmeldung: blau muss AUS bleiben
  pinMode(PIN_RGB_RED, OUTPUT);
  pinMode(PIN_RGB_GREEN, OUTPUT);
  pinMode(PIN_RGB_BLUE, OUTPUT);
#endif
  phasePendingPresses = 0;
  phaseLastInterruptUs = micros() - PHASE_DEBOUNCE_MS * 1000UL;
  attachInterrupt(digitalPinToInterrupt(PIN_PHASE_BUTTON), onPhaseButtonFalling, FALLING);
  Serial.printf("TASTER D19 beim Start: %s (INPUT_PULLUP, nach GND druecken)\n",
                digitalRead(PIN_PHASE_BUTTON) == LOW ? "LOW (Kontakt bereits geschlossen)" : "HIGH (offen)");
  loadGrowPhase();
}

/* Nutzersollwerte, getrennt von pH-Alarmgrenzen und Sensor-Kalibrierung.
 * Kein Wochenstartdatum gegeben: keine automatische Fortschaltung.
 * 0 = Woche nicht eingestellt; 1..12 = explizit ausgewaehlte Woche.
 * EC in mS/cm, pH dimensionslos.
 */
struct NutrientTargets {
  float ph;
  float ecMsCm;
};
static const NutrientTargets *activeNutrientTargets();
static constexpr NutrientTargets NUTRIENT_WEEK_TARGETS[12] = {
    {6.0f, 1.6f}, // Woche 1
    {5.8f, 1.8f}, // Woche 2
    {5.5f, 2.0f}, // Woche 3
    {5.5f, 2.0f}, // Woche 4
    {5.5f, 2.0f}, // Woche 5
    {5.5f, 2.0f}, // Woche 6
    {5.5f, 2.0f}, // Woche 7
    {5.5f, 2.5f}, // Woche 8
    {5.5f, 1.8f}, // Woche 9
    {5.5f, 1.8f}, // Woche 10
    {5.5f, 0.2f}, // Woche 11
    {5.5f, 0.2f}  // Woche 12
};
static constexpr char GROW_WEEK_NVS[] = "growweek_v1";
static uint8_t growWeek = 0;
static const NutrientTargets *activeNutrientTargets() {
  return growWeek >= 1 && growWeek <= 12
      ? &NUTRIENT_WEEK_TARGETS[growWeek - 1] : nullptr;
}
static void loadGrowWeek() {
  Preferences prefs;
  if (!prefs.begin(GROW_WEEK_NVS, true)) {
    Serial.println(F("WOCHE: nicht gesetzt (NVS nicht lesbar)."));
    return;
  }
  const uint8_t saved = prefs.getUChar("week", 0);
  prefs.end();
  growWeek = saved <= 12 ? saved : 0;
  if (const NutrientTargets *t = activeNutrientTargets()) {
    Serial.printf("WOCHE %u | Soll pH %.2f | Soll EC %.2f mS/cm\n",
        growWeek, t->ph, t->ecMsCm);
  } else {
    Serial.println(F("WOCHE: nicht gesetzt; pH/EC-Sollwerte noch nicht aktiv."));
  }
}
static bool setGrowWeek(int week) {
  if (week < 1 || week > 12) return false;
  if (growWeek == static_cast<uint8_t>(week)) return true;
  Preferences prefs;
  if (!prefs.begin(GROW_WEEK_NVS, false)) return false;
  const bool saved = prefs.putUChar("week", static_cast<uint8_t>(week)) == sizeof(uint8_t);
  prefs.end();
  if (!saved) return false;
  growWeek = static_cast<uint8_t>(week);
  const NutrientTargets *t = activeNutrientTargets();
  Serial.printf("WOCHE -> %u | pH-Soll %.2f | EC-Soll %.2f mS/cm (NVS gespeichert)\n",
      growWeek, t->ph, t->ecMsCm);
  return true;
}
static void writeNutrientTargets(JsonObject obj) {
  const NutrientTargets *t = activeNutrientTargets();
  obj["grow_week"] = growWeek == 0 ? 0 : growWeek;
  if (t) {
    obj["ph_target"] = t->ph;
    obj["ec_target_ms_cm"] = t->ecMsCm;
  } else {
    obj["ph_target"] = nullptr;
    obj["ec_target_ms_cm"] = nullptr;
  }
  obj["ph_alarm_min"] = PH_LOW;
  obj["ph_alarm_max"] = PH_HIGH;
}

enum class PhAlarm : uint8_t { Normal, Low, High, Fault };
static const char *phCode(PhAlarm status);
static const char *phMessage(PhAlarm status);
static void setPhAlarm(PhAlarm next);
static PhAlarm evaluatePhAlarm(float v, bool measurementValid,
                               PhAlarm current, PhAlarm &pending, uint8_t &samples);
PhAlarm phAlarm = PhAlarm::Fault, phPending = PhAlarm::Fault;
uint8_t phSamples = 0;
bool phAlertPending = true; // initial sensor fault is an alert
unsigned long lastPhAlertAttempt = 0;
static const char *phCode(PhAlarm status) {
  switch (status) {
    case PhAlarm::Normal: return "normal";
    case PhAlarm::Low: return "low";
    case PhAlarm::High: return "high";
    default: return "sensor_fault";
  }
}
static const char *phMessage(PhAlarm status) {
  switch (status) {
    case PhAlarm::Normal: return "pH wieder im Bereich 5.5-6.2";
    case PhAlarm::Low: return "pH-Alarm: unter 5.5";
    case PhAlarm::High: return "pH-Alarm: ueber 6.2";
    default: return "pH-Sensoralarm: ungueltige Messung oder Kalibrierung";
  }
}
static void setPhAlarm(PhAlarm next) {
  if (next == phAlarm) return;
  phAlarm = next;
  phAlertPending = true;
  Serial.printf("pH-STATUS: %s\n", phMessage(next));
}

/* Eine Zustandsfunktion fuer echte Messwerte UND gekennzeichnete Testwerte. */
static PhAlarm evaluatePhAlarm(float v, bool measurementValid,
                              PhAlarm current, PhAlarm &pending, uint8_t &samples) {
  if (!measurementValid || !isfinite(v) || v < 0.0f || v > 14.0f) {
    samples = 0;
    pending = PhAlarm::Fault;
    return PhAlarm::Fault;
  }
  PhAlarm next = PhAlarm::Normal;
  if (v < PH_LOW || (current == PhAlarm::Low && v < PH_LOW + PH_HYST))
    next = PhAlarm::Low;
  else if (v > PH_HIGH || (current == PhAlarm::High && v > PH_HIGH - PH_HYST))
    next = PhAlarm::High;
  if (next == current) { samples = 0; return current; }
  if (next != pending) { pending = next; samples = 1; }
  else if (samples < PH_CONFIRM) ++samples;
  if (samples >= PH_CONFIRM) { samples = 0; return next; }
  return current;
}
static void updatePhAlarm() {
  setPhAlarm(evaluatePhAlarm(
      sensors.ph, phCalibrated && sensors.adsOk,
      phAlarm, phPending, phSamples));
}

static bool phTestActive = false;
static float phTestValue = NAN;
static PhAlarm phTestAlarm = PhAlarm::Fault, phTestPending = PhAlarm::Fault;
static uint8_t phTestSamples = 0;
static bool phTestAlertPending = false;
static unsigned long lastPhTestAlertAttempt = 0;

static void updatePhTestAlarm() {
  if (!phTestActive) return;
  const PhAlarm next = evaluatePhAlarm(phTestValue, true, phTestAlarm,
                                       phTestPending, phTestSamples);
  if (next == phTestAlarm) return;
  phTestAlarm = next;
  phTestAlertPending = true;
  Serial.printf("[pH TEST] %s\n", phMessage(phTestAlarm));
}
static void setPhTestMode(char c) {
  if (c == '0') {
    phTestActive = false;
    phTestValue = NAN;
    phTestAlertPending = false;
    Serial.printf("[pH TEST AUS] Echtes Sensor-Alarmsignal: %s\n", phMessage(phAlarm));
    return;
  }
  phTestActive = true;
  phTestValue = c == '1' ? 5.40f : c == '2' ? 6.30f : 6.00f;
  phTestAlarm = PhAlarm::Fault;
  phTestPending = PhAlarm::Fault;
  phTestSamples = 0;
  phTestAlertPending = false;
  Serial.printf("[pH TEST EIN] pH %.2f simuliert; Sensor/NVS/SSR unveraendert.\n",
                phTestValue);
  for (uint8_t i = 0; i < PH_CONFIRM; ++i) updatePhTestAlarm();
  Serial.printf("[pH TEST] Ergebnis: %s (3 simulierte Messungen)\n",
                phMessage(phTestAlarm));
}
static void scanI2CAndBind(bool forceFullScan = false); // ADS fest D21/D22; BH fest D13/D14; 'i' Diagnose

static void handlePhTestSerial() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c >= '0' && c <= '3') setPhTestMode(c);
    else if (c == 'i' || c == 'I') scanI2CAndBind(true);
    else if (c == 'k' || c == 'K') {
      String arg = Serial.readStringUntil('\n');
      arg.trim();
      const int week = arg.toInt();
      if (!setGrowWeek(week)) Serial.println(F("WOCHE ungueltig/nicht gespeichert: k1 bis k12"));
    }
    else if (c == 'h' || c == 'H' || c == '?') {
      Serial.println(F("pH-Test: 1=LOW 5.40 | 2=HIGH 6.30 | 3=NORMAL 6.00 | 0=Test aus"));
      Serial.println(F("Woche per Serial: k1 bis k12 + Zeilenumbruch, im NVS gespeichert."));
      Serial.println(F("i = alle I2C-Adressen auf D13/D14 und D21/D22 anzeigen (keine Neuzuordnung)"));
    }
  }
}



/* Nicht blockierende akustische Warnung: LOW/HIGH in 5-Sekunden-Abstand,
 * Sensorfehler alle 10 Sekunden. Gleiches Muster im pH-Softwaretest.
 * Kein Dauerton. Der Pin bleibt deaktiviert, solange die Buzzerart unklar ist.
 */
static void setupPhBuzzer() {
#if GROW_ENABLE_PH_BUZZER
  digitalWrite(PIN_PH_BUZZER, GROW_PH_BUZZER_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PIN_PH_BUZZER, OUTPUT);
#endif
}
static void updatePhBuzzer(unsigned long nowMs) {
#if GROW_ENABLE_PH_BUZZER
  const PhAlarm shown = phTestActive ? phTestAlarm : phAlarm;
  static PhAlarm last = PhAlarm::Normal;
  static bool lastTest = false;
  static unsigned long sinceMs = 0;
  if (shown != last || lastTest != phTestActive) {
    last = shown;
    lastTest = phTestActive;
    sinceMs = nowMs;
  }
  const unsigned long slot = (nowMs - sinceMs) %
                             (shown == PhAlarm::Fault ? 10000UL : 5000UL);
  bool sound = false;
  if (shown == PhAlarm::Low) {
    sound = slot < 180UL || (slot >= 330UL && slot < 510UL);
  } else if (shown == PhAlarm::High) {
    sound = slot < 180UL || (slot >= 330UL && slot < 510UL) ||
            (slot >= 660UL && slot < 840UL);
  } else if (shown == PhAlarm::Fault) {
    sound = slot < 120UL;
  }
  digitalWrite(PIN_PH_BUZZER,
               sound == (GROW_PH_BUZZER_ACTIVE_HIGH != 0) ? HIGH : LOW);
#else
  (void)nowMs;
#endif
}

/* ---------- Lokale API ---------- */
static constexpr uint16_t LOCAL_HTTP_PORT = 8081;
WebServer localServer(LOCAL_HTTP_PORT);
const char *LOCAL_HEADER_KEYS[] = {"X-Telemetry-Key"};

/* ---------- Timing ---------- */
unsigned long lastSensorRead = 0;
unsigned long lastOledRefresh = 0;
unsigned long lastTelemetry = 0;
unsigned long lastWifiReconnect = 0;

/* ---------- Hilfsfunktionen ---------- */

static float computeVpd(float tC, float rh) {
  if (isnan(tC) || isnan(rh) || rh < 0.0f || rh > 100.0f) return NAN;
  const float svp = 0.61078f * expf((17.27f * tC) / (tC + 237.3f));
  return svp * (1.0f - rh / 100.0f);
}

static AirReading readDht(DHT &dht) {
  AirReading r;
  r.tempC = dht.readTemperature();
  r.rh = dht.readHumidity();
  r.ok = isfinite(r.tempC) && isfinite(r.rh) &&
         r.tempC > -20.0f && r.tempC < 70.0f &&
         r.rh >= 0.0f && r.rh <= 100.0f;
  if (!r.ok) {
    r.tempC = NAN;
    r.rh = NAN;
  }
  return r;
}

static float adsCountsToVolts(int16_t raw) {
  /* GAIN_ONE = +/-4.096 V -> 125 uV/bit */
  return static_cast<float>(raw) * 0.000125f;
}

static bool validDsTempC(float t) {
  return isfinite(t) && t > -20.0f && t < 80.0f &&
         t != DEVICE_DISCONNECTED_C;
}
static void loadWaterSensorRom() {
  waterSensorRomSelected = false;
  Preferences p;
  if (!p.begin("waterds_v1", true)) return;
  if (p.getBytesLength("rom") == 8) {
    p.getBytes("rom", waterSensorRom, 8);
    waterSensorRomSelected =
        waterSensorRom[0] == 0x28 &&
        OneWire::crc8(waterSensorRom, 7) == waterSensorRom[7];
  }
  p.end();
}
static void loadToCalibration() {
  toCalibrated = false;
  Preferences p;
  if (!p.begin("toads_v1", true)) return;
  const bool valid = p.getBool("valid", false);
  toVoltage1 = p.getFloat("v1", NAN);
  toVoltage2 = p.getFloat("v2", NAN);
  toTemp1C = p.getFloat("c1", NAN);
  toTemp2C = p.getFloat("c2", NAN);
  p.end();
  toCalibrated = valid && isfinite(toVoltage1) && isfinite(toVoltage2) &&
      isfinite(toTemp1C) && isfinite(toTemp2C) &&
      toVoltage1 > 0.05f && toVoltage1 < 3.25f &&
      toVoltage2 > 0.05f && toVoltage2 < 3.25f &&
      toTemp1C >= -5.0f && toTemp1C <= 60.0f &&
      toTemp2C >= -5.0f && toTemp2C <= 60.0f &&
      fabsf(toVoltage1 - toVoltage2) >= 0.025f &&
      fabsf(toTemp1C - toTemp2C) >= 5.0f;
}
static float toCaseTemp(float v) {
  if (!PH_TO_WIRED || !toCalibrated || !isfinite(v)) return NAN;
  // Nur innerhalb der beiden Referenzpunkte interpolieren.
  if (v < fminf(toVoltage1, toVoltage2) ||
      v > fmaxf(toVoltage1, toVoltage2)) return NAN;
  const float temp = toTemp1C + (v - toVoltage1) *
      (toTemp2C - toTemp1C) / (toVoltage2 - toVoltage1);
  return temp >= -5.0f && temp <= 60.0f ? temp : NAN;
}
static bool phTempCompAvailable(float waterTempC) {
  return phCalibrated && validDsTempC(waterTempC) &&
      validDsTempC(phCalTemp4C) && validDsTempC(phCalTemp7C) &&
      fabsf(phCalTemp4C - phCalTemp7C) <= 3.0f;
}

static float phFromVoltage(float v, float waterTempC) {
  if (!phCalibrated || !isfinite(v) || v < 0.0f || v > 3.3f) return NAN;
  float phDeltaFrom7 = (v - phVolt7) * (-3.0f) / (phVolt4 - phVolt7);
  // Nernst-Abhaengigkeit der Elektrodensteilheit: proportional zur absoluten Temperatur.
  // Nur wenn pH4/pH7 Kalibrier-Temperaturen gemessen und gueltig sind.
  if (phTempCompAvailable(waterTempC)) {
    const float calKelvin = 273.15f + (phCalTemp4C + phCalTemp7C) * 0.5f;
    phDeltaFrom7 *= calKelvin / (273.15f + waterTempC);
  }
  return 7.0f + phDeltaFrom7;
}

static float cqrobotTdsPpm(float inputV, float temperatureC) {
  if (!isfinite(inputV) || !validDsTempC(temperatureC)) return NAN;
  const float coefficient = 1.0f + 0.02f * (temperatureC - 25.0f);
  if (coefficient <= 0.1f) return NAN;
  const float compensatedV = inputV / coefficient;
  const float ppm =
      (133.42f * compensatedV * compensatedV * compensatedV
       - 255.86f * compensatedV * compensatedV
       + 857.39f * compensatedV) * 0.5f;
  return ppm < 0.0f ? 0.0f : ppm;
}
static float cqrobotEcMs(float inputV, float waterTempC) {
  if (!ecCalibrated || !validDsTempC(waterTempC)) return NAN;
  // cqrobotTdsPpm und ecFactor liefern die Referenzleitfaehigkeit bei 25 C.
  // Ausgabe dagegen stets bei tatsaechlicher DS18B20-Temperatur, ohne
  // die reale Wassertemperatur jemals als 25 C anzunehmen.
  const float ppmAt25C = cqrobotTdsPpm(inputV, waterTempC);
  const float tempFactor = 1.0f + 0.02f * (waterTempC - 25.0f);
  if (!isfinite(ppmAt25C) || tempFactor <= 0.1f) return NAN;
  return (ppmAt25C * ecFactor / 500.0f) * tempFactor;
}
/* CQRobot-Abtastung: 30 ADS1115-A1-Werte im Abstand von 40 ms.
 * Ringpuffer verhindert 1.2 s blockierendes Lesen im Regel-/HTTP-Loop.
 * Der pH-Kanal A0 wird dabei nicht veraendert.
 */
static constexpr uint8_t EC_FILTER_SAMPLES = 30;
float ecVoltageRing[EC_FILTER_SAMPLES] = {};
uint8_t ecRingIndex = 0, ecRingCount = 0;
unsigned long lastEcSampleMs = 0;

static void sampleTdsBackground(unsigned long nowMs) {
  if (!sensors.adsOk) return;
  if (lastEcSampleMs != 0 && nowMs - lastEcSampleMs < 40UL) return;
  lastEcSampleMs = nowMs;
  const float v = adsCountsToVolts(ads.readADC_SingleEnded(1));
  if (!isfinite(v) || v < 0.0f || v > 2.3f) return;
  ecVoltageRing[ecRingIndex] = v;
  ecRingIndex = (ecRingIndex + 1) % EC_FILTER_SAMPLES;
  if (ecRingCount < EC_FILTER_SAMPLES) ++ecRingCount;
}
static float readTdsMedianVoltage() {
  if (ecRingCount < EC_FILTER_SAMPLES) return NAN;
  float values[EC_FILTER_SAMPLES];
  for (uint8_t i = 0; i < EC_FILTER_SAMPLES; ++i) values[i] = ecVoltageRing[i];
  for (uint8_t i = 1; i < EC_FILTER_SAMPLES; ++i) {
    const float key = values[i];
    uint8_t j = i;
    while (j > 0 && values[j - 1] > key) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = key;
  }
  return (values[EC_FILTER_SAMPLES / 2 - 1] + values[EC_FILTER_SAMPLES / 2]) * 0.5f;
}

static void setJsonFloat(JsonObject obj, const char *key, float value) {
  if (!isfinite(value)) obj[key] = nullptr;
  else obj[key] = value;
}

static void setJsonFloat(JsonObject obj, const String &key, float value) {
  if (!isfinite(value)) obj[key] = nullptr;
  else obj[key] = value;
}

static bool localAuthorized() {
  if (strlen(TELEMETRY_API_KEY) == 0) return true;
  return localServer.hasHeader("X-Telemetry-Key") &&
         localServer.header("X-Telemetry-Key") == TELEMETRY_API_KEY;
}

static void sendUnauthorized() {
  localServer.send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
}

static void writeActuators(JsonObject obj, const ActuatorState &s) {
  obj["light"] = s.light;
  obj["heater"] = s.heater;
  obj["circulation_vent"] = s.circulation;
  obj["exhaust_vent"] = s.exhaust;
}

/* Sensor-ESP32 hat keine SSR-Ausgaenge. Keine GPIO-Initialisierung. */
static bool sameState(const ActuatorState &a, const ActuatorState &b) {
  return a.light == b.light && a.heater == b.heater &&
         a.circulation == b.circulation && a.exhaust == b.exhaust;
}

static void applyActuators() {
  // C3-Kommunikation ist noch nicht implementiert. Sollwerte nur speichern,
  // NIEMALS einen Pin des Sensor-ESP32 fuer einen Aktor schalten.
  if (!sensors.dht[0].ok && targets.heater) {
    targets.heater = false;
    ++commandSeq;
  }
  actual = ActuatorState{}; // kein bestaetigter Leistungscontroller
}

/* ---------- BH1750 ---------- */
// 1,00 x 1,00 m ist die physische Anbauflaeche, unabhaengig von der Blattgroesse.
// Eine Einzelpunkt-Luxmessung gibt nur einen groben Mittelwert-Schaetzer fuer
// einfallenden Lichtstrom auf dieser Flaeche, NICHT die Lumenleistung der Lampe.
static constexpr float GROW_LIGHT_AREA_M2 = 1.00f;
static float estimatedAreaLumens(float lux) {
  return isfinite(lux) && lux >= 0.0f ? lux * GROW_LIGHT_AREA_M2 : NAN;
}


// Feste Zuordnung wiederhergestellt: ADS1115 D21/D22, BH1750 D13/D14.
// Bus-Adressscan dient nur zur Diagnose und bindet Module nicht um.
static bool i2cResponds(TwoWire& bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

static void scanI2CAndBind(bool forceFullScan) {
  // Urspruengliche feste Zuordnung:
  // ADS1115 mit OLED auf Wire / GPIO21 SDA + GPIO22 SCL, ADDR=GND (0x48).
  // BH1750 getrennt auf bhWire / GPIO13 SDA + GPIO14 SCL.
  // I2C-Scan ist reine Diagnose und darf ADS/BH nicht auf andere Busse umbinden.
  ads.setGain(GAIN_ONE);
  sensors.adsOk = ads.begin(0x48, &Wire);
  adsFoundAddr = sensors.adsOk ? 0x48 : 0;
  adsFoundBus = sensors.adsOk ? "D21(SDA)/D22(SCL)" : "nicht gefunden";
  if (sensors.adsOk)
    Serial.println(F("ADS1115 GEFUNDEN: 0x48 auf D21/D22 (OLED-Bus)"));
  else
    Serial.println(F("ADS1115 NICHT GEFUNDEN: 0x48 auf D21/D22"));

  bh1750Present = false;
  bh1750Addr = 0;
  bhFoundBus = "nicht gefunden";
  bhSelectedWire = &bhWire;
  const uint8_t bhAddresses[] = {0x23, 0x5C};
  for (uint8_t addr : bhAddresses) {
    if (!i2cResponds(bhWire, addr)) continue;
    bh1750Present = true;
    bh1750Addr = addr;
    bhFoundBus = "D13(SDA)/D14(SCL)";
    break;
  }
  if (bh1750Present)
    Serial.printf("BH1750 GEFUNDEN: 0x%02X auf D13/D14\n", bh1750Addr);
  else
    Serial.println(F("BH1750 nicht gefunden: 0x23/0x5C auf D13/D14"));

  if (forceFullScan || !sensors.adsOk) {
    // Nur anzeigen, keine automatische Neuzuordnung der Sensoren.
    TwoWire* buses[] = {&Wire, &bhWire};
    const char* names[] = {"D21(SDA)/D22(SCL)", "D13(SDA)/D14(SCL)"};
    Serial.println(F("=== I2C-DIAGNOSE: ADDRESSEn AUF BEIDEN BUSSEN ==="));
    for (uint8_t b = 0; b < 2; ++b) {
      Serial.printf("I2C %s: ", names[b]);
      uint8_t count = 0;
      for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
        if (i2cResponds(*buses[b], addr)) {
          Serial.printf("0x%02X ", addr);
          ++count;
        }
      }
      if (!count) Serial.print("kein Geraet");
      Serial.println();
    }
  }
}

static uint32_t readBh1750() {
  if (!bh1750Present) return 0;
  bhSelectedWire->beginTransmission(bh1750Addr);
  bhSelectedWire->write(0x01);  // power on
  if (bhSelectedWire->endTransmission() != 0) return 0;
  delay(10);
  bhSelectedWire->beginTransmission(bh1750Addr);
  bhSelectedWire->write(0x10);  // continuously H-resolution mode
  if (bhSelectedWire->endTransmission() != 0) return 0;
  delay(180);
  if (bhSelectedWire->requestFrom(static_cast<int>(bh1750Addr), 2) != 2) return 0;
  uint16_t raw = (static_cast<uint16_t>(bhSelectedWire->read()) << 8) | bhSelectedWire->read();
  return static_cast<uint32_t>(raw / 1.2f);
}

/* MLX90640 32x24, I2C 0x33 auf bestehendem Bus GPIO21/22.
 * Bis zur Montage/Validierung NICHT fuer die automatische Regelung nutzen.
 * Die mittlere Region ist nur eine thermische ROI, keine Blatterkennung.
 */
static float saturatedVaporPressureKpa(float tempC) {
  return 0.61078f * expf(17.27f * tempC / (tempC + 237.3f));
}
static void updateLeafVpdCandidate() {
  sensors.vpdLeafCandidate = NAN;
  if (!sensors.mlx90640Ok || !isfinite(sensors.thermalRoiTempC)) return;
  float values[3];
  uint8_t n = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    if (!sensors.dht[i].ok) continue;
    const float actualVapor = saturatedVaporPressureKpa(sensors.dht[i].tempC)
                              * (sensors.dht[i].rh / 100.0f);
    values[n++] = saturatedVaporPressureKpa(sensors.thermalRoiTempC) - actualVapor;
  }
  if (n < 2) return; // fuer Referenz mind. zwei gueltige Luftsensoren
  for (uint8_t i = 1; i < n; ++i) {
    float v = values[i];
    uint8_t j = i;
    while (j > 0 && values[j - 1] > v) {
      values[j] = values[j - 1]; --j;
    }
    values[j] = v;
  }
  sensors.vpdLeafCandidate = n == 3 ? values[1] : 0.5f * (values[0] + values[1]);
}
#if GROW_ENABLE_MLX90640
static Adafruit_MLX90640 mlx90640;
static float mlxFrame[32 * 24];
static bool mlxInitialized = false;
static unsigned long lastMlxReadMs = 0;
static void setupMlx90640() {
  Wire.setClock(400000); // schnelle Bilduebertragung, ADS/OLED danach wieder 100 kHz
  mlxInitialized = mlx90640.begin(0x33, &Wire);
  if (mlxInitialized) {
    mlx90640.setMode(MLX90640_CHESS);
    mlx90640.setResolution(MLX90640_ADC_18BIT);
    mlx90640.setRefreshRate(MLX90640_2_HZ);
  }
  Wire.setClock(100000);
  Serial.printf("MLX90640 I2C 0x33: %s\n",
                mlxInitialized ? "OK (ROI noch nicht validiert)" : "NICHT ERKANNT");
  lastMlxReadMs = millis();
}
static void pollMlx90640(unsigned long nowMs) {
  if (!mlxInitialized || nowMs - lastMlxReadMs < 10000UL) return;
  lastMlxReadMs = nowMs;
  Wire.setClock(400000);
  const int error = mlx90640.getFrame(mlxFrame);
  Wire.setClock(100000);
  sensors.mlx90640Ok = false;
  sensors.thermalRoiTempC = NAN;
  if (error != 0) {
    updateLeafVpdCandidate();
    Serial.printf("MLX90640 Bildfehler: %d\n", error);
    return;
  }
  float roi[16 * 12];
  uint16_t n = 0;
  for (uint8_t y = 6; y < 18; ++y) {
    for (uint8_t x = 8; x < 24; ++x) {
      const float temp = mlxFrame[y * 32 + x];
      if (isfinite(temp) && temp >= 0.0f && temp <= 60.0f) roi[n++] = temp;
    }
  }
  if (n < 96) { updateLeafVpdCandidate(); return; }
  for (uint16_t i = 1; i < n; ++i) {
    const float temp = roi[i];
    uint16_t j = i;
    while (j > 0 && roi[j - 1] > temp) { roi[j] = roi[j - 1]; --j; }
    roi[j] = temp;
  }
  sensors.thermalRoiTempC = n & 1 ? roi[n / 2] :
                            0.5f * (roi[n / 2 - 1] + roi[n / 2]);
  sensors.mlx90640Ok = true;
  updateLeafVpdCandidate();
  Serial.printf("MLX90640 ROI %.2f C | VPD-Blatt-Kandidat %.3f kPa | %u Pixel\n",
      sensors.thermalRoiTempC, sensors.vpdLeafCandidate, static_cast<unsigned>(n));
}
#else
static void setupMlx90640() {}
static void pollMlx90640(unsigned long) {}
#endif

/* ---------- Sensor-Lesung ---------- */

static void readSensors() {
  sensors.dht[0] = readDht(dht1);
  sensors.dht[1] = readDht(dht2);
  sensors.dht[2] = readDht(dht3);

  sensors.vpd = sensors.dht[0].ok
      ? computeVpd(sensors.dht[0].tempC, sensors.dht[0].rh)
      : NAN;
  updateLeafVpdCandidate();

  // Sensor-Suche wiederholen, falls bei Programmstart noch kein
  // DS18B20 angeschlossen war (DallasTemperature cached die Geraetezahl).
  if (ds18b20.getDeviceCount() == 0) ds18b20.begin();
  ds18b20.requestTemperatures();
  const uint8_t foundDs = ds18b20.getDeviceCount();
  sensors.dsCount = foundDs > 2 ? 2 : foundDs;
  sensors.ds1C = sensors.dsCount >= 1 ? ds18b20.getTempCByIndex(0) : NAN;
  sensors.ds2C = sensors.dsCount >= 2 ? ds18b20.getTempCByIndex(1) : NAN;
  if (!validDsTempC(sensors.ds1C)) sensors.ds1C = NAN;
  if (!validDsTempC(sensors.ds2C)) sensors.ds2C = NAN;
  // Bei nur EINEM Fühler dessen Wert verwenden, selbst wenn die
  // NVS noch die ROM-Adresse eines frueheren DS18B20 gespeichert hat.
  if (sensors.dsCount == 1) {
    sensors.waterTempC = sensors.ds1C;
  } else if (sensors.dsCount > 1 && waterSensorRomSelected) {
    const float selected = ds18b20.getTempC(waterSensorRom);
    sensors.waterTempC = validDsTempC(selected) ? selected : NAN;
  } else {
    sensors.waterTempC = NAN;
  }

  if (sensors.adsOk) {
    const int16_t rawPh = ads.readADC_SingleEnded(0);
    const float sampledTdsV = readTdsMedianVoltage();
    const int16_t rawTo = PH_TO_WIRED ? ads.readADC_SingleEnded(2) : 0;
    sensors.phInputV = adsCountsToVolts(rawPh);
    sensors.tdsInputV = sampledTdsV;
    sensors.phToInputV = PH_TO_WIRED ? adsCountsToVolts(rawTo) : NAN;
    sensors.caseTempC = toCaseTemp(sensors.phToInputV);
    sensors.phTempCompensated = phTempCompAvailable(sensors.waterTempC);
    sensors.ph = phFromVoltage(sensors.phInputV, sensors.waterTempC);
    // Ohne DS18B20 keine 25-C-Annahme und kein als kompensiert bezeichneter EC-Wert.
    sensors.ec = cqrobotEcMs(sensors.tdsInputV, sensors.waterTempC);
    sensors.tdsPpm = isfinite(sensors.ec) ? sensors.ec * 500.0f : NAN;
  } else {
    sensors.phInputV = NAN;
    sensors.phToInputV = NAN;
    sensors.caseTempC = NAN;
    sensors.phTempCompensated = false;
    sensors.tdsInputV = NAN;
    sensors.ph = NAN;
    sensors.tdsPpm = NAN;
    sensors.ec = NAN;
  }

  updatePhAlarm();
  sensors.lux = readBh1750();
}

/* ---------- Steuerung ---------- */

static void autoControl() {
  if (strcmp(controlMode, "auto") != 0) return;

  /* Licht wird spaeter durch Zeitplan/Serverziel ergaenzt und hier nicht veraendert. */

  if (!sensors.dht[0].ok) {
    // Waehrend der DHT22 noch nicht verkabelt oder ausgefallen ist,
    // aktiviert AUTO keinerlei SSR. Keine automatisch laufenden
    // Ventilatoren oder Heizung bei ungueltiger Temperatur/Feuchte.
    const ActuatorState before = targets;
    targets = ActuatorState{};
    if (!sameState(before, targets)) ++commandSeq;
    return;
  }

  const PhaseProfile &profile = phaseProfile();
  if (!profile.configured) {
    // Fehlende Bluete-Werte duerfen nicht still durch Wuchs-Werte ersetzt werden.
    const ActuatorState before = targets;
    targets = ActuatorState{};
    if (!sameState(before, targets)) ++commandSeq;
    return;
  }
  const float t = sensors.dht[0].tempC;
  const float rh = sensors.dht[0].rh;
  ActuatorState before = targets;

  if (t < profile.targetAirC - TEMP_HYST) targets.heater = true;
  else if (t > profile.targetAirC + TEMP_HYST) targets.heater = false;

  if (rh > profile.targetHumidityPct + HUM_HYST) targets.circulation = true;
  else if (rh < profile.targetHumidityPct - HUM_HYST) targets.circulation = false;

  if (t > profile.targetAirC + 2.0f) targets.exhaust = true;
  else if (t < profile.targetAirC + 0.5f) targets.exhaust = false;

  if (!sameState(before, targets)) ++commandSeq;
}

/* ---------- Lokale HTTP-API ---------- */

static void handleSummary() {
  if (!localAuthorized()) return sendUnauthorized();

  DynamicJsonDocument doc(3072);
  doc["ok"] = true;

  JsonObject control = doc.createNestedObject("control");
  control["control_mode"] = controlMode;
  control["phase"] = phaseName(growPhase);
  writeNutrientTargets(control);
  control["phase_profile_configured"] = phaseProfile().configured;
  if (phaseProfile().configured) {
    control["phase_air_target_c"] = phaseProfile().targetAirC;
    control["phase_humidity_target_pct"] = phaseProfile().targetHumidityPct;
  } else {
    control["phase_air_target_c"] = nullptr;
    control["phase_humidity_target_pct"] = nullptr;
  }
  JsonObject targetObj = control.createNestedObject("targets");
  writeActuators(targetObj, targets);

  JsonObject reading = doc.createNestedObject("reading");
  writeNutrientTargets(reading);
  if (sensors.dht[0].ok) {
    reading["air_temperature"] = sensors.dht[0].tempC;
    reading["humidity"] = sensors.dht[0].rh;
  } else {
    reading["air_temperature"] = nullptr;
    reading["humidity"] = nullptr;
  }
  setJsonFloat(reading, "vpd", sensors.vpd);
  reading["mlx90640_ok"] = sensors.mlx90640Ok;
  setJsonFloat(reading, "thermal_roi_temperature", sensors.thermalRoiTempC);
  setJsonFloat(reading, "vpd_leaf_candidate", sensors.vpdLeafCandidate);
  setJsonFloat(reading, "ph", sensors.ph);
  reading["ph_temperature_compensated"] = sensors.phTempCompensated;
  setJsonFloat(reading, "case_temperature", sensors.caseTempC);
  setJsonFloat(reading, "ph_voltage", sensors.phInputV);
  setJsonFloat(reading, "ph_to_voltage", sensors.phToInputV);
  if (PH_DO_WIRED) reading["ph_do_high"] = sensors.phDoHigh;
  else reading["ph_do_high"] = nullptr;
  reading["ph_calibrated"] = phCalibrated;
  reading["ph_alarm"] = phCode(phAlarm);
  reading["ph_alarm_active"] = phAlarm != PhAlarm::Normal;
  reading["ph_buzzer_configured"] = (GROW_ENABLE_PH_BUZZER != 0);
  reading["ph_alarm_test"] = phTestActive;
  if (phTestActive) {
    reading["ph_alarm_test_status"] = phCode(phTestAlarm);
    reading["ph_alarm_test_value"] = phTestValue;
  }
  setJsonFloat(reading, "ec", sensors.ec); // mS/cm bei aktueller Wassertemperatur
  setJsonFloat(reading, "ec_measurement_temperature_c", sensors.waterTempC);
  reading["ec_calibrated"] = ecCalibrated;
  reading["ec_reference_temp_verified"] = validDsTempC(ecCalTempC);
  setJsonFloat(reading, "ec_reference_voltage", ecReferenceInputV);
  setJsonFloat(reading, "tds_ppm", sensors.tdsPpm);
  setJsonFloat(reading, "tds_voltage", sensors.tdsInputV);
  reading["light_level"] = sensors.lux; // lx, Bestands-API unveraendert
  if (bh1750Present) {
    reading["illuminance_lux"] = sensors.lux;
    reading["estimated_incident_lumens"] = estimatedAreaLumens(static_cast<float>(sensors.lux));
  } else {
    reading["illuminance_lux"] = nullptr;
    reading["estimated_incident_lumens"] = nullptr;
  }
  reading["light_area_m2"] = GROW_LIGHT_AREA_M2;
  reading["lumens_estimated"] = true;
  setJsonFloat(reading, "ds18b20_1_temperature", sensors.ds1C);
  setJsonFloat(reading, "ds18b20_2_temperature", sensors.ds2C);
  setJsonFloat(reading, "water_temperature", sensors.waterTempC);
  reading["water_temperature_source"] = validDsTempC(sensors.waterTempC) ? "ds18b20" : "unassigned";

  for (int i = 0; i < 3; ++i) {
    String prefix = String("dht") + String(i + 1) + "_";
    setJsonFloat(reading, prefix + "temperature", sensors.dht[i].tempC);
    setJsonFloat(reading, prefix + "humidity", sensors.dht[i].rh);
  }

  JsonObject outputs = doc.createNestedObject("actuators");
  outputs["source"] = "esp32_c3_not_connected";
  outputs["connected"] = false;
  // Bestaetigung der physischen Abschaltung ist noch nicht verfuegbar.
  outputs["pump_service_off_confirmed"] = false;
  JsonObject actualObj = outputs.createNestedObject("actual");
  writeActuators(actualObj, actual);

  String body;
  serializeJson(doc, body);
  localServer.send(200, "application/json", body);
}

static void handleActuatorTargets() {
  if (!localAuthorized()) return sendUnauthorized();

  StaticJsonDocument<512> doc;
  doc["ok"] = true;
  doc["power_controller_connected"] = false;
  doc["control_mode"] = controlMode;
  doc["phase"] = phaseName(growPhase);
  writeNutrientTargets(doc.as<JsonObject>());
  doc["phase_profile_configured"] = phaseProfile().configured;
  doc["seq"] = commandSeq;
  doc["source_ms"] = millis();
  JsonObject commands = doc.createNestedObject("commands");
  writeActuators(commands, targets);
  JsonObject active = doc.createNestedObject("actual");
  writeActuators(active, actual);

  String body;
  serializeJson(doc, body);
  localServer.send(200, "application/json", body);
}

static void handleControlTargets() {
  if (!localAuthorized()) return sendUnauthorized();
  // Aktorbefehle nicht als angewendet ausgeben, solange C3 fehlt.
  localServer.send(503, "application/json",
                   "{\"ok\":false,\"error\":\"esp32_c3_not_connected\"}");
}

static void handleControlMode() {
  if (!localAuthorized()) return sendUnauthorized();

  StaticJsonDocument<128> in;
  if (deserializeJson(in, localServer.arg("plain"))) {
    return localServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_json\"}");
  }

  const char *mode = in["mode"];
  if (!mode || (strcmp(mode, "auto") != 0 && strcmp(mode, "manual") != 0)) {
    return localServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_mode\"}");
  }

  strncpy(controlMode, mode, sizeof(controlMode) - 1);
  controlMode[sizeof(controlMode) - 1] = '\0';
  ++commandSeq;
  autoControl();
  applyActuators();
  localServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleGrowWeek() {
  if (!localAuthorized()) return sendUnauthorized();
  StaticJsonDocument<128> input;
  if (deserializeJson(input, localServer.arg("plain"))) {
    return localServer.send(400, "application/json",
        "{\"ok\":false,\"error\":\"bad_json\"}");
  }
  const int selected = input["week"].as<int>();
  if (selected < 1 || selected > 12) {
    return localServer.send(400, "application/json",
        "{\"ok\":false,\"error\":\"week_must_be_1_to_12\"}");
  }
  if (!setGrowWeek(selected)) {
    return localServer.send(500, "application/json",
        "{\"ok\":false,\"error\":\"week_nvs_not_saved\"}");
  }
  StaticJsonDocument<192> doc;
  doc["ok"] = true;
  JsonObject payload = doc.to<JsonObject>();
  payload["ok"] = true;
  writeNutrientTargets(payload);
  String body;
  serializeJson(doc, body);
  localServer.send(200, "application/json", body);
}
static void handleGrowPhase() {
  if (!localAuthorized()) return sendUnauthorized();
  StaticJsonDocument<128> input;
  if (deserializeJson(input, localServer.arg("plain"))) {
    return localServer.send(400, "application/json",
        "{\"ok\":false,\"error\":\"bad_json\"}");
  }
  const char *requested = input["phase"];
  if (!requested ||
      (strcmp(requested, "wuchs") != 0 && strcmp(requested, "bluete") != 0)) {
    return localServer.send(400, "application/json",
        "{\"ok\":false,\"error\":\"phase_must_be_wuchs_or_bluete\"}");
  }
  const GrowPhase next = strcmp(requested, "bluete") == 0
      ? GrowPhase::Bluete : GrowPhase::Wuchs;
  // Unveraenderte, bereits vom D19-Taster verwendete Phasenfunktion.
  setGrowPhase(next);
  // Nur dann Erfolg melden, wenn der gewaehlte Zustand auch in NVS steht.
  Preferences prefs;
  if (!prefs.begin(PHASE_NVS, true)) {
    return localServer.send(500, "application/json",
        "{\"ok\":false,\"error\":\"phase_nvs_unavailable\"}");
  }
  const uint8_t stored = prefs.getUChar("phase", 255);
  prefs.end();
  if (stored != static_cast<uint8_t>(next)) {
    return localServer.send(500, "application/json",
        "{\"ok\":false,\"error\":\"phase_nvs_not_saved\"}");
  }
  StaticJsonDocument<128> out;
  out["ok"] = true;
  out["phase"] = phaseName(growPhase);
  String response;
  serializeJson(out, response);
  localServer.send(200, "application/json", response);
}

static void setupLocalApi() {
  localServer.collectHeaders(LOCAL_HEADER_KEYS, 1);
  localServer.on("/api/local/summary", HTTP_GET, handleSummary);
  localServer.on("/api/local/actuators", HTTP_GET, handleActuatorTargets);
  localServer.on("/api/local/control/targets", HTTP_POST, handleControlTargets);
  localServer.on("/api/local/control/mode", HTTP_POST, handleControlMode);
  localServer.on("/api/local/control/week", HTTP_POST, handleGrowWeek);
  localServer.on("/api/local/control/phase", HTTP_POST, handleGrowPhase);
  localServer.begin();
}

/* ---------- Telemetrie zum Homeserver ---------- */

static bool sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) return false;

  DynamicJsonDocument doc(4096);
  JsonObject root = doc.to<JsonObject>();

  setJsonFloat(root, "air_temperature", sensors.dht[0].tempC);
  setJsonFloat(root, "humidity", sensors.dht[0].rh);
  setJsonFloat(root, "water_temperature", sensors.waterTempC);
  setJsonFloat(root, "case_temperature", sensors.caseTempC);
  root["water_temperature_source"] = validDsTempC(sensors.waterTempC) ? "ds18b20" : "unassigned";
  root["phase"] = phaseName(growPhase);
  writeNutrientTargets(root);
  root["phase_profile_configured"] = phaseProfile().configured;
  root["ph_temperature_compensated"] = sensors.phTempCompensated;
  root["light_level"] = sensors.lux; // lx, Bestandstelemetrie unveraendert
  if (bh1750Present) {
    root["illuminance_lux"] = sensors.lux;
    root["estimated_incident_lumens"] = estimatedAreaLumens(static_cast<float>(sensors.lux));
  } else {
    root["illuminance_lux"] = nullptr;
    root["estimated_incident_lumens"] = nullptr;
  }
  root["light_area_m2"] = GROW_LIGHT_AREA_M2;
  root["lumens_estimated"] = true;
  setJsonFloat(root, "ec", sensors.ec); // mS/cm bei aktueller Wassertemperatur
  setJsonFloat(root, "ec_measurement_temperature_c", sensors.waterTempC);
  root["ec_calibrated"] = ecCalibrated;
  root["ec_reference_temp_verified"] = validDsTempC(ecCalTempC);
  setJsonFloat(root, "ec_reference_voltage", ecReferenceInputV);
  setJsonFloat(root, "ph", sensors.ph);
  setJsonFloat(root, "ph_to_voltage", sensors.phToInputV);
  if (PH_DO_WIRED) root["ph_do_high"] = sensors.phDoHigh;
  else root["ph_do_high"] = nullptr;
  root["ph_calibrated"] = phCalibrated;
  root["ph_alarm"] = phCode(phAlarm);
  root["ph_alarm_active"] = phAlarm != PhAlarm::Normal;
  root["ph_buzzer_configured"] = (GROW_ENABLE_PH_BUZZER != 0);
  root["ph_alarm_test"] = phTestActive;
  if (phTestActive) {
    root["ph_alarm_test_status"] = phCode(phTestAlarm);
    root["ph_alarm_test_value"] = phTestValue;
  }

  setJsonFloat(root, "vpd", sensors.vpd);
  root["mlx90640_ok"] = sensors.mlx90640Ok;
  setJsonFloat(root, "thermal_roi_temperature", sensors.thermalRoiTempC);
  setJsonFloat(root, "vpd_leaf_candidate", sensors.vpdLeafCandidate);
  setJsonFloat(root, "ph_voltage", sensors.phInputV);
  setJsonFloat(root, "tds_voltage", sensors.tdsInputV);
  setJsonFloat(root, "tds_ppm", sensors.tdsPpm);
  setJsonFloat(root, "ds18b20_1_temperature", sensors.ds1C);
  setJsonFloat(root, "ds18b20_2_temperature", sensors.ds2C);

  for (int i = 0; i < 3; ++i) {
    String prefix = String("dht") + String(i + 1) + "_";
    setJsonFloat(root, prefix + "temperature", sensors.dht[i].tempC);
    setJsonFloat(root, prefix + "humidity", sensors.dht[i].rh);
  }

  JsonObject relays = root.createNestedObject("relays");
  writeActuators(relays, actual);

  JsonObject dev = root.createNestedObject("device_info");
  dev["device_id"] = "grow_sensor_01";
  dev["firmware_version"] = "5.1.0-sensor-esp";
  dev["phase"] = phaseName(growPhase);
  dev["grow_week"] = growWeek;
  dev["control_mode"] = controlMode;
  dev["ads1115"] = sensors.adsOk;
  dev["bh1750"] = bh1750Present;
  dev["bh1750_address"] = bh1750Addr;
  dev["ds18b20_count"] = sensors.dsCount;
  dev["ph_do_configured"] = PH_DO_WIRED;
  dev["ph_to_configured"] = PH_TO_WIRED;
  dev["ph_to_type"] = "raw_voltage_uncalibrated";
  dev["ec_reference_us_cm"] = 1413;
  dev["ec_temp_compensation"] = "ds18b20_live_temperature_or_null";
  dev["water_ds_rom_selected"] = waterSensorRomSelected;
  dev["to_case_calibrated"] = toCalibrated;
  dev["actuator_controller"] = "esp32_c3_not_connected";

  String body;
  serializeJson(doc, body);

  HTTPClient http;
  http.setTimeout(5000);
  if (!growHttpBegin(http, "/api/v1/telemetry")) return false;
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  const int code = http.POST(body);
  http.end();
  return code == 200;
}

static void sendPhAlertEvent() {
  if (!phAlertPending || WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(5000);
  if (!growHttpBegin(http, "/api/v1/alerts")) return;
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  StaticJsonDocument<256> doc;
  doc["severity"] = phAlarm == PhAlarm::Normal ? "info" : "warn";
  doc["source"] = "grow_sensor_01";
  doc["message"] = phMessage(phAlarm);
  String body;
  serializeJson(doc, body);
  const int response = http.POST(body);
  http.end();
  if (response == 200) phAlertPending = false;
}

/* Der TEST-Alarm wird getrennt von echten Alarmen mit "TEST" an den Server
 * gesendet. Er veraendert niemals den realen ph_alarm-/ph-Messwert.
 */
static void sendPhTestAlertEvent() {
  if (!phTestActive || !phTestAlertPending || WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(5000);
  if (!growHttpBegin(http, "/api/v1/alerts")) return;
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  StaticJsonDocument<256> doc;
  doc["severity"] = "info";
  doc["source"] = "grow_sensor_01";
  doc["message"] = String("[TEST, SIMULIERT] ") + phMessage(phTestAlarm);
  String body;
  serializeJson(doc, body);
  const int response = http.POST(body);
  http.end();
  if (response == 200) {
    phTestAlertPending = false;
    Serial.println(F("[pH TEST] Server hat das gekennzeichnete Test-Event angenommen."));
  } else {
    Serial.printf("[pH TEST] Alarm-HTTP %d (nicht bestaetigt).\n", response);
  }
}

/* ---------- OLED ---------- */

static void drawOled() {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(growPhase == GrowPhase::Wuchs ? F("WUCHS") : F("BLUETE"));
  if (growWeek) display.printf(" W%u", growWeek);
  else display.print(F(" W?"));
  if (!phaseProfile().configured) display.print(F(" !"));

  display.setCursor(0, 12);
  if (sensors.dht[0].ok) {
    display.printf("T %.1fC RH %.0f%%", sensors.dht[0].tempC, sensors.dht[0].rh);
  } else {
    display.print("DHT1 FEHLER");
  }

  display.setCursor(0, 24);
  // Zur Platzersparnis alle 5 s abwechselnd Temperatur oder Licht zeigen.
  // pH/EC und die Alarmzeile bleiben zu jeder Zeit sichtbar.
  if ((millis() / 5000UL) % 2UL == 1UL) {
    if (bh1750Present) {
      display.printf("%lu lx ~%.0f lm",
                     static_cast<unsigned long>(sensors.lux),
                     estimatedAreaLumens(static_cast<float>(sensors.lux)));
    } else display.print(F("BH1750 --"));
  } else {
    display.print("Box ");
    if (isfinite(sensors.caseTempC)) display.printf("%.1fC", sensors.caseTempC);
    else display.print("--");
    display.print(" H2O ");
    if (isfinite(sensors.waterTempC)) display.printf("%.1fC", sensors.waterTempC);
    else display.print("--");
  }

  display.setCursor(0, 36);
  display.print("pH ");
  if (isnan(sensors.ph)) display.print("--");
  else display.printf("%.2f", sensors.ph);
  display.print(" EC ");
  if (!isfinite(sensors.ec)) display.print("--");
  else display.printf("%.2f", sensors.ec);

  display.setCursor(0, 50);
  const PhAlarm shown = phTestActive ? phTestAlarm : phAlarm;
  if (phTestActive) display.print("TEST ");
  if (shown == PhAlarm::Low) display.print("ALARM pH <5.5");
  else if (shown == PhAlarm::High) display.print("ALARM pH >6.2");
  else if (shown == PhAlarm::Fault) display.print("pH SENSORFEHLER");
  else display.print("pH OK 5.5-6.2");

  display.display();
}

/* ---------- Setup / Loop ---------- */

void setup() {
  // Kein SSR-Pin auf dem Sensor-ESP32, Aktoren werden hier nicht geschaltet.
  actual = ActuatorState{};
  Serial.begin(115200);
  delay(300);
  setupGrowPhaseButtons();
  loadGrowWeek();
  setupPhBuzzer();
  Serial.println(F("pH-Test: 1=LOW 5.40 | 2=HIGH 6.30 | 3=NORMAL 6.00 | 0=Test aus"));
  loadPhCalibration();
  loadEcCalibration();
  loadWaterSensorRom();
  loadToCalibration();
  if (!ecCalibrated) Serial.println("EC unkalibriert: 'e' im Diagnose-Sketch verwenden.");
  if (!phCalibrated) Serial.println("WARNUNG: pH-Kalibrierung fehlt.");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  Wire.setTimeOut(30);
  bhWire.begin(PIN_BH_SDA, PIN_BH_SCL, 50000);
  bhWire.setTimeOut(30);

  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 24);
    display.print("Grow Sensor V4");
    display.display();
  }

  scanI2CAndBind();
  setupMlx90640();

  dht1.begin();
  dht2.begin();
  dht3.begin();
  ds18b20.begin();

  Serial.printf("ADS1115: %s | 0x%02X auf %s\n", sensors.adsOk ? "OK" : "NICHT GEFUNDEN", adsFoundAddr, adsFoundBus);
  Serial.printf("BH1750: %s | 0x%02X auf %s\n", bh1750Present ? "OK" : "NICHT GEFUNDEN", bh1750Addr, bhFoundBus);
  Serial.printf("DS18B20 auf D23 gefunden: %u\n", ds18b20.getDeviceCount());

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin("grow-sensor")) {
      MDNS.addService("http", "tcp", LOCAL_HTTP_PORT);
    }
  }

  readSensors();
  lastSensorRead = millis();
  autoControl();
  applyActuators();
  setupLocalApi();
}

void loop() {
  const unsigned long now = millis();
  static unsigned long lastI2cRetryMs = millis();
  if (!sensors.adsOk && now - lastI2cRetryMs >= 30000UL) {
    lastI2cRetryMs = now;
    scanI2CAndBind();
  }

  localServer.handleClient();
  pollGrowPhaseButtons(now);
  handlePhTestSerial();
  updatePhBuzzer(now);
  sampleTdsBackground(now);
  pollMlx90640(now);

  if (WiFi.status() != WL_CONNECTED && now - lastWifiReconnect >= WIFI_RECONNECT_MS) {
    lastWifiReconnect = now;
    WiFi.reconnect();
  }

  if (now - lastSensorRead >= SENSOR_READ_MS) {
    lastSensorRead = now;
    readSensors();
    if (phTestActive) updatePhTestAlarm();
  }

  autoControl();
  applyActuators();

  if (now - lastOledRefresh >= OLED_REFRESH_MS) {
    lastOledRefresh = now;
    drawOled();
  }

  if (now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = now;
    sendTelemetry();
  }
  if (phAlertPending && WiFi.status() == WL_CONNECTED &&
      now - lastPhAlertAttempt >= 10000UL) {
    lastPhAlertAttempt = now;
    sendPhAlertEvent();
  }
  if (phTestAlertPending && phTestActive && WiFi.status() == WL_CONNECTED &&
      now - lastPhTestAlertAttempt >= 10000UL) {
    lastPhTestAlertAttempt = now;
    sendPhTestAlertEvent();
  }

  delay(2);
}
