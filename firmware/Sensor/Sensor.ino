/**
 * Grow Controller: ESP32-Pro-Sensorcontroller ohne lokales Display.
 *
 * - 3x DHT22, DS18B20 (D23), ADS1115 (D21/D22), BH1750 (D13/D14)
 * - Kamera MLX90640 (optional) am Sensor-I2C D21/D22 (0x33)
 * - HTTP-API fuer CYD/Raspberry Pi, Telemetrie und pH-Alarm
 * - Zweites Geraet: ESP32-C3 SuperMini als eigenstaendiger Leistungscontroller.
 *
 * Sensorplatine: separate Sensorplatine; dieser Sketch bedient KEINE Relaispins.
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
#include <time.h>
#include "phase_clock_math.h"
#include "air_fusion.h"
#include "calibration_capture.h"
#include <Wire.h>
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
// Kameratemperatur nur nach Blatt-ROI-Kalibrierung als VPD-Quelle freigeben.
#ifndef GROW_MLX_LEAF_VERIFIED
#define GROW_MLX_LEAF_VERIFIED 0
#endif
static constexpr float VPD_LEAF_AIR_OFFSET_C = 2.0f; // Nutzer-VPD-Tabelle

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
 * BLAU zeigt die vom C3 gemeldete Pumpen-Wartungssperre an.
 * Die Rueckmeldung bestaetigt den Firmwarezustand, KEINE elektrische Isolation.
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
#error "device_secrets.h missing; run python export_firmware_secrets_from_env.py from the repository root"
#endif

#include "grow_http_client.h"

/* ---------- Pins ---------- */
static constexpr uint8_t PIN_DHT1 = 25;
static constexpr uint8_t PIN_DHT2 = 26;
static constexpr uint8_t PIN_DHT3 = 27;
static constexpr uint8_t PIN_PHASE_BUTTON = 16; // Phase laut letzter Hardwarezuordnung
static constexpr uint8_t PIN_PUMP_SERVICE_BUTTON = 19; // softwareseitige Pumpen-Wartungssperre
static constexpr uint8_t PIN_RGB_GREEN = 4; // Gruen an D4; GPIO32 liefert kein Signal
static constexpr uint8_t PIN_RGB_RED = 33;
static constexpr uint8_t PIN_RGB_BLUE = 17; // TX2: Pumpen-Filterwechsel, nur bei bestaetigtem Pumpen-AUS
static constexpr uint8_t PIN_PH_BUZZER = 5; // Buzzer an D5; deaktiviert bis Typ/Treiber geklaert
static constexpr unsigned long PHASE_DEBOUNCE_MS = 50;
static constexpr uint8_t PIN_ONEWIRE = 23; // DS18B20 auf D23; PH4502C DO wird nicht genutzt
// A02YYUW on UART1: Sensor-TX -> GPIO18 (ESP32 RX), Sensor-RX offen.
// Bei 3.3-V-Versorgung liegen UART-Ausgangspegel im ESP32-Bereich.
static constexpr uint8_t PIN_LEVEL_UART_RX = 18;
static constexpr uint32_t LEVEL_UART_BAUD = 9600;
static constexpr uint16_t LEVEL_MIN_DISTANCE_MM = 30;
static constexpr uint16_t LEVEL_MAX_DISTANCE_MM = 4500;
static constexpr unsigned long LEVEL_TIMEOUT_MS = 3000UL;
// Erst nach verifizierter TO-Ausgangsspannung <= ADS-VDD (3.3 V) einschalten.
static constexpr bool PH_TO_WIRED = false;
static constexpr bool PH_DO_WIRED = false; // PH4502C DO bleibt ungenutzt

static constexpr int PIN_I2C_SDA = 21;
static constexpr int PIN_I2C_SCL = 22;
static constexpr int PIN_BH_SDA = 13;
static constexpr int PIN_BH_SCL = 14;

/* ---------- Intervale ---------- */
static constexpr unsigned long SENSOR_READ_MS = 2000;
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

/* ---------- I2C: ADS1115 und optionale MLX90640 auf GPIO21/22 ---------- */
Adafruit_ADS1115 ads;

/* ADS1115 auf Wire D21/D22, BH1750 separat auf zweitem I2C-Controller D13/D14 */
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
  AirReading air; // fusionierte Lufttemperatur / Feuchte fuer Regelung und VPD
  uint8_t airValidSensorCount = 0;
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

  float vpd = NAN; // aktiver Blatt-VPD in kPa (standardmaessig T Blatt = T Luft - 2 C)
  float vpdAir = NAN; // separater Luft-VPD, Blatt-Offset 0
  float leafTemperatureC = NAN; // geschaetzte oder verifizierte Blatt-Temperatur
  bool vpdFromCamera = false;
  float thermalRoiTempC = NAN; // mittlere Bildregion, NOCH NICHT als Blatt validiert
  float vpdLeafCandidate = NAN; // vorlaeufiger Blatt-VPD: ROI + fusionierte DHT22-Werte
  bool mlx90640Ok = false;
  uint32_t lux = 0;
  float levelDistanceMm = NAN; // Distanz, keine Liter ohne Kalibrierung

  bool adsOk = false;
} sensors;

/* ---------- A02YYUW: UART-Distanz, ohne automatische Dosierung ---------- */
HardwareSerial levelUart(1); // UART0 bleibt USB-Serial
static uint8_t levelFrame[4] = {};
static uint8_t levelFramePos = 0;
static unsigned long lastValidLevelMs = 0;
static constexpr uint8_t LEVEL_MEDIAN_COUNT = 5;
static uint16_t recentLevelMm[LEVEL_MEDIAN_COUNT] = {};
static uint8_t recentLevelCount = 0;
static uint8_t recentLevelIndex = 0;

static bool levelSensorOk() {
  return lastValidLevelMs != 0 &&
         millis() - lastValidLevelMs <= LEVEL_TIMEOUT_MS;
}
static void pollLevelSensor() {
  // Nicht blockierender 4-Byte-Rahmen: FF, High, Low, Checksum.
  while (levelUart.available()) {
    const uint8_t b = static_cast<uint8_t>(levelUart.read());
    if (levelFramePos == 0 && b != 0xFF) continue;
    levelFrame[levelFramePos++] = b;
    if (levelFramePos < 4) continue;
    levelFramePos = 0;
    const uint8_t sum = static_cast<uint8_t>(
        levelFrame[0] + levelFrame[1] + levelFrame[2]);
    if (sum != levelFrame[3]) continue;
    const uint16_t mm = (static_cast<uint16_t>(levelFrame[1]) << 8) |
                        levelFrame[2];
    if (mm < LEVEL_MIN_DISTANCE_MM || mm > LEVEL_MAX_DISTANCE_MM) continue;
    recentLevelMm[recentLevelIndex] = mm;
    recentLevelIndex = (recentLevelIndex + 1) % LEVEL_MEDIAN_COUNT;
    if (recentLevelCount < LEVEL_MEDIAN_COUNT) ++recentLevelCount;
    uint16_t sorted[LEVEL_MEDIAN_COUNT];
    for (uint8_t i = 0; i < recentLevelCount; ++i)
      sorted[i] = recentLevelMm[i];
    for (uint8_t i = 1; i < recentLevelCount; ++i) {
      const uint16_t key = sorted[i];
      int j = static_cast<int>(i) - 1;
      while (j >= 0 && sorted[j] > key) {
        sorted[j + 1] = sorted[j];
        --j;
      }
      sorted[j + 1] = key;
    }
    sensors.levelDistanceMm = sorted[recentLevelCount / 2];
    lastValidLevelMs = millis();
  }
  if (!levelSensorOk()) sensors.levelDistanceMm = NAN;
}
// Bis zu acht per Hand erfasste Volumenpunkte: groesserer Abstand = weniger Liter.
// Absichtlich getrennt von pH-/EC-NVS; kein Default-Tankvolumen wird geraten.
static constexpr uint8_t LEVEL_CAL_MAX = 8;
static constexpr char LEVEL_CAL_NVS[] = "levelcal_v1";
struct LevelPoint {
  uint16_t distanceMm;
  float liters;
};
static LevelPoint levelPoints[LEVEL_CAL_MAX] = {};
static uint8_t levelPointCount = 0;
static void loadLevelCalibration() {
  Preferences prefs;
  if (!prefs.begin(LEVEL_CAL_NVS, true)) return;
  uint8_t count = prefs.getUChar("count", 0);
  if (count >= 2 && count <= LEVEL_CAL_MAX &&
      prefs.getBytesLength("points") == count * sizeof(LevelPoint)) {
    LevelPoint candidate[LEVEL_CAL_MAX] = {};
    if (prefs.getBytes("points", candidate, count * sizeof(LevelPoint)) ==
        count * sizeof(LevelPoint)) {
      bool valid = true;
      for (uint8_t i = 0; i < count; ++i) {
        if (candidate[i].distanceMm < LEVEL_MIN_DISTANCE_MM ||
            candidate[i].distanceMm > LEVEL_MAX_DISTANCE_MM ||
            !isfinite(candidate[i].liters) || candidate[i].liters < 0.0f ||
            candidate[i].liters > 90.0f ||
            (i > 0 && (candidate[i - 1].distanceMm <= candidate[i].distanceMm ||
                       candidate[i - 1].liters >= candidate[i].liters)))
          valid = false;
      }
      if (valid) {
        memcpy(levelPoints, candidate, count * sizeof(LevelPoint));
        levelPointCount = count;
      }
    }
  }
  prefs.end();
}
static bool saveLevelCalibration() {
  Preferences prefs;
  if (!prefs.begin(LEVEL_CAL_NVS, false)) return false;
  const size_t bytes = levelPointCount * sizeof(LevelPoint);
  const bool ok = prefs.putBytes("points", levelPoints, bytes) == bytes &&
                  prefs.putUChar("count", levelPointCount) == sizeof(uint8_t);
  prefs.end();
  return ok;
}
static float calibratedLevelLiters() {
  if (!levelSensorOk() || levelPointCount < 2) return NAN;
  const float d = sensors.levelDistanceMm;
  if (d > levelPoints[0].distanceMm ||
      d < levelPoints[levelPointCount - 1].distanceMm) return NAN;
  for (uint8_t i = 1; i < levelPointCount; ++i) {
    if (d >= levelPoints[i].distanceMm) {
      const float lowD = levelPoints[i - 1].distanceMm;
      const float highD = levelPoints[i].distanceMm;
      const float t = (lowD - d) / (lowD - highD);
      return levelPoints[i - 1].liters +
             t * (levelPoints[i].liters - levelPoints[i - 1].liters);
    }
  }
  return NAN;
}
static void writeLevelMeasurement(JsonObject obj) {
  obj["level_sensor"] = "a02yyuw";
  obj["level_sensor_ok"] = levelSensorOk();
  obj["level_calibrated"] = levelPointCount >= 2;
  obj["level_calibration_points"] = levelPointCount;
  if (levelSensorOk()) obj["level_distance_mm"] = sensors.levelDistanceMm;
  else obj["level_distance_mm"] = nullptr;
  const float liters = calibratedLevelLiters();
  if (isfinite(liters)) {
    obj["level_liters"] = liters;
    obj["level_percent"] = 100.0f * liters /
                           levelPoints[levelPointCount - 1].liters;
  } else {
    obj["level_liters"] = nullptr;
    obj["level_percent"] = nullptr;
  }
  obj["level_percent_reference"] = "highest_calibrated_volume";
}

/* ---------- Aktor-Soll / Ist ---------- */
struct ActuatorState {
  bool light = false;
  bool heater = false;
  bool circulation = false;
  bool exhaust = false;
  bool waterPump = false;
  bool lampCooling = false;
};
// Arduino IDE generates function prototypes automatically. Declare these
// ourselves after the type definition so its autogenerated prototypes do not
// reference ActuatorState before it exists.
static void writeActuators(JsonObject obj, const ActuatorState &s);
static bool sameState(const ActuatorState &a, const ActuatorState &b);

ActuatorState targets;
ActuatorState actual;
uint32_t commandSeq = 0;
static constexpr uint8_t DOSING_PUMP_COUNT = 5; // exakt 5, gemeinsam mit C3/Remote
static constexpr const char *DOSING_CHANNEL_1_PRODUCT = "Plagron pH Min";
static constexpr bool DOSING_CHANNEL_1_ENABLED = false; // chemische Dosierung gesperrt
// Die noch nicht zugeordneten Kanäle 2-5 werden nicht automatisch belegt.
// Physischer Kanal 6 ist bisher auf dem C3 nicht vorhanden.
static constexpr uint16_t PUMP_PULSE_MAX_MS = 2000;
static constexpr unsigned long C3_HEARTBEAT_TIMEOUT_MS = 8000UL;
unsigned long lastC3StatusMs = 0;
bool c3OutputsEnabled = false;
bool c3Failsafe = true;
uint8_t c3PumpActive = 0;
uint32_t c3PumpAckSeq = 0;
bool pumpServiceOff = true;
uint8_t pendingPumpIndex = 0;
uint16_t pendingPumpDurationMs = 0;
uint32_t pumpCommandSeq = 0;
unsigned long pumpCommandMs = 0;
static bool c3Connected() {
  return lastC3StatusMs != 0 && millis() - lastC3StatusMs < C3_HEARTBEAT_TIMEOUT_MS;
}
char controlMode[8] = "auto";


/* Zwei rastfreie Schliesser: Phase D16-GND, Pumpenwartung D19-GND;
 * eine 4-Pin-RGB-LED.
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
static bool setGrowWeek(int week); // NVS-Funktion steht weiter unten
static bool beginPhaseClock(GrowPhase next);
static bool setGrowWeekManually(int week);
static void loadPhaseClock();
static void pollPhaseClock();
static bool setPhaseClockEpoch(uint32_t startEpoch);
static time_t validatedUtcNow();
static PhaseProfile blueteProfile = {NAN, NAN, false};
static constexpr char PHASE_PROFILE_NVS[] = "phaseprof_v1";
static constexpr char PHASE_NVS[] = "growphase_v1";
GrowPhase growPhase = GrowPhase::Wuchs;
static bool cycleStopped = false; // Nach W13 dauerhaft STOP, bis neuer Durchlauf gestartet wird.
static uint32_t phaseStartEpoch = 0;
static bool phaseClockPending = false;
// FALLING-Interrupt erfasst auch kurze Tastendruecke waehrend blockierender ADC-Messungen.
// Kein Kontaktprellen als mehrere Eingaben werten (50 ms Mindestabstand).
static volatile uint32_t phasePendingPresses = 0;
static volatile uint32_t phaseLastInterruptUs = 0;
static volatile uint32_t pumpPendingPresses = 0;
static volatile uint32_t pumpLastInterruptUs = 0;
static void IRAM_ATTR onPumpButtonFalling() {
  const uint32_t us = micros();
  if (static_cast<uint32_t>(us - pumpLastInterruptUs) >= PHASE_DEBOUNCE_MS * 1000UL) {
    pumpLastInterruptUs = us;
    if (pumpPendingPresses < 1000000UL) ++pumpPendingPresses;
  }
}
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
  return growPhase == GrowPhase::Bluete ? blueteProfile : WUCHS_PROFILE;
}
// Bestaetigung ist eine Software-Rueckmeldung des C3, KEINE Spannungsfreiheit.
static bool pumpServiceOffConfirmed = false;
static void refreshPhaseRgb() {
#if GROW_ENABLE_PHASE_RGB
  // Blau hat Vorrang: bei bestaetigter Filterwartung R/G AUS statt
  // Mischfarben wie Cyan/Weiss. Ohne Rueckmeldung nur Wuchs/Bluete.
  const bool blueOn = !cycleStopped && pumpServiceOff && pumpServiceOffConfirmed && c3Connected();
  const bool redOn = !cycleStopped && !blueOn && growPhase == GrowPhase::Bluete;
  const bool greenOn = !cycleStopped && !blueOn;
  digitalWrite(PIN_RGB_RED, redOn == (GROW_RGB_COMMON_ANODE == 0) ? HIGH : LOW);
  digitalWrite(PIN_RGB_GREEN, greenOn == (GROW_RGB_COMMON_ANODE == 0) ? HIGH : LOW);
  digitalWrite(PIN_RGB_BLUE, blueOn == (GROW_RGB_COMMON_ANODE == 0) ? HIGH : LOW);
#endif
}
static void loadPhaseProfile() {
  Preferences prefs;
  if (!prefs.begin(PHASE_PROFILE_NVS, true)) return;
  const bool valid = prefs.getBool("valid", false);
  const float t = prefs.getFloat("air_c", NAN);
  const float h = prefs.getFloat("humidity", NAN);
  prefs.end();
  if (valid && isfinite(t) && t >= 10.0f && t <= 40.0f &&
      isfinite(h) && h >= 20.0f && h <= 90.0f)
    blueteProfile = {t, h, true};
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
  // Der physische Taster und die API sind verbindlich: eine explizite
  // Phasenwahl darf von der Wochenuhr NICHT rueckgaengig gemacht werden.
  if (next == growPhase && phaseStartEpoch && !cycleStopped && !phaseClockPending)
    return;
  Preferences prefs;
  if (!prefs.begin(PHASE_NVS, false)) {
    Serial.println(F("PHASENWECHSEL ABGELEHNT: NVS nicht verfuegbar."));
    return;
  }
  const bool saved = prefs.putUChar("phase", static_cast<uint8_t>(next)) ==
                     sizeof(uint8_t);
  prefs.end();
  if (!saved) {
    Serial.println(F("PHASENWECHSEL ABGELEHNT: NVS-Schreibfehler."));
    return;
  }
  growPhase = next;
  // Nur ein ausdruecklicher manueller Wechsel richtet die Phasenuhr neu
  // aus: Wuchs startet W1, Bluete startet W3. Kein automatischer Reset.
  if (!beginPhaseClock(next))
    Serial.println(F("PHASEN-UHR: UTC noch nicht verfuegbar; Auswahl gespeichert, Zeitstart ausstehend."));
  if (!setGrowWeek(next == GrowPhase::Bluete ? 3 : 1))
    Serial.println(F("WARNUNG: Phasenwoche konnte nicht gespeichert werden."));
  targets = ActuatorState{};
  ++commandSeq;
  refreshPhaseRgb();
  Serial.printf("PHASE MANUELL -> %s | Profil %s | NVS OK | Uhr %s\n",
      phaseName(growPhase),
      phaseProfile().configured ? "OK" : "NICHT KONFIGURIERT",
      phaseClockPending ? "wartet auf NTP" : "gespeichert");
}

static void pollGrowPhaseButtons(unsigned long nowMs) {
  (void)nowMs;
  uint32_t pressedCount;
  noInterrupts();
  pressedCount = phasePendingPresses;
  phasePendingPresses = 0;
  interrupts();
  if (pressedCount == 0) return;
  Serial.printf("PHASENTASTER D16: %lu Druck-Flanke(n), GPIO16 aktuell %s\n",
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
  Serial.printf("PHASENTASTER D16 beim Start: %s (INPUT_PULLUP, nach GND druecken)\n",
                digitalRead(PIN_PHASE_BUTTON) == LOW ? "LOW (Kontakt bereits geschlossen)" : "HIGH (offen)");
  pinMode(PIN_PUMP_SERVICE_BUTTON, INPUT_PULLUP);
  pumpPendingPresses = 0;
  pumpLastInterruptUs = micros() - PHASE_DEBOUNCE_MS * 1000UL;
  attachInterrupt(digitalPinToInterrupt(PIN_PUMP_SERVICE_BUTTON), onPumpButtonFalling, FALLING);
  Serial.printf("PUMPENTASTER D19 beim Start: %s\n",
                digitalRead(PIN_PUMP_SERVICE_BUTTON) == LOW ? "LOW (Kontakt geschlossen)" : "HIGH (offen)");
  loadPhaseProfile();
  loadGrowPhase();
}
static void pollPumpServiceButton(unsigned long nowMs) {
  (void)nowMs;
  uint32_t count;
  noInterrupts();
  count = pumpPendingPresses;
  pumpPendingPresses = 0;
  interrupts();
  if (!count) return;
  Serial.printf("PUMPENTASTER D19: %lu Druck-Flanke(n)\n",
                static_cast<unsigned long>(count));
  if (!(count & 1UL)) return;
  if (cycleStopped) {
    Serial.println(F("Pumpen bleiben gesperrt: Durchlauf abgeschlossen."));
    return;
  }
  pumpServiceOff = !pumpServiceOff;
  pendingPumpIndex = 0;
  pendingPumpDurationMs = 0;
  ++pumpCommandSeq;
  // Blau erst nach erneuter, frischer C3-Rueckmeldung.
  pumpServiceOffConfirmed = false;
  refreshPhaseRgb();
  Serial.printf("Pumpen-Wartungssperre %s (C3-Bestaetigung ausstehend)\n",
                pumpServiceOff ? "EIN" : "AUS");
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
static constexpr char PHASE_CLOCK_NVS[] = "cycleclock_v1";
static constexpr uint32_t PHASE_WEEK_SECONDS = grow_clock::SECONDS_PER_WEEK;
static constexpr time_t PHASE_MIN_VALID_UNIX = 1700000000;
static bool phaseDurationElapsed = false;
static bool phaseClockPendingBloom = false;
static unsigned long lastPhaseClockPollMs = 0;

static time_t validatedUtcNow() {
  const time_t now = time(nullptr);
  return now >= PHASE_MIN_VALID_UNIX ? now : 0;
}
static bool persistPhaseClock(uint32_t startEpoch, bool stopped = false) {
  Preferences prefs;
  if (!prefs.begin(PHASE_CLOCK_NVS, false)) return false;
  const bool saved =
      prefs.putUInt("start", startEpoch) == sizeof(uint32_t) &&
      prefs.putBool("stopped", stopped) == sizeof(bool) &&
      prefs.putBool("pending", false) == sizeof(bool);
  prefs.end();
  return saved;
}
static bool setPhaseClockEpoch(uint32_t startEpoch) {
  const time_t now = validatedUtcNow();
  if (!now || startEpoch < static_cast<uint32_t>(PHASE_MIN_VALID_UNIX) ||
      startEpoch > static_cast<uint32_t>(now)) return false;
  if (!persistPhaseClock(startEpoch, false)) return false;
  phaseStartEpoch = startEpoch;
  phaseClockPending = false;
  cycleStopped = false;
  phaseDurationElapsed = false;
  lastPhaseClockPollMs = 0;
  return true;
}
static bool beginPhaseClock(GrowPhase next) {
  // Nur eine explizite Phasenwahl setzt die zeitliche Referenz:
  // Wuchs beginnt in W1; Bluete beginnt in W3.
  const bool bloom = next == GrowPhase::Bluete;
  const time_t now = validatedUtcNow();
  if (now) {
    const uint32_t offset = bloom ? 2U * PHASE_WEEK_SECONDS : 0U;
    if (setPhaseClockEpoch(static_cast<uint32_t>(now) - offset)) {
      phaseClockPendingBloom = bloom;
      return true;
    }
    Serial.println(F("FEHLER: Phasenuhr konnte nicht in NVS gespeichert werden."));
  }
  // Bei noch fehlender UTC keinen alten Zeitstempel als neue Phase
  // weiterbenutzen. Den ausstehenden Start ueber Neustarts erhalten.
  Preferences prefs;
  bool saved = false;
  if (prefs.begin(PHASE_CLOCK_NVS, false)) {
    saved = prefs.putUInt("start", 0U) == sizeof(uint32_t) &&
            prefs.putBool("stopped", false) == sizeof(bool) &&
            prefs.putBool("pending_bloom", bloom) == sizeof(bool) &&
            prefs.putBool("pending", true) == sizeof(bool);
    prefs.end();
  }
  if (!saved) {
    Serial.println(F("FEHLER: ausstehender Phasenstart nicht in NVS gespeichert."));
    // Zeit bleibt absichtlich gesperrt statt einen falschen Start zu erfinden.
  }
  phaseStartEpoch = 0;
  phaseDurationElapsed = false;
  cycleStopped = false;
  phaseClockPending = true;
  phaseClockPendingBloom = bloom;
  lastPhaseClockPollMs = 0;
  return false;
}
static void loadPhaseClock() {
  Preferences prefs;
  if (!prefs.begin(PHASE_CLOCK_NVS, true)) {
    Serial.println(F("DURCHLAUF: kein Startdatum hinterlegt."));
    return;
  }
  const uint32_t saved = prefs.getUInt("start", 0);
  const bool stopped = prefs.getBool("stopped", false);
  const bool pending = prefs.getBool("pending", false);
  const bool pendingBloom = prefs.getBool("pending_bloom", false);
  prefs.end();
  if (pending) {
    phaseStartEpoch = 0;
    cycleStopped = false;
    phaseClockPending = true;
    phaseClockPendingBloom = pendingBloom;
    Serial.printf("DURCHLAUF: manuelle Phase %s gespeichert; Uhrstart wartet auf NTP.\n",
                  phaseName(growPhase));
    return;
  }
  if (saved >= static_cast<uint32_t>(PHASE_MIN_VALID_UNIX)) {
    phaseStartEpoch = saved;
    cycleStopped = stopped;
    phaseDurationElapsed = stopped;
    if (cycleStopped) {
      targets = ActuatorState{};
      pumpServiceOff = true;
    }
    Serial.printf("DURCHLAUF: UTC-Start %lu; Status %s\n",
                  static_cast<unsigned long>(saved), stopped ? "STOP" : "aktiv");
  } else Serial.println(F("DURCHLAUF: Startdatum nicht konfiguriert."));
  refreshPhaseRgb();
}
static bool setGrowWeekManually(int week) {
  if (week < 1 || week > 13) return false;
  const time_t now = validatedUtcNow();
  if (!now) return false;
  const uint32_t elapsed = static_cast<uint32_t>(week - 1) * PHASE_WEEK_SECONDS;
  const uint32_t start = static_cast<uint32_t>(now) - elapsed;
  if (!setPhaseClockEpoch(start)) return false;
  const bool saved = setGrowWeek(week);
  if (saved) Serial.printf("DURCHLAUF: Woche %d manuell gestartet\n", week);
  return saved;
}
static void stopCompletedCycle() {
  // Einmalig ausfuehren. Laufende Aktoren und Pumpen sperren;
  // Anzeige/Telemetrie weiter betreiben.
  cycleStopped = true;
  phaseDurationElapsed = true;
  phaseClockPending = false;
  pumpServiceOff = true;
  pumpServiceOffConfirmed = false;
  pendingPumpIndex = 0;
  pendingPumpDurationMs = 0;
  targets = ActuatorState{};
  ++commandSeq;
  ++pumpCommandSeq;
  if (!persistPhaseClock(phaseStartEpoch, true))
    Serial.println(F("FEHLER: STOP-Status konnte nicht im NVS gespeichert werden."));
  Serial.println(F("DURCHLAUF BEENDET: W1-W13 abgeschlossen, Steuerung STOP."));
  refreshPhaseRgb();
}
static void pollPhaseClock() {
  const unsigned long nowMs = millis();
  if (lastPhaseClockPollMs && nowMs - lastPhaseClockPollMs < 10000UL) return;
  lastPhaseClockPollMs = nowMs;
  const time_t now = validatedUtcNow();
  if (!now) return; // Ohne gueltige Uhr bleibt der Controller gesperrt.
  if (phaseClockPending) {
    const uint32_t offset = phaseClockPendingBloom ? 2U * PHASE_WEEK_SECONDS : 0U;
    if (setPhaseClockEpoch(static_cast<uint32_t>(now) - offset))
      Serial.println(F("DURCHLAUF: Start nach NTP-Synchronisation gespeichert."));
    else Serial.println(F("DURCHLAUF: Start konnte nicht gespeichert werden."));
  }
  if (!phaseStartEpoch || now < static_cast<time_t>(phaseStartEpoch)) return;
  const grow_clock::CycleState state =
      grow_clock::evaluateCycle(phaseStartEpoch, static_cast<uint32_t>(now));
  if (!state.valid) return;
  // Keine zeitbasierte Phasenumstellung: manuell gewaehlte Phase bleibt
  // ueber Wochenwechsel und Stromausfall bestehen, bis der Nutzer sie aendert.
  if (state.totalWeek != growWeek && !setGrowWeek(state.totalWeek))
    Serial.println(F("DURCHLAUF: Wochenstand konnte nicht gespeichert werden."));
  if (state.finished && !cycleStopped) stopCompletedCycle();
}
static const char *phaseClockStatus() {
  if (cycleStopped) return "completed_stopped";
  if (phaseClockPending) return "waiting_for_ntp";
  if (!phaseStartEpoch) return "start_not_set";
  const time_t now = validatedUtcNow();
  if (!now) return "clock_unavailable";
  if (now < static_cast<time_t>(phaseStartEpoch)) return "clock_before_cycle_start";
  return "running";
}

static const NutrientTargets *activeNutrientTargets() {
  return growWeek >= 1 && growWeek <= 12
      ? &NUTRIENT_WEEK_TARGETS[growWeek - 1] : nullptr; // Woche 13: kein erfundener EC/pH-Wert
}
static void loadGrowWeek() {
  Preferences prefs;
  if (!prefs.begin(GROW_WEEK_NVS, true)) {
    Serial.println(F("WOCHE: nicht gesetzt (NVS nicht lesbar)."));
    return;
  }
  const uint8_t saved = prefs.getUChar("week", 0);
  prefs.end();
  growWeek = saved <= 13 ? saved : 0;
  if (const NutrientTargets *t = activeNutrientTargets()) {
    Serial.printf("WOCHE %u | Soll pH %.2f | Soll EC %.2f mS/cm\n",
        growWeek, t->ph, t->ecMsCm);
  } else {
    Serial.println(F("WOCHE: nicht gesetzt; pH/EC-Sollwerte noch nicht aktiv."));
  }
}
static bool setGrowWeek(int week) {
  if (week < 1 || week > 13) return false;
  if (growWeek == static_cast<uint8_t>(week)) return true;
  Preferences prefs;
  if (!prefs.begin(GROW_WEEK_NVS, false)) return false;
  const bool saved = prefs.putUChar("week", static_cast<uint8_t>(week)) == sizeof(uint8_t);
  prefs.end();
  if (!saved) return false;
  growWeek = static_cast<uint8_t>(week);
  const NutrientTargets *t = activeNutrientTargets();
  if (t) Serial.printf("WOCHE -> %u | pH-Soll %.2f | EC-Soll %.2f mS/cm (NVS gespeichert)\n",
      growWeek, t->ph, t->ecMsCm);
  else Serial.printf("WOCHE -> %u | kein pH/EC-Wochenprofil hinterlegt\n", growWeek);
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
bool phAlarmConfirmed = false; // Erst nach einer echten Sensorablesung bzw. 3 gueltigen Messungen
bool phAlertPending = false;   // Beim Booten niemals den voreingestellten Fault verschicken
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
  const bool measurementValid = phCalibrated && sensors.adsOk &&
      isfinite(sensors.ph) && sensors.ph >= 0.0f && sensors.ph <= 14.0f;
  const PhAlarm next = evaluatePhAlarm(
      sensors.ph, measurementValid, phAlarm, phPending, phSamples);

  if (!phAlarmConfirmed) {
    // Der initiale Fault ist nur ein Platzhalter, noch kein bestaetigter Alarm.
    // Ungueltige reale Sensorablesungen gelten wie bisher sofort als Fault.
    // Gueltige Messungen benoetigen weiterhin PH_CONFIRM (3) Samples.
    if (measurementValid && next == PhAlarm::Fault) return;
    phAlarmConfirmed = true;
    phAlarm = next;
    // Beim erstmaligen Normalzustand keine "wieder im Bereich"-Meldung erzeugen.
    // Echte Startfehler sowie bestaetigte Low/High-Werte dagegen melden.
    phAlertPending = next != PhAlarm::Normal;
    Serial.printf("pH-STARTSTATUS: %s\n", phMessage(next));
    return;
  }

  setPhAlarm(next);
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

static void handleCalibrationSerial(char command);
static void pollCalibrationCapture();
static bool calibrationActive(); // vor dem seriellen Handler deklariert
static void handlePhTestSerial() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c >= '0' && c <= '3') setPhTestMode(c);
    else if (c == 'i' || c == 'I') {
      if (calibrationActive())
        Serial.println(F("I2C-Scan waehrend Kalibrierung gesperrt."));
      else scanI2CAndBind(true);
    }
    else if (c == '7' || c == '4' || c == 'e' || c == 'E' ||
             c == 's' || c == 'S' || c == 'x' || c == 'X')
      handleCalibrationSerial(c);
    else if (c == 'k' || c == 'K') {
      String arg = Serial.readStringUntil('\n');
      arg.trim();
      const int week = arg.toInt();
      if (!setGrowWeekManually(week)) Serial.println(F("WOCHE nicht erlaubt oder NTP fehlt: k1 bis k13"));
    }
    else if (c == 'h' || c == 'H' || c == '?') {
      Serial.println(F("pH-Test: 1=LOW 5.40 | 2=HIGH 6.30 | 3=NORMAL 6.00 | 0=Test aus"));
      Serial.println(F("Woche per Serial: k1 bis k13 + Zeilenumbruch (NTP erforderlich)."));
      Serial.println(F("i = alle I2C-Adressen auf D13/D14 und D21/D22 anzeigen (keine Neuzuordnung)"));
      Serial.println(F("KALIBRIERUNG: 7=pH7 | 4=pH4 | e=EC 1413uS/cm | s=Status | x=Abbrechen"));
      Serial.println(F("ADS1115 A0=pH, A1=EC; DS18B20 D23 MUSS gueltige Wassertemperatur liefern."));
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
unsigned long lastTelemetry = 0;
unsigned long lastWifiReconnect = 0;
bool sensorMdnsStarted = false;

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

static void updateFusedAir() {
  const grow_air::Reading samples[3] = {
      {sensors.dht[0].tempC, sensors.dht[0].rh, sensors.dht[0].ok},
      {sensors.dht[1].tempC, sensors.dht[1].rh, sensors.dht[1].ok},
      {sensors.dht[2].tempC, sensors.dht[2].rh, sensors.dht[2].ok}
  };
  const grow_air::Fusion combined = grow_air::fuseThree(samples);
  sensors.airValidSensorCount = combined.validCount;
  sensors.air.tempC = combined.tempC;
  sensors.air.rh = combined.humidityPct;
  sensors.air.ok = combined.valid;
  static uint8_t lastCount = 255;
  if (combined.validCount != lastCount) {
    lastCount = combined.validCount;
    Serial.printf("DHT22-KLIMA: %u/3 gueltig | %s\n",
                  combined.validCount,
                  combined.validCount == 3 ? "Median" :
                  combined.validCount == 2 ? "Mittelwert (redundanzreduziert)" :
                  combined.validCount == 1 ? "WARNUNG: nur ein Sensor" :
                                              "FEHLER: Klimaregelung gesperrt");
  }
}
static const char *airSensorQuality() {
  switch (sensors.airValidSensorCount) {
    case 3: return "median_3";
    case 2: return "mean_2";
    case 1: return "warning_single_sensor";
    default: return "invalid_no_sensors";
  }
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
  obj["water_pump"] = s.waterPump;
  obj["lamp_cooling"] = s.lampCooling;
}

/* Sensor-ESP32 hat keine SSR-Ausgaenge. Keine GPIO-Initialisierung. */
static bool sameState(const ActuatorState &a, const ActuatorState &b) {
  return a.light == b.light && a.heater == b.heater &&
         a.circulation == b.circulation && a.exhaust == b.exhaust &&
         a.waterPump == b.waterPump && a.lampCooling == b.lampCooling;
}

static void applyActuators() {
  // Der C3 holt Sollwerte ueber HTTP ab. Keine Sensor-GPIOs fuer Aktoren.
  if (!sensors.air.ok && targets.heater) {
    targets.heater = false;
    ++commandSeq;
  }
  // Istwerte werden nur durch den C3-Status aktualisiert.
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
  // ADS1115 auf Wire / GPIO21 SDA + GPIO22 SCL, ADDR=GND (0x48).
  // BH1750 getrennt auf bhWire / GPIO13 SDA + GPIO14 SCL.
  // I2C-Scan ist reine Diagnose und darf ADS/BH nicht auf andere Busse umbinden.
  ads.setGain(GAIN_ONE);
  sensors.adsOk = ads.begin(0x48, &Wire);
  adsFoundAddr = sensors.adsOk ? 0x48 : 0;
  adsFoundBus = sensors.adsOk ? "D21(SDA)/D22(SCL)" : "nicht gefunden";
  if (sensors.adsOk)
    Serial.println(F("ADS1115 GEFUNDEN: 0x48 auf D21/D22 (Sensor-I2C)"));
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
// Exakte Physik hinter der Tabelle: VPD = e_sat(Blatt) - RH * e_sat(Luft).
static float computeLeafVpd(float airTempC, float relativeHumidity, float leafTempC) {
  if (!isfinite(airTempC) || !isfinite(leafTempC) ||
      !isfinite(relativeHumidity) || relativeHumidity < 0.0f ||
      relativeHumidity > 100.0f) return NAN;
  return saturatedVaporPressureKpa(leafTempC) -
         saturatedVaporPressureKpa(airTempC) * (relativeHumidity / 100.0f);
}
struct VpdControlBand {
  float minKpa;
  float maxKpa;
  const char *stage;
  bool configured;
};
// Arduino-Prototyp nach Typdefinition halten (ESP32 IDE Sketch-Preprozessor).
static VpdControlBand activeVpdControlBand();
// 2 Wochen Wachstum, 2 Wochen Uebergang, 9 Wochen eigentliche Bluete.
static VpdControlBand activeVpdControlBand() {
  if (growPhase == GrowPhase::Wuchs && growWeek >= 1 && growWeek <= 2)
    return {0.4f, 0.8f, "early_growth", true};
  if (growPhase == GrowPhase::Bluete && growWeek >= 3 && growWeek <= 4)
    return {0.8f, 1.2f, "transition", true};
  if (growPhase == GrowPhase::Bluete && growWeek >= 5 && growWeek <= 13)
    return {1.2f, 1.6f, "flowering", true};
  return {NAN, NAN, "week_or_phase_not_set", false};
}
static const char *vpdChartBand(float value) {
  if (!isfinite(value)) return "unavailable";
  if (value < 0.4f || value > 1.6f) return "danger";
  if (value < 0.8f) return "low_transpiration";
  if (value < 1.2f) return "healthy_transpiration";
  return "high_transpiration";
}
static void updateLeafVpdCandidate() {
  sensors.vpdLeafCandidate = NAN;
  // Kandidat aus denselben fusionierten Luftdaten wie der regulaere VPD.
  // Die thermische ROI ist erst nach ausdruecklicher Validierung Blatttemperatur.
  if (!sensors.air.ok || !sensors.mlx90640Ok ||
      !isfinite(sensors.thermalRoiTempC)) return;
  sensors.vpdLeafCandidate = computeLeafVpd(
      sensors.air.tempC, sensors.air.rh, sensors.thermalRoiTempC);
}

static void updateActiveVpd() {
  sensors.vpd = NAN;
  sensors.vpdAir = NAN;
  sensors.leafTemperatureC = NAN;
  sensors.vpdFromCamera = false;
  if (!sensors.air.ok) return;
  const float airC = sensors.air.tempC;
  const float rh = sensors.air.rh;
  sensors.vpdAir = computeVpd(airC, rh);
  sensors.leafTemperatureC = airC - VPD_LEAF_AIR_OFFSET_C;
  sensors.vpd = computeLeafVpd(airC, rh, sensors.leafTemperatureC);
#if GROW_ENABLE_MLX90640 && GROW_MLX_LEAF_VERIFIED
  // Nur nach ausdruecklicher Validierung der Kamera-ROI, nicht bei blosser Montage.
  if (sensors.mlx90640Ok && isfinite(sensors.thermalRoiTempC) &&
      isfinite(sensors.vpdLeafCandidate)) {
    sensors.leafTemperatureC = sensors.thermalRoiTempC;
    sensors.vpd = sensors.vpdLeafCandidate;
    sensors.vpdFromCamera = true;
  }
#endif
}
#if GROW_ENABLE_MLX90640
static Adafruit_MLX90640 mlx90640;
static float mlxFrame[32 * 24];
static bool mlxInitialized = false;
static unsigned long lastMlxReadMs = 0;
static void setupMlx90640() {
  Wire.setClock(400000); // Kamera-Uebertragung; ADS danach wieder 100 kHz
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
    updateActiveVpd();
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
  if (n < 96) { updateLeafVpdCandidate(); updateActiveVpd(); return; }
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
  updateActiveVpd();
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
  updateFusedAir();

  updateLeafVpdCandidate();
  updateActiveVpd();

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

/* ---------- Sichtbare Live-Messwerte im seriellen Monitor ---------- */
static void printLiveFloat(float value, uint8_t precision) {
  if (isfinite(value)) Serial.print(value, precision);
  else Serial.print(F("--"));
}

static void printLiveSensorValues() {
  Serial.println(F("========== LIVE-SENSORWERTE =========="));
  for (uint8_t i = 0; i < 3; ++i) {
    Serial.printf("DHT22 #%u | Luft: ", i + 1);
    printLiveFloat(sensors.dht[i].tempC, 1);
    Serial.print(F(" C | Feuchte: "));
    printLiveFloat(sensors.dht[i].rh, 1);
    Serial.println(F(" %"));
  }
  Serial.print(F("KLIMA (fusioniert) | Luft: "));
  printLiveFloat(sensors.air.tempC, 1);
  Serial.print(F(" C | Feuchte: "));
  printLiveFloat(sensors.air.rh, 1);
  Serial.printf(" %% | gueltige DHT: %u/3\n", sensors.airValidSensorCount);

  Serial.printf("DS18B20 | erkannt: %u | Sensor 1: ", sensors.dsCount);
  printLiveFloat(sensors.ds1C, 2);
  Serial.print(F(" C | Sensor 2: "));
  printLiveFloat(sensors.ds2C, 2);
  Serial.println(F(" C"));
  Serial.print(F("WASSER | "));
  printLiveFloat(sensors.waterTempC, 2);
  Serial.println(F(" C"));

  Serial.print(F("BH1750 | "));
  if (bh1750Present) Serial.printf("%lu lx\n", static_cast<unsigned long>(sensors.lux));
  else Serial.println(F("NICHT ERKANNT"));

  Serial.printf("ADS1115 | %s\n", sensors.adsOk ? "OK" : "NICHT ERKANNT");
  Serial.print(F("pH | "));
  printLiveFloat(sensors.ph, 2);
  Serial.print(F(" | A0: "));
  printLiveFloat(sensors.phInputV, 3);
  Serial.printf(" V | kalibriert: %s\n", phCalibrated ? "ja" : "nein");

  Serial.print(F("EC | "));
  printLiveFloat(sensors.ec, 2);
  Serial.print(F(" mS/cm | TDS: "));
  printLiveFloat(sensors.tdsPpm, 1);
  Serial.print(F(" ppm | A1: "));
  printLiveFloat(sensors.tdsInputV, 3);
  Serial.println(F(" V"));
  Serial.println(F("====================================="));
}

/* ---------- Integrierte pH-/EC-Kalibrierung, ohne Firmwarewechsel ---------- */
// Die 160x20ms (pH) bzw. 8x30x40ms (EC) Samples werden ueber den
// regulaeren loop verteilt: Webserver, C3-Heartbeat und Alarme bleiben aktiv.
enum class CalibrationMode : uint8_t { None, Ph7, Ph4, Ec1413 };
struct CalibrationCapture {
  CalibrationMode mode = CalibrationMode::None;
  grow_cal::Stats window;
  grow_cal::Stats medians;
  float ecSamples[grow_cal::EC_SAMPLES_PER_WINDOW] = {};
  float waterTempStartC = NAN;
  uint8_t windowsDone = 0;
  uint32_t nextSampleMs = 0;
  uint32_t startedMs = 0;
};
static CalibrationCapture calibration;

// Einziger ADS1115-Lesepfad waehrend der Kalibrierung.
static bool calibrationActive() {
  return calibration.mode != CalibrationMode::None;
}
static void resetEcFilterAfterCalibration() {
  ecRingIndex = 0;
  ecRingCount = 0;
  lastEcSampleMs = 0;
}
static void endCalibrationCapture() {
  calibration = CalibrationCapture{};
  resetEcFilterAfterCalibration();
  Serial.println(F("KALIBRIERMODUS ENDE: normale Messungen wieder freigegeben."));
}

// Explizite Arduino-Prototypen erst NACH Enum/Struct-Deklaration.
static const char *calibrationName(CalibrationMode mode);
static void printCalibrationStatus();
static bool storePhCalibrationPoint(CalibrationMode mode, float voltage, float temperatureC);
static bool storeEcCalibration(float voltage, float temperatureC, float factor);
static void finishCalibration();

static const char *calibrationName(CalibrationMode mode) {
  switch (mode) {
    case CalibrationMode::Ph7: return "pH7";
    case CalibrationMode::Ph4: return "pH4";
    case CalibrationMode::Ec1413: return "EC1413";
    default: return "inaktiv";
  }
}
static void printCalibrationStatus() {
  Serial.println(F("--- Kalibrierstatus / NVS ---"));
  Serial.printf("ADS: %s | DS18B20 Wasser: ", sensors.adsOk ? "OK" : "FEHLT");
  if (validDsTempC(sensors.waterTempC))
    Serial.printf("%.2f C\n", sensors.waterTempC);
  else Serial.println(F("FEHLT, Kalibrieren gesperrt"));
  Serial.print(F("pH7: "));
  if (isfinite(phVolt7)) Serial.printf("%.6f V\n", phVolt7);
  else Serial.println(F("FEHLT"));
  Serial.print(F("pH4: "));
  if (isfinite(phVolt4)) Serial.printf("%.6f V\n", phVolt4);
  else Serial.println(F("FEHLT"));
  Serial.printf("pH: %s | EC: %s\n",
                phCalibrated ? "KALIBRIERT" : "UNVOLLSTAENDIG",
                ecCalibrated ? "KALIBRIERT" : "UNVOLLSTAENDIG");
  if (ecCalibrated)
    Serial.printf("EC A1 Referenz %.6f V | Faktor %.5f | %.2f C\n",
                  ecReferenceInputV, ecFactor, ecCalTempC);
  if (calibration.mode != CalibrationMode::None)
    Serial.printf("LAEUFT: %s | Messungen %u | Fenster %u/%u\n",
                  calibrationName(calibration.mode), calibration.window.count,
                  calibration.windowsDone, grow_cal::EC_WINDOWS);
}
static bool storePhCalibrationPoint(CalibrationMode mode, float voltage, float temperatureC) {
  const bool isSeven = mode == CalibrationMode::Ph7;
  if (!isSeven && mode != CalibrationMode::Ph4) return false;
  if (!isfinite(voltage) || voltage < 0.0f || voltage > 3.3f ||
      !validDsTempC(temperatureC)) return false;
  const float other = isSeven ? phVolt4 : phVolt7;
  if (isfinite(other) && fabsf(other - voltage) < 0.001f) {
    Serial.println(F("ABGELEHNT: Spannungsabstand zwischen pH4 und pH7 <1mV."));
    return false;
  }
  const char *vk = isSeven ? "volt7" : "volt4";
  const char *tk = isSeven ? "temp7" : "temp4";
  const char *hk = isSeven ? "have7" : "have4";
  Preferences p;
  if (!p.begin("phads_v1", false)) return false;
  // Existierende Punkte bleiben erhalten; Flag zuletzt schreiben.
  const bool written = p.putFloat(vk, voltage) == sizeof(float) &&
                       p.putFloat(tk, temperatureC) == sizeof(float) &&
                       p.putBool(hk, true) == sizeof(bool);
  p.end();
  if (!written) return false;
  Preferences verify;
  if (!verify.begin("phads_v1", true)) return false;
  const float savedV = verify.getFloat(vk, NAN);
  const float savedT = verify.getFloat(tk, NAN);
  const bool ok = verify.getBool(hk, false) &&
                  isfinite(savedV) && isfinite(savedT) &&
                  fabsf(savedV - voltage) <= 0.000001f &&
                  fabsf(savedT - temperatureC) <= 0.01f;
  verify.end();
  loadPhCalibration();
  return ok;
}
static bool storeEcCalibration(float voltage, float temperatureC, float factor) {
  if (!isfinite(voltage) || voltage <= 0.05f || voltage > 2.3f ||
      !validDsTempC(temperatureC) || !isfinite(factor) ||
      factor < 0.1f || factor > 10.0f) return false;
  Preferences p;
  if (!p.begin("ecads_v1", false)) return false;
  const bool written = p.putFloat("factor", factor) == sizeof(float) &&
                       p.putFloat("ref_v", voltage) == sizeof(float) &&
                       p.putFloat("temp_c", temperatureC) == sizeof(float) &&
                       p.putBool("valid", true) == sizeof(bool);
  p.end();
  if (!written) return false;
  Preferences verify;
  if (!verify.begin("ecads_v1", true)) return false;
  const float savedFactor = verify.getFloat("factor", NAN);
  const float savedV = verify.getFloat("ref_v", NAN);
  const float savedT = verify.getFloat("temp_c", NAN);
  const bool ok = verify.getBool("valid", false) &&
                  isfinite(savedFactor) && isfinite(savedV) && isfinite(savedT) &&
                  fabsf(savedFactor - factor) <= 0.00001f &&
                  fabsf(savedV - voltage) <= 0.000001f &&
                  fabsf(savedT - temperatureC) <= 0.01f;
  verify.end();
  loadEcCalibration();
  return ok && ecCalibrated;
}
static void finishCalibration() {
  const CalibrationMode mode = calibration.mode;
  // Temperatur erst NACH dem letzten ADC-Sample erneut pruefen.
  ds18b20.requestTemperatures();
  float waterC = NAN;
  if (sensors.dsCount == 1) {
    waterC = ds18b20.getTempCByIndex(0);
  } else if (sensors.dsCount > 1 && waterSensorRomSelected) {
    waterC = ds18b20.getTempC(waterSensorRom);
  }
  if (!validDsTempC(waterC) ||
      fabsf(waterC - calibration.waterTempStartC) > 3.0f) {
    Serial.println(F("ABGELEHNT: DS18B20 fehlt oder Temperaturdifferenz >3C."));
    endCalibrationCapture();
    return;
  }
  sensors.waterTempC = waterC;
  bool saved = false;
  if (mode == CalibrationMode::Ph7 || mode == CalibrationMode::Ph4) {
    const float v = calibration.window.mean();
    const float sd = calibration.window.stdDev();
    Serial.printf("%s ADS A0: %.6f V | Std %.2f mV | DS %.2f C\n",
                  calibrationName(mode), v, sd * 1000.0f, waterC);
    if (calibration.window.count == grow_cal::PH_SAMPLES &&
        isfinite(sd) && sd <= grow_cal::PH_MAX_STD_V)
      saved = storePhCalibrationPoint(mode, v, waterC);
    else Serial.println(F("ABGELEHNT: pH-Messwerte instabil (>10mV) oder unvollstaendig."));
  } else if (mode == CalibrationMode::Ec1413) {
    const float v = calibration.medians.mean();
    const float sd = calibration.medians.stdDev();
    const float span = calibration.medians.maxV - calibration.medians.minV;
    Serial.printf("EC ADS A1: %.6f V | Median-Std %.2f mV | Spanne %.2f mV | DS %.2f C\n",
                  v, sd * 1000.0f, span * 1000.0f, waterC);
    if (calibration.windowsDone == grow_cal::EC_WINDOWS &&
        isfinite(sd) && sd <= grow_cal::EC_MAX_MEDIAN_STD_V &&
        span <= grow_cal::EC_MAX_MEDIAN_SPAN_V) {
      const float factor = grow_cal::ecCalibrationFactor(v, waterC);
      if (isfinite(factor)) saved = storeEcCalibration(v, waterC, factor);
      else Serial.println(F("ABGELEHNT: Referenzspannung/EC-Faktor unplausibel."));
    } else Serial.println(F("ABGELEHNT: EC-Mediane instabil (>10mV Std oder >30mV Spanne)."));
  }
  endCalibrationCapture();
  if (saved) {
    Serial.println(F("OK: NVS-Kalibrierung gespeichert und zurueckgelesen."));
    // Neue pH-/EC-Werte erst mit frischen ADC-Messungen nach Kalibrierende.
  } else Serial.println(F("NICHT GESPEICHERT: Kalibrierung abgelehnt oder NVS-Fehler."));
  printCalibrationStatus();
}
static void handleCalibrationSerial(char command) {
  if (command == 's' || command == 'S') {
    printCalibrationStatus();
    return;
  }
  if (command == 'x' || command == 'X') {
    if (calibration.mode != CalibrationMode::None) {
      endCalibrationCapture();
      Serial.println(F("ABGEBROCHEN: kein neuer Kalibrierwert gespeichert."));
    }
    return;
  }
  if (calibration.mode != CalibrationMode::None) {
    Serial.println(F("Kalibrierung laeuft, zum Abbrechen 'x' senden."));
    return;
  }
  if (!sensors.adsOk) {
    Serial.println(F("ABGELEHNT: ADS1115 0x48 nicht verfuegbar."));
    return;
  }
  if (!validDsTempC(sensors.waterTempC)) {
    Serial.println(F("ABGELEHNT: DS18B20-Wassertemperatur auf D23 fehlt."));
    return;
  }
  CalibrationMode mode = CalibrationMode::None;
  if (command == '7') mode = CalibrationMode::Ph7;
  else if (command == '4') mode = CalibrationMode::Ph4;
  else if (command == 'e' || command == 'E') mode = CalibrationMode::Ec1413;
  if (mode == CalibrationMode::None) return;
  calibration = CalibrationCapture{};
  resetEcFilterAfterCalibration(); // keine alten EC-Medianwerte verwenden
  calibration.mode = mode;
  calibration.waterTempStartC = sensors.waterTempC;
  calibration.startedMs = millis();
  calibration.nextSampleMs = millis();
  Serial.printf("KALIBRIERUNG START: %s | DS %.2f C | mit x abbrechen.\n",
                calibrationName(mode), calibration.waterTempStartC);
  Serial.println(F("KALIBRIERMODUS: normale Sensorwerte, Hintergrund-ADC, I2C-Scan und Telemetrie PAUSIERT."));
}
static void pollCalibrationCapture() {
  if (calibration.mode == CalibrationMode::None) return;
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - calibration.nextSampleMs) < 0) return;
  if (!sensors.adsOk || !validDsTempC(sensors.waterTempC) ||
      now - calibration.startedMs > 60000UL) {
    Serial.println(F("KALIBRIERUNG ABBRUCH: ADS/DS18B20 fehlt oder Timeout."));
    endCalibrationCapture();
    return;
  }
  const bool ec = calibration.mode == CalibrationMode::Ec1413;
  const float v = adsCountsToVolts(ads.readADC_SingleEnded(ec ? 1 : 0));
  if (!calibration.window.add(v, ec ? 2.3f : 3.3f)) {
    Serial.println(F("KALIBRIERUNG ABBRUCH: ADS-Messung ungueltig."));
    endCalibrationCapture();
    return;
  }
  if (!ec) {
    calibration.nextSampleMs = now + grow_cal::PH_INTERVAL_MS;
    if (calibration.window.count == grow_cal::PH_SAMPLES) finishCalibration();
    return;
  }
  const uint16_t n = calibration.window.count;
  calibration.ecSamples[n - 1] = v;
  calibration.nextSampleMs = now + grow_cal::EC_INTERVAL_MS;
  if (n != grow_cal::EC_SAMPLES_PER_WINDOW) return;
  const float medianV = grow_cal::median30(calibration.ecSamples);
  const float rawStd = calibration.window.stdDev();
  if (!calibration.medians.add(medianV, 2.3f)) {
    endCalibrationCapture();
    Serial.println(F("EC ABBRUCH: ungueltiger Fenster-Median."));
    return;
  }
  ++calibration.windowsDone;
  Serial.printf("EC Fenster %u/%u: Median %.5f V | Roh-Std %.1f mV\n",
                calibration.windowsDone, grow_cal::EC_WINDOWS, medianV, rawStd * 1000.0f);
  if (calibration.windowsDone == grow_cal::EC_WINDOWS) {
    finishCalibration();
    return;
  }
  calibration.window = grow_cal::Stats{};
  calibration.nextSampleMs = now + grow_cal::EC_WINDOW_GAP_MS;
}

/* ---------- Steuerung ---------- */

static void autoControl() {
  const time_t nowUtc = validatedUtcNow();
  const bool pastEnd = phaseStartEpoch && nowUtc &&
      grow_clock::evaluateCycle(phaseStartEpoch, static_cast<uint32_t>(nowUtc)).finished;
  if (cycleStopped || pastEnd || !phaseStartEpoch || !nowUtc) {
    if (!sameState(targets, ActuatorState{})) ++commandSeq;
    targets = ActuatorState{};
    return;
  }
  if (strcmp(controlMode, "auto") != 0) return;

  // Licht, Wasserpumpe und Dosierung bleiben von dieser Klimaregelung unberuehrt.
  ActuatorState before = targets;
  const VpdControlBand band = activeVpdControlBand();
  if (!sensors.air.ok || !isfinite(sensors.vpd) || !band.configured) {
    targets.heater = false;
    targets.circulation = false;
    targets.exhaust = false;
    if (!sameState(before, targets)) ++commandSeq;
    return;
  }
  const float t = sensors.air.tempC;
  const float vpd = sensors.vpd;
  const PhaseProfile &profile = phaseProfile();
  static constexpr float VPD_HYST = 0.05f; // verhindert Schaltflattern
  const bool tooHumid = vpd < band.minKpa - VPD_HYST;
  const bool tooDry = vpd > band.maxKpa + VPD_HYST;

  // Abluft kann bei hoher Feuchte nur helfen, wenn die Zuluft trockener ist.
  // Ohne Zuluftsensor ist die Wirksamkeit nicht verifiziert.
  if (tooHumid) targets.exhaust = true;
  else if (vpd > band.minKpa + VPD_HYST) targets.exhaust = false;
  if (tooDry) targets.exhaust = false;

  // Temperatur-Sollwert muss fuer die jeweilige Phase explizit konfiguriert sein.
  if (!profile.configured) {
    targets.heater = false;
  } else {
    if (t < profile.targetAirC - TEMP_HYST && !tooDry)
      targets.heater = true;
    else if (t > profile.targetAirC + TEMP_HYST || tooDry)
      targets.heater = false;

    // Uebertemperatur relativ zum konfigurierten Sollwert hat Vorrang.
    if (t > profile.targetAirC + 2.0f) {
      targets.heater = false;
      targets.exhaust = true;
    }
  }
  // Nicht gleichzeitig heizen und Abluft einschalten.
  if (targets.heater) targets.exhaust = false;
  // Umluft verteilt nur die Raumluft, sie senkt die Feuchte nicht direkt.
  targets.circulation = tooHumid || tooDry || targets.exhaust || targets.heater;
  if (!sameState(before, targets)) ++commandSeq;
}

/* ---------- Lokale HTTP-API ---------- */

static void handleSummary() {
  if (!localAuthorized()) return sendUnauthorized();

  DynamicJsonDocument doc(4096);
  doc["ok"] = true;

  JsonObject control = doc.createNestedObject("control");
  control["control_mode"] = controlMode;
  control["phase"] = phaseName(growPhase);
  writeNutrientTargets(control);
  control["phase_profile_configured"] = phaseProfile().configured;
  const VpdControlBand vpdBand = activeVpdControlBand();
  control["phase_clock_status"] = phaseClockStatus();
  control["phase_started_at_unix"] = phaseStartEpoch ? static_cast<unsigned long>(phaseStartEpoch) : 0UL;
  control["phase_duration_elapsed"] = phaseDurationElapsed;
  control["cycle_stopped"] = cycleStopped || !phaseStartEpoch || !validatedUtcNow();
  control["vpd_control_stage"] = vpdBand.stage;
  control["vpd_control_configured"] = vpdBand.configured;
  if (vpdBand.configured) {
    control["vpd_target_min_kpa"] = vpdBand.minKpa;
    control["vpd_target_max_kpa"] = vpdBand.maxKpa;
  } else {
    control["vpd_target_min_kpa"] = nullptr;
    control["vpd_target_max_kpa"] = nullptr;
  }
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
  writeLevelMeasurement(reading);
  if (sensors.air.ok) {
    reading["air_temperature"] = sensors.air.tempC;
    reading["humidity"] = sensors.air.rh;
  } else {
    reading["air_temperature"] = nullptr;
    reading["humidity"] = nullptr;
  }
  reading["air_valid_sensor_count"] = sensors.airValidSensorCount;
  reading["air_measurement_quality"] = airSensorQuality();
  reading["air_sensor_warning"] = sensors.airValidSensorCount == 1;
  setJsonFloat(reading, "vpd", sensors.vpd);
  setJsonFloat(reading, "vpd_air", sensors.vpdAir);
  setJsonFloat(reading, "leaf_temperature_c", sensors.leafTemperatureC);
  reading["vpd_source"] = sensors.vpdFromCamera ? "verified_infrared_leaf" : "leaf_air_minus_2c";
  reading["vpd_chart_band"] = vpdChartBand(sensors.vpd);
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
  reading["calibration_active"] = calibration.mode != CalibrationMode::None;
  reading["calibration_mode"] = calibrationName(calibration.mode);
  reading["calibration_samples"] = calibration.window.count;
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
  outputs["source"] = "esp32_c3";
  outputs["connected"] = c3Connected();
  outputs["outputs_enabled"] = c3Connected() && c3OutputsEnabled;
  outputs["failsafe"] = c3Failsafe;
  outputs["pump_count"] = DOSING_PUMP_COUNT;
  outputs["pump_1_product"] = DOSING_CHANNEL_1_PRODUCT;
  outputs["pump_1_enabled"] = DOSING_CHANNEL_1_ENABLED;
  outputs["pump_active"] = c3Connected() ? c3PumpActive : 0;
  outputs["pump_service_off"] = pumpServiceOff;
  outputs["pump_service_off_confirmed"] = pumpServiceOff && pumpServiceOffConfirmed && c3Connected();
  JsonObject actualObj = outputs.createNestedObject("actual");
  writeActuators(actualObj, actual);

  String body;
  serializeJson(doc, body);
  localServer.send(200, "application/json", body);
}

static void handleActuatorTargets() {
  if (!localAuthorized()) return sendUnauthorized();

  StaticJsonDocument<1536> doc;
  doc["ok"] = true;
  doc["power_controller_connected"] = c3Connected();
  doc["pump_count"] = DOSING_PUMP_COUNT;
  doc["pump_1_product"] = DOSING_CHANNEL_1_PRODUCT;
  doc["pump_1_enabled"] = DOSING_CHANNEL_1_ENABLED;
  doc["pump_service_off"] = pumpServiceOff;
  doc["pump_pending"] = pendingPumpIndex != 0 && (millis() - pumpCommandMs) < 3000UL;
  doc["pump_index"] = pendingPumpIndex;
  doc["pump_duration_ms"] = pendingPumpDurationMs;
  doc["pump_seq"] = pumpCommandSeq;
  doc["control_mode"] = controlMode;
  doc["phase"] = phaseName(growPhase);
  writeNutrientTargets(doc.as<JsonObject>());
  doc["phase_profile_configured"] = phaseProfile().configured;
  const VpdControlBand vpdBand = activeVpdControlBand();
  doc["phase_clock_status"] = phaseClockStatus();
  doc["phase_started_at_unix"] = phaseStartEpoch ? static_cast<unsigned long>(phaseStartEpoch) : 0UL;
  doc["phase_duration_elapsed"] = phaseDurationElapsed;
  doc["cycle_stopped"] = cycleStopped || !phaseStartEpoch || !validatedUtcNow();
  doc["vpd_control_stage"] = vpdBand.stage;
  if (vpdBand.configured) {
    doc["vpd_target_min_kpa"] = vpdBand.minKpa;
    doc["vpd_target_max_kpa"] = vpdBand.maxKpa;
  }
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

static void c3ApiError(int code, const char *reason) {
  StaticJsonDocument<128> d;
  d["ok"] = false;
  d["error"] = reason;
  String response;
  serializeJson(d, response);
  localServer.send(code, "application/json", response);
}
static void handleControlTargets() {
  if (!localAuthorized()) return sendUnauthorized();
  if (cycleStopped || !phaseStartEpoch || !validatedUtcNow()) return c3ApiError(423, "cycle_not_running");
  if (strcmp(controlMode, "manual") != 0)
    return c3ApiError(409, "manual_mode_required");
  if (!c3Connected() || !c3OutputsEnabled)
    return c3ApiError(503, "c3_not_ready");
  StaticJsonDocument<256> in;
  if (deserializeJson(in, localServer.arg("plain")))
    return c3ApiError(400, "bad_json");
  ActuatorState next = targets;
  const char *names[6] = {"light", "heater", "circulation_vent",
                          "exhaust_vent", "water_pump", "lamp_cooling"};
  bool *fields[6] = {&next.light, &next.heater, &next.circulation,
                     &next.exhaust, &next.waterPump, &next.lampCooling};
  bool any = false;
  for (uint8_t i=0; i<6; ++i) {
    if (!in.containsKey(names[i])) continue;
    if (!in[names[i]].is<bool>()) return c3ApiError(400, "boolean_required");
    *fields[i] = in[names[i]].as<bool>();
    any = true;
  }
  if (!any) return c3ApiError(400, "no_target");
  if (pumpServiceOff && next.waterPump)
    return c3ApiError(423, "pump_service_locked");
  if (next.heater && !sensors.air.ok)
    return c3ApiError(409, "heater_sensor_unavailable");
  if (!sameState(next, targets)) {
    targets = next;
    ++commandSeq;
  }
  localServer.send(200, "application/json",
                   "{\"ok\":true,\"status\":\"requested_not_confirmed\"}");
}
static void handlePumpPulse() {
  if (!localAuthorized()) return sendUnauthorized();
  if (cycleStopped || !phaseStartEpoch || !validatedUtcNow()) return c3ApiError(423, "cycle_not_running");
  // The C3 does not yet process Sensor pump_seq/pump_index requests. Do not
  // report a queued request as successful or set a pending command that is
  // never acknowledged. Manual dosing is locked until integration + calibration.
  c3ApiError(503, "dosing_not_implemented");
}
static void handleActuatorStatus() {
  if (!localAuthorized()) return sendUnauthorized();
  StaticJsonDocument<768> doc;
  if (deserializeJson(doc, localServer.arg("plain")) ||
      doc["pump_count"].as<int>() != DOSING_PUMP_COUNT)
    return c3ApiError(400, "bad_actuator_status");
  JsonObject a = doc["actual"];
  if (a.isNull()) return c3ApiError(400, "missing_actual");
  actual.light = a["light"] | false;
  actual.heater = a["heater"] | false;
  actual.circulation = a["circulation_vent"] | false;
  actual.exhaust = a["exhaust_vent"] | false;
  actual.waterPump = a["water_pump"] | false;
  actual.lampCooling = a["lamp_cooling"] | false;
  c3Failsafe = doc["failsafe"] | true;
  c3OutputsEnabled = doc["outputs_enabled"] | false;
  c3PumpActive = doc["pump_active"] | 0;
  c3PumpAckSeq = doc["pump_ack_seq"] | 0U;
  lastC3StatusMs = millis();
  pumpServiceOffConfirmed = pumpServiceOff &&
      (doc["pump_service_off"] | false) &&
      (doc["pump_service_off_confirmed"] | false) &&
      c3PumpActive == 0 && !actual.waterPump;
  if (pendingPumpIndex != 0 && c3PumpAckSeq == pumpCommandSeq) {
    pendingPumpIndex = 0;
    pendingPumpDurationMs = 0;
  }
  // C3 bestaetigt nur die logische Pumpensperre, keine elektrische Trennung.
  refreshPhaseRgb();
  localServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleControlMode() {
  if (!localAuthorized()) return sendUnauthorized();
  if (cycleStopped || !phaseStartEpoch || !validatedUtcNow()) return c3ApiError(423, "cycle_not_running");

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
  pendingPumpIndex = 0;
  pendingPumpDurationMs = 0;
  ++pumpCommandSeq;
  if (strcmp(controlMode, "auto") == 0) targets = ActuatorState{};
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
  if (selected < 1 || selected > 13) {
    return localServer.send(400, "application/json",
        "{\"ok\":false,\"error\":\"week_must_be_1_to_13\"}");
  }
  if (!setGrowWeekManually(selected)) {
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
  // Dieselbe Phasenfunktion wie am D16-Taster.
  setGrowPhase(next);
  // Nur dann Erfolg melden, wenn der gewaehlte Zustand auch in NVS steht.
  Preferences prefs;
  if (!prefs.begin(PHASE_NVS, true)) {
    return localServer.send(500, "application/json",
        "{\"ok\":false,\"error\":\"phase_nvs_unavailable\"}");
  }
  const uint8_t stored = prefs.getUChar("phase", 255);
  prefs.end();
  if (stored != static_cast<uint8_t>(growPhase)) {
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

static void handlePhaseProfile() {
  if (!localAuthorized()) return sendUnauthorized();
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, localServer.arg("plain")) || !doc.is<JsonObject>())
    return localServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_json\"}");
  const char *phase = doc["phase"] | "";
  if (strcmp(phase, "bluete") != 0)
    return localServer.send(422, "application/json", "{\"ok\":false,\"error\":\"phase_must_be_bluete\"}");
  if (!doc["air_target_c"].is<float>() && !doc["air_target_c"].is<int>())
    return localServer.send(422, "application/json", "{\"ok\":false,\"error\":\"bad_air_target\"}");
  if (!doc["humidity_target_pct"].is<float>() && !doc["humidity_target_pct"].is<int>())
    return localServer.send(422, "application/json", "{\"ok\":false,\"error\":\"bad_humidity_target\"}");
  const float t = doc["air_target_c"].as<float>();
  const float h = doc["humidity_target_pct"].as<float>();
  if (!isfinite(t) || t < 10.0f || t > 40.0f ||
      !isfinite(h) || h < 20.0f || h > 90.0f)
    return localServer.send(422, "application/json", "{\"ok\":false,\"error\":\"targets_out_of_range\"}");
  Preferences prefs;
  if (!prefs.begin(PHASE_PROFILE_NVS, false))
    return localServer.send(500, "application/json", "{\"ok\":false,\"error\":\"nvs_unavailable\"}");
  const bool saved = prefs.putFloat("air_c", t) == sizeof(float) &&
                     prefs.putFloat("humidity", h) == sizeof(float) &&
                     prefs.putBool("valid", true) == 1;
  prefs.end();
  if (!saved)
    return localServer.send(500, "application/json", "{\"ok\":false,\"error\":\"nvs_write_failed\"}");
  blueteProfile = {t, h, true};
  targets = ActuatorState{};
  ++commandSeq;
  refreshPhaseRgb();
  Serial.printf("BLUETE-Profil gespeichert: Luft %.1f C, Feuchte %.1f %%\n", t, h);
  localServer.send(200, "application/json", "{\"ok\":true,\"phase\":\"bluete\",\"configured\":true}");
}

// Startzeit explizit setzen oder neu starten, ohne den Phasenschalter zu umgehen.
// {"start_unix": <UTC timestamp>} oder {"start_now":true}
static void handlePhaseClock() {
  if (!localAuthorized()) return sendUnauthorized();
  StaticJsonDocument<128> input;
  if (deserializeJson(input, localServer.arg("plain")) || !input.is<JsonObject>())
    return localServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_json\"}");
  const time_t now = validatedUtcNow();
  if (!now)
    return localServer.send(503, "application/json", "{\"ok\":false,\"error\":\"clock_unavailable\"}");
  uint32_t start = 0;
  if (input["start_now"].as<bool>() && input.size() == 1) {
    start = static_cast<uint32_t>(now);
  } else if (input.size() == 1 && input.containsKey("start_unix") &&
             (input["start_unix"].is<int>() ||
              input["start_unix"].is<long>() ||
              input["start_unix"].is<unsigned int>() ||
              input["start_unix"].is<unsigned long>())) {
    start = input["start_unix"].as<uint32_t>();
  } else {
    return localServer.send(422, "application/json", "{\"ok\":false,\"error\":\"start_now_or_start_unix_required\"}");
  }
  // Ein neuer vollstaendiger Durchlauf beginnt immer mit Wachstum.
  if (!setPhaseClockEpoch(start))
    return localServer.send(422, "application/json", "{\"ok\":false,\"error\":\"invalid_start_or_nvs_failure\"}");
  growPhase = GrowPhase::Wuchs;
  Preferences prefs;
  if (prefs.begin(PHASE_NVS, false)) {
    prefs.putUChar("phase", static_cast<uint8_t>(growPhase));
    prefs.end();
  }
  setGrowWeek(1);
  targets = ActuatorState{};
  ++commandSeq;
  refreshPhaseRgb();
  pollPhaseClock();
  localServer.send(200, "application/json", "{\"ok\":true}");
}
static void handleLevelCalibration() {
  if (!localAuthorized()) return sendUnauthorized();
  // Eine bekannte Wassermenge zum aktuellen, frischen Distanzwert erfassen.
  // Keine Aenderung an Aktoren oder Dosiervorgaengen.
  if (!levelSensorOk()) return localServer.send(
      503, "application/json", "{\"ok\":false,\"error\":\"level_sensor_unavailable\"}");
  StaticJsonDocument<128> body;
  if (deserializeJson(body, localServer.arg("plain")) ||
      !body["liters"].is<float>() && !body["liters"].is<int>()) {
    return localServer.send(
        400, "application/json", "{\"ok\":false,\"error\":\"liters_required\"}");
  }
  const float liters = body["liters"].as<float>();
  if (!isfinite(liters) || liters < 0.0f || liters > 90.0f)
    return localServer.send(
        422, "application/json", "{\"ok\":false,\"error\":\"liters_out_of_range\"}");
  const uint16_t mm = static_cast<uint16_t>(roundf(sensors.levelDistanceMm));
  if (levelPointCount >= LEVEL_CAL_MAX)
    return localServer.send(
        409, "application/json", "{\"ok\":false,\"error\":\"calibration_full\"}");
  LevelPoint candidate[LEVEL_CAL_MAX];
  memcpy(candidate, levelPoints, sizeof(levelPoints));
  uint8_t newCount = levelPointCount + 1;
  candidate[levelPointCount] = {mm, liters};
  // Sortiere nach Distanz absteigend; Liter muss entsprechend ansteigen.
  for (uint8_t i = 1; i < newCount; ++i) {
    const LevelPoint key = candidate[i];
    int j = static_cast<int>(i) - 1;
    while (j >= 0 && candidate[j].distanceMm < key.distanceMm) {
      candidate[j + 1] = candidate[j];
      --j;
    }
    candidate[j + 1] = key;
  }
  for (uint8_t i = 1; i < newCount; ++i) {
    if (candidate[i - 1].distanceMm <= candidate[i].distanceMm ||
        candidate[i - 1].liters >= candidate[i].liters)
      return localServer.send(422, "application/json",
          "{\"ok\":false,\"error\":\"non_monotonic_calibration\"}");
  }
  const uint8_t oldCount = levelPointCount;
  LevelPoint oldPoints[LEVEL_CAL_MAX];
  memcpy(oldPoints, levelPoints, sizeof(levelPoints));
  memcpy(levelPoints, candidate, sizeof(levelPoints));
  levelPointCount = newCount;
  if (!saveLevelCalibration()) {
    memcpy(levelPoints, oldPoints, sizeof(levelPoints));
    levelPointCount = oldCount;
    return localServer.send(500, "application/json",
        "{\"ok\":false,\"error\":\"nvs_save_failed\"}");
  }
  StaticJsonDocument<160> reply;
  reply["ok"] = true;
  reply["points"] = levelPointCount;
  reply["distance_mm"] = mm;
  reply["liters"] = liters;
  String out;
  serializeJson(reply, out);
  localServer.send(200, "application/json", out);
}
static void setupLocalApi() {
  localServer.collectHeaders(LOCAL_HEADER_KEYS, 1);
  localServer.on("/api/local/summary", HTTP_GET, handleSummary);
  localServer.on("/api/local/level/calibration", HTTP_POST, handleLevelCalibration);
  localServer.on("/api/local/actuators", HTTP_GET, handleActuatorTargets);
  localServer.on("/api/local/control/targets", HTTP_POST, handleControlTargets);
  localServer.on("/api/local/control/pump", HTTP_POST, handlePumpPulse);
  localServer.on("/api/local/actuator-status", HTTP_POST, handleActuatorStatus);
  localServer.on("/api/local/control/mode", HTTP_POST, handleControlMode);
  localServer.on("/api/local/control/week", HTTP_POST, handleGrowWeek);
  localServer.on("/api/local/control/phase", HTTP_POST, handleGrowPhase);
  localServer.on("/api/local/control/phase-profile", HTTP_POST, handlePhaseProfile);
  localServer.on("/api/local/control/phase-clock", HTTP_POST, handlePhaseClock);
  localServer.begin();
}

/* ---------- Telemetrie zum Homeserver ---------- */

static bool sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) return false;

  DynamicJsonDocument doc(4096);
  JsonObject root = doc.to<JsonObject>();

  setJsonFloat(root, "air_temperature", sensors.air.tempC);
  setJsonFloat(root, "humidity", sensors.air.rh);
  root["air_valid_sensor_count"] = sensors.airValidSensorCount;
  root["air_measurement_quality"] = airSensorQuality();
  root["air_sensor_warning"] = sensors.airValidSensorCount == 1;
  setJsonFloat(root, "water_temperature", sensors.waterTempC);
  setJsonFloat(root, "case_temperature", sensors.caseTempC);
  root["water_temperature_source"] = validDsTempC(sensors.waterTempC) ? "ds18b20" : "unassigned";
  root["phase"] = phaseName(growPhase);
  writeNutrientTargets(root);
  writeLevelMeasurement(root);
  root["phase_profile_configured"] = phaseProfile().configured;
  const VpdControlBand vpdBand = activeVpdControlBand();
  root["phase_clock_status"] = phaseClockStatus();
  root["phase_started_at_unix"] = phaseStartEpoch ? static_cast<unsigned long>(phaseStartEpoch) : 0UL;
  root["phase_duration_elapsed"] = phaseDurationElapsed;
  root["cycle_stopped"] = cycleStopped || !phaseStartEpoch || !validatedUtcNow();
  root["vpd_control_stage"] = vpdBand.stage;
  if (vpdBand.configured) {
    root["vpd_target_min_kpa"] = vpdBand.minKpa;
    root["vpd_target_max_kpa"] = vpdBand.maxKpa;
  }
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
  root["calibration_active"] = calibration.mode != CalibrationMode::None;
  root["calibration_mode"] = calibrationName(calibration.mode);
  root["calibration_samples"] = calibration.window.count;
  root["ph_alarm"] = phCode(phAlarm);
  root["ph_alarm_active"] = phAlarm != PhAlarm::Normal;
  root["ph_buzzer_configured"] = (GROW_ENABLE_PH_BUZZER != 0);
  root["ph_alarm_test"] = phTestActive;
  if (phTestActive) {
    root["ph_alarm_test_status"] = phCode(phTestAlarm);
    root["ph_alarm_test_value"] = phTestValue;
  }

  setJsonFloat(root, "vpd", sensors.vpd);
  setJsonFloat(root, "vpd_air", sensors.vpdAir);
  setJsonFloat(root, "leaf_temperature_c", sensors.leafTemperatureC);
  root["vpd_source"] = sensors.vpdFromCamera ? "verified_infrared_leaf" : "leaf_air_minus_2c";
  root["vpd_chart_band"] = vpdChartBand(sensors.vpd);
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
  dev["actuator_controller"] = c3Connected() ? "esp32_c3_online" : "esp32_c3_offline";
  dev["dosing_pump_count"] = DOSING_PUMP_COUNT;

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

/* ---------- Setup / Loop ---------- */

void setup() {
  // Kein SSR-Pin auf dem Sensor-ESP32, Aktoren werden hier nicht geschaltet.
  actual = ActuatorState{};
  Serial.begin(115200);
  delay(300);
  levelUart.begin(LEVEL_UART_BAUD, SERIAL_8N1, PIN_LEVEL_UART_RX, -1);
  loadLevelCalibration();
  setupGrowPhaseButtons();
  loadPhaseProfile();
  loadGrowPhase();  // Vor der Uhr den verbindlichen NVS-Phasenstatus laden.
  loadGrowWeek();
  loadPhaseClock(); // Gespeicherten Start wiederherstellen, NICHT neu starten.
  setupPhBuzzer();
  Serial.println(F("pH-Test: 1=LOW 5.40 | 2=HIGH 6.30 | 3=NORMAL 6.00 | 0=Test aus"));
  loadPhCalibration();
  loadEcCalibration();
  loadWaterSensorRom();
  loadToCalibration();
  if (!ecCalibrated) Serial.println("EC unkalibriert: 'e' im seriellen Monitor zum Kalibrieren.");
  Serial.println(F("KALIBRIERUNG INTEGRIERT: 7=pH7 4=pH4 e=EC1413 s=Status x=Abbruch h=Hilfe"));
  if (!phCalibrated) Serial.println("WARNUNG: pH-Kalibrierung fehlt.");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  Wire.setTimeOut(30);
  bhWire.begin(PIN_BH_SDA, PIN_BH_SCL, 50000);
  bhWire.setTimeOut(30);

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
  configTime(0, 0, "pool.ntp.org", "time.google.com"); // UTC-Zeit ohne Ortsannahme

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin("grow-sensor")) {
      sensorMdnsStarted = true;
      MDNS.addService("http", "tcp", LOCAL_HTTP_PORT);
    }
  }

  readSensors();
  printLiveSensorValues();
  lastSensorRead = millis();
  pollPhaseClock(); // Laufzeit-Grenzen vor dem ersten automatischen Schaltkommando pruefen.
  autoControl();
  applyActuators();
  setupLocalApi();
}

void loop() {
  const unsigned long now = millis();
  static unsigned long lastI2cRetryMs = millis();
  if (!calibrationActive() && !sensors.adsOk && now - lastI2cRetryMs >= 30000UL) {
    lastI2cRetryMs = now;
    scanI2CAndBind();
  }

  localServer.handleClient();
  pollLevelSensor();
  refreshPhaseRgb();  // Verbindungstimeout: blau sofort wieder aus.
  pollGrowPhaseButtons(now);
  pollPumpServiceButton(now);
  pollPhaseClock();
  handlePhTestSerial();
  pollCalibrationCapture(); // exklusiver ADC-Leser; lokale API/C3 bleibt erreichbar
  updatePhBuzzer(now);
  if (!calibrationActive()) {
    sampleTdsBackground(now);
    pollMlx90640(now);
  }

  if (WiFi.status() != WL_CONNECTED && now - lastWifiReconnect >= WIFI_RECONNECT_MS) {
    lastWifiReconnect = now;
    WiFi.reconnect();
  }
  if (WiFi.status() == WL_CONNECTED && !sensorMdnsStarted) {
    if (MDNS.begin("grow-sensor")) {
      sensorMdnsStarted = true;
      MDNS.addService("http", "tcp", LOCAL_HTTP_PORT);
    }
  }

  if (!calibrationActive() && now - lastSensorRead >= SENSOR_READ_MS) {
    lastSensorRead = now;
    readSensors();
    printLiveSensorValues();
    if (phTestActive) updatePhTestAlarm();
  }

  autoControl();
  applyActuators();

  if (!calibrationActive() && now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
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
