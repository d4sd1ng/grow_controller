/**
 * Hardware-Diagnose und pH-Kalibrierung auf dem EINZELNEN ESP32.
 * Aufgebaut: OLED GPIO21/22, ADS1115 A0 pH / A1 TDS, DS18B20 GPIO23.
 * Noch nicht angeschlossene Sensoren (3x DHT22 GPIO25/26/27,
 * BH1750 GPIO13/14) werden als FEHLEND angezeigt, ohne die anderen
 * Tests und die pH-Kalibrierung zu blockieren.
 *
 * SSR-Steuerpins 17/18/19/33 werden nur auf AUS initialisiert.
 * Es gibt KEINEN Schalt-/Lasttest und KEINEN 230-V-Test.
 *
 * pH-Messung ausschliesslich ueber ADS1115 A0, kein VP/GPIO36.
 * A0/A1 duerfen die ADS1115-Versorgung (3.3 V) nicht ueberschreiten.
 * Eingangsspannungen vor dem ersten Zuschalten separat messen.
 *
 * 115200 Baud: 7=pH7 kalibrieren, 4=pH4, s=Kalibrierwerte,
 * t=komplette Diagnose, r=ADS-Kalibrierung loeschen, h=Hilfe.
 * pH kann auch ohne DS18B20 kalibriert werden; EC 1413uS mit 'e'.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>
#include <math.h>

static constexpr uint8_t PIN_DS18B20 = 23;
static constexpr uint8_t PIN_PH_DO = 32;
static constexpr bool PH_DO_WIRED = false; // erst nach 3.3-V-Pegelpruefung aktivieren
static constexpr bool PH_TO_WIRED = false; // TO an A2 erst nach sicherer Pegelpruefung
static constexpr uint8_t PIN_OLED_SDA = 21;
static constexpr uint8_t PIN_OLED_SCL = 22;
static constexpr uint8_t PIN_BH_SDA = 13;
static constexpr uint8_t PIN_BH_SCL = 14;
static constexpr uint8_t PIN_DHT1 = 25;
static constexpr uint8_t PIN_DHT2 = 26;
static constexpr uint8_t PIN_DHT3 = 27;
static constexpr uint8_t PIN_SSR_LIGHT = 17;
static constexpr uint8_t PIN_SSR_CIRC = 18;
static constexpr uint8_t PIN_SSR_EXH = 19;
static constexpr uint8_t PIN_SSR_HEAT = 33;
static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr uint8_t ADS_ADDR = 0x48;

static constexpr float R_TOP_OHM = 10000.0f;
static constexpr float R_BOTTOM_OHM = 20000.0f;
static constexpr float DIVIDER_RATIO =
    R_BOTTOM_OHM / (R_TOP_OHM + R_BOTTOM_OHM); // 2/3
static constexpr float DIVIDER_RECONSTRUCT = 1.0f / DIVIDER_RATIO; // 1.5

static constexpr uint16_t LIVE_SAMPLE_COUNT = 32;
static constexpr uint16_t CAPTURE_SAMPLE_COUNT = 160;
static constexpr uint16_t EC_LIVE_SAMPLE_COUNT = 17;
static constexpr uint16_t EC_CAPTURE_SAMPLES = 120;
static constexpr float EC_REFERENCE_MS_CM = 1.413f; // 1413 uS/cm bei 25 C
static constexpr float EC_MAX_STD_V = 0.025f;
static constexpr float EC_MAX_SPAN_V = 0.090f;
static constexpr uint16_t SAMPLE_DELAY_MS = 20;
static constexpr unsigned long TEMP_INTERVAL_MS = 2000;
static constexpr unsigned long OLED_INTERVAL_MS = 500;

static constexpr float CAPTURE_MAX_STD_V = 0.010f;
static constexpr float MIN_CAL_POINT_DISTANCE_V = 0.001f;

// Neues Namespace: alte interne-ADC-Kalibrierung bleibt erhalten,
// wird aber NICHT irrtuemlich auf ADS1115-Messwerte angewendet.
static constexpr char NVS_NAMESPACE[] = "phads_v1";
static constexpr char NVS_HAVE_7[] = "have7";
static constexpr char NVS_HAVE_4[] = "have4";
static constexpr char NVS_VOLT_7[] = "volt7";
static constexpr char NVS_VOLT_4[] = "volt4";
static constexpr char NVS_TEMP_7[] = "temp7";
static constexpr char NVS_TEMP_4[] = "temp4";
static constexpr char EC_NAMESPACE[] = "ecads_v1";
static constexpr char EC_NVS_FACTOR[] = "factor";
static constexpr char EC_NVS_REFERENCE_V[] = "ref_v";

Preferences preferences;
OneWire oneWire(PIN_DS18B20);
DallasTemperature temperatureSensors(&oneWire);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_ADS1115 ads;
TwoWire bhWire = TwoWire(1);
DHT dht1(PIN_DHT1, DHT22);
DHT dht2(PIN_DHT2, DHT22);
DHT dht3(PIN_DHT3, DHT22);

bool oledOk = false;
bool adsOk = false;
bool tempSensorPresent = false;
bool bhOk = false;
uint8_t bhAddress = 0;
uint8_t oneWireCount = 0;

struct AirData { float tempC = NAN; float humidity = NAN; bool ok = false; };
AirData air[3];
float bhLux = NAN;
float liveInputV = NAN;
float livePoV = NAN;
float liveStdV = NAN;
float liveTdsV = NAN;
float liveTdsStdV = NAN;
float ecFactor = NAN;
float ecReferenceInputV = NAN;
bool ecCalibrated = false;
float liveToV = NAN;
bool liveDoHigh = false;
float liveTempC = NAN;
float liveTemp2C = NAN;

bool havePh7 = false;
bool havePh4 = false;
float ph7InputV = NAN;
float ph4InputV = NAN;
float ph7TempC = NAN;
float ph4TempC = NAN;

unsigned long lastTempMs = 0;
unsigned long lastOtherSensorsMs = 0;
unsigned long lastOledMs = 0;

struct SampleStats {
  float meanV;
  float stdV;
  uint32_t minMv;
  uint32_t maxMv;
};

static bool validTemperature(float t) {
  return !isnan(t) && t > -20.0f && t < 80.0f && t != DEVICE_DISCONNECTED_C;
}

static SampleStats sampleChannel(uint8_t channel, uint16_t count, uint16_t delayMs) {
  SampleStats empty{NAN, NAN, 0, 0};
  if (!adsOk || count == 0) return empty;
  double sum = 0.0;
  double sumSq = 0.0;
  uint32_t minMv = UINT32_MAX;
  uint32_t maxMv = 0;

  for (uint16_t i = 0; i < count; ++i) {
    const int16_t raw = ads.readADC_SingleEnded(channel);
    const float v = ads.computeVolts(raw);  // ADS1115 Gain_ONE
    if (!isfinite(v) || v < 0.0f) return empty;
    const uint32_t mv = static_cast<uint32_t>(lroundf(v * 1000.0f));
    sum += v;
    sumSq += static_cast<double>(v) * v;
    if (mv < minMv) minMv = mv;
    if (mv > maxMv) maxMv = mv;
    if (delayMs) delay(delayMs);
  }
  const double mean = sum / count;
  double variance = sumSq / count - mean * mean;
  if (variance < 0.0) variance = 0.0;
  return SampleStats{static_cast<float>(mean),
                     static_cast<float>(sqrt(variance)), minMv, maxMv};
}

static void scanBh1750() {
  bhOk = false;
  bhAddress = 0;
  const uint8_t candidates[] = {0x23, 0x5C};
  for (uint8_t addr : candidates) {
    bhWire.beginTransmission(addr);
    if (bhWire.endTransmission() == 0) {
      bhOk = true;
      bhAddress = addr;
      break;
    }
  }
}

static float readBh1750() {
  if (!bhOk) return NAN;
  bhWire.beginTransmission(bhAddress);
  bhWire.write(0x01);  // Power ON
  if (bhWire.endTransmission() != 0) return NAN;
  bhWire.beginTransmission(bhAddress);
  bhWire.write(0x10);  // Continuously H-Resolution
  if (bhWire.endTransmission() != 0) return NAN;
  delay(180);
  if (bhWire.requestFrom(static_cast<int>(bhAddress), 2) != 2) return NAN;
  const uint16_t raw = (static_cast<uint16_t>(bhWire.read()) << 8) | bhWire.read();
  return raw / 1.2f;
}

static void updateOptionalSensors(bool force = false) {
  if (!force && millis() - lastOtherSensorsMs < 3000UL) return;
  lastOtherSensorsMs = millis();
  DHT* modules[3] = {&dht1, &dht2, &dht3};
  for (uint8_t i = 0; i < 3; ++i) {
    AirData a;
    a.tempC = modules[i]->readTemperature();
    a.humidity = modules[i]->readHumidity();
    a.ok = isfinite(a.tempC) && isfinite(a.humidity) &&
           a.tempC > -20.0f && a.tempC < 70.0f &&
           a.humidity >= 0.0f && a.humidity <= 100.0f;
    if (!a.ok) { a.tempC = NAN; a.humidity = NAN; }
    air[i] = a;
  }
  bhLux = readBh1750();
}

/* CQRobot-Spezifikation: ppm = polynomial(U) * 0.5.
 * Referenzfaktor ppm/(500 ppm pro mS/cm) -> polynomial(U)/1000 mS/cm
 * bei 25 C. Kalibrierung skaliert die Kennlinie auf 1413 uS/cm.
 */
static float ecUncalibratedMs(float v) {
  if (!isfinite(v) || v < 0.0f || v > 2.3f) return NAN;
  return (133.42f*v*v*v - 255.86f*v*v + 857.39f*v) / 1000.0f;
}
static float ecFromVoltage(float v) {
  const float base = ecUncalibratedMs(v);
  return ecCalibrated && isfinite(base) ? base * ecFactor : NAN;
}
static void loadEcCalibration() {
  ecCalibrated = false;
  Preferences ecPrefs;
  if (!ecPrefs.begin(EC_NAMESPACE, true)) return;
  const bool valid = ecPrefs.getBool("valid", false);
  ecFactor = ecPrefs.getFloat(EC_NVS_FACTOR, NAN);
  ecReferenceInputV = ecPrefs.getFloat(EC_NVS_REFERENCE_V, NAN);
  ecPrefs.end();
  ecCalibrated = valid && isfinite(ecFactor) &&
                 ecFactor >= 0.1f && ecFactor <= 10.0f &&
                 isfinite(ecReferenceInputV) && ecReferenceInputV > 0.05f &&
                 ecReferenceInputV <= 2.3f;
}
static bool captureEc1413() {
  if (!adsOk) { Serial.println(F("EC ABGELEHNT: ADS1115 fehlt.")); return false; }
  Serial.println(F("EC 1413 uS/cm @25 C: erfasse A1 ueber 3 s ..."));
  const SampleStats stats = sampleChannel(1, EC_CAPTURE_SAMPLES, 25);
  const float spanV = (static_cast<float>(stats.maxMv) -
                       static_cast<float>(stats.minMv)) / 1000.0f;
  Serial.printf("EC A1: Mittel %.5f V | Std %.3f mV | Spanne %.3f V\n",
                stats.meanV, stats.stdV * 1000.0f, spanV);
  if (!isfinite(stats.meanV) || !isfinite(stats.stdV) ||
      stats.stdV > EC_MAX_STD_V || spanV > EC_MAX_SPAN_V) {
    Serial.println(F("EC NICHT gespeichert: A1 schwankt zu stark (Std >25 mV oder Spanne >90 mV)."));
    return false;
  }
  const float ecRaw = ecUncalibratedMs(stats.meanV);
  if (!isfinite(ecRaw) || ecRaw < 0.05f) {
    Serial.println(F("EC NICHT gespeichert: unplausibles A1-Signal."));
    return false;
  }
  const float factor = EC_REFERENCE_MS_CM / ecRaw;
  if (!isfinite(factor) || factor < 0.1f || factor > 10.0f) {
    Serial.println(F("EC NICHT gespeichert: Kalibrierfaktor unplausibel."));
    return false;
  }
  Preferences ecPrefs;
  if (!ecPrefs.begin(EC_NAMESPACE, false)) {
    Serial.println(F("EC NICHT gespeichert: NVS nicht verfuegbar."));
    return false;
  }
  bool ok = ecPrefs.putFloat(EC_NVS_FACTOR, factor) == sizeof(float);
  ok = (ecPrefs.putFloat(EC_NVS_REFERENCE_V, stats.meanV) == sizeof(float)) && ok;
  ok = (ecPrefs.putBool("valid", true) == sizeof(bool)) && ok;
  ecPrefs.end();
  if (!ok) { Serial.println(F("EC NICHT gespeichert: NVS Schreibfehler.")); return false; }
  loadEcCalibration();
  if (!ecCalibrated || fabsf(ecFactor - factor) > 0.00001f) {
    Serial.println(F("EC NICHT bestaetigt: NVS Lesefehler."));
    return false;
  }
  Serial.printf("EC KALIBRIERT: 1413 uS/cm = 1.413 mS/cm | A1 %.5f V | Faktor %.5f\n",
                ecReferenceInputV, ecFactor);
  Serial.println(F("ACHTUNG: Referenzleitfaehigkeit gilt bei 25 C. Ohne Wassertemperatur keine echte Temperaturkompensation."));
  return true;
}

static void printHardwareStatus() {
  Serial.println(F("\n=== HARDWARE-STATUS ==="));
  Serial.printf("OLED 0x3C : %s\n", oledOk ? "OK" : "NICHT ERKANNT");
  Serial.printf("ADS  0x48: %s\n", adsOk ? "OK" : "NICHT ERKANNT");
  if (adsOk) {
    Serial.printf("ADS A0 pH: %.5f V | PO (geschaetzt): %.5f V | Std: %.2f mV\n",
                  liveInputV, livePoV, liveStdV * 1000.0f);
    Serial.printf("ADS A1 TDS: %.5f V | Std %.2f mV\n", liveTdsV, liveTdsStdV * 1000.0f);
    if (ecCalibrated) Serial.printf("EC (25C angen.): %.3f mS/cm\n", ecFromVoltage(liveTdsV));
    else Serial.println(F("EC: NICHT KALIBRIERT (e = 1413 uS/cm Referenz erfassen)"));
    if (PH_TO_WIRED) Serial.printf("ADS A2 TO: %.5f V (roh, keine Celsius-Kennlinie)\n", liveToV);
    else Serial.println(F("ADS A2 TO: NOCH NICHT VERKABELT/GEPRUEFT"));
  }
  Serial.printf("PH4502C DO GPIO32: %s\n", PH_DO_WIRED ? (liveDoHigh ? "HIGH" : "LOW") : "NICHT VERKABELT/DEAKTIVIERT");
  Serial.printf("DS18B20 GPIO23: %u Geraet(e)\n", static_cast<unsigned>(oneWireCount));
  if (validTemperature(liveTempC)) Serial.printf(" DS #1: %.2f C\n", liveTempC);
  else Serial.println(F(" DS #1: NICHT ERKANNT"));
  if (oneWireCount > 1) {
    if (validTemperature(liveTemp2C)) Serial.printf(" DS #2: %.2f C\n", liveTemp2C);
    else Serial.println(F(" DS #2: NICHT ERKANNT"));
  }
  for (uint8_t i = 0; i < 3; ++i) {
    if (air[i].ok) Serial.printf("DHT22 #%u: %.2f C, %.1f %%\n",
                              i + 1, air[i].tempC, air[i].humidity);
    else Serial.printf("DHT22 #%u: NICHT ERKANNT / NOCH NICHT VERKABELT\n", i + 1);
  }
  if (bhOk && isfinite(bhLux)) Serial.printf("BH1750: %.0f lux\n", bhLux);
  else Serial.println(F("BH1750: NICHT ERKANNT / NOCH NICHT VERKABELT"));
  Serial.println(F("SSR1-4: AUS (kein Aktor-Test)"));
}

static bool calibrationReady() {
  return havePh7 &&
         havePh4 &&
         !isnan(ph7InputV) &&
         !isnan(ph4InputV) &&
         fabsf(ph4InputV - ph7InputV) >= MIN_CAL_POINT_DISTANCE_V;
}

static float calibrationSlope() {
  if (!calibrationReady()) return NAN;
  return (4.0f - 7.0f) / (ph4InputV - ph7InputV);
}

static float calibrationOffset() {
  const float slope = calibrationSlope();
  if (isnan(slope)) return NAN;
  return 7.0f - slope * ph7InputV;
}

static float phFromInputVoltage(float inputV) {
  const float slope = calibrationSlope();
  const float offset = calibrationOffset();
  if (isnan(slope) || isnan(offset) || isnan(inputV)) return NAN;
  return slope * inputV + offset;
}

static void updateTemperature(bool force = false) {
  const unsigned long now = millis();
  if (!force && now - lastTempMs < TEMP_INTERVAL_MS) return;
  lastTempMs = now;

  temperatureSensors.requestTemperatures();
  oneWireCount = temperatureSensors.getDeviceCount();
  const float t = oneWireCount > 0 ? temperatureSensors.getTempCByIndex(0) : NAN;
  const float t2 = oneWireCount > 1 ? temperatureSensors.getTempCByIndex(1) : NAN;
  liveTempC = validTemperature(t) ? t : NAN;
  liveTemp2C = validTemperature(t2) ? t2 : NAN;
  tempSensorPresent = validTemperature(liveTempC);
}

static bool loadCalibration() {
  if (!preferences.begin(NVS_NAMESPACE, true)) {
    havePh7 = false;
    havePh4 = false;
    ph7InputV = NAN;
    ph4InputV = NAN;
    ph7TempC = NAN;
    ph4TempC = NAN;
    return false;
  }

  havePh7 = preferences.getBool(NVS_HAVE_7, false);
  havePh4 = preferences.getBool(NVS_HAVE_4, false);
  ph7InputV = preferences.getFloat(NVS_VOLT_7, NAN);
  ph4InputV = preferences.getFloat(NVS_VOLT_4, NAN);
  ph7TempC = preferences.getFloat(NVS_TEMP_7, NAN);
  ph4TempC = preferences.getFloat(NVS_TEMP_4, NAN);
  preferences.end();

  if (havePh7 && isnan(ph7InputV)) havePh7 = false;
  if (havePh4 && isnan(ph4InputV)) havePh4 = false;
  return true;
}

static bool saveCalibration() {
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;

  bool ok = true;

  if (havePh7) {
    ok = ok && (preferences.putFloat(NVS_VOLT_7, ph7InputV) == sizeof(float));
    ok = ok && (preferences.putFloat(NVS_TEMP_7, ph7TempC) == sizeof(float)); // NAN = nicht gemessen
  } else {
    if (preferences.isKey(NVS_VOLT_7)) ok = ok && preferences.remove(NVS_VOLT_7);
    if (preferences.isKey(NVS_TEMP_7)) ok = ok && preferences.remove(NVS_TEMP_7);
  }

  if (havePh4) {
    ok = ok && (preferences.putFloat(NVS_VOLT_4, ph4InputV) == sizeof(float));
    ok = ok && (preferences.putFloat(NVS_TEMP_4, ph4TempC) == sizeof(float));
  } else {
    if (preferences.isKey(NVS_VOLT_4)) ok = ok && preferences.remove(NVS_VOLT_4);
    if (preferences.isKey(NVS_TEMP_4)) ok = ok && preferences.remove(NVS_TEMP_4);
  }

  ok = ok && (preferences.putBool(NVS_HAVE_7, havePh7) == sizeof(bool));
  ok = ok && (preferences.putBool(NVS_HAVE_4, havePh4) == sizeof(bool));
  preferences.end();

  if (!ok || !preferences.begin(NVS_NAMESPACE, true)) return false;

  const bool storedHave7 = preferences.getBool(NVS_HAVE_7, false);
  const bool storedHave4 = preferences.getBool(NVS_HAVE_4, false);
  const float storedPh7 = preferences.getFloat(NVS_VOLT_7, NAN);
  const float storedPh4 = preferences.getFloat(NVS_VOLT_4, NAN);
  const float storedTemp7 = preferences.getFloat(NVS_TEMP_7, NAN);
  const float storedTemp4 = preferences.getFloat(NVS_TEMP_4, NAN);
  preferences.end();

  if (storedHave7 != havePh7 || storedHave4 != havePh4) return false;
  if (havePh7 &&
      (isnan(storedPh7) || fabsf(storedPh7 - ph7InputV) > 0.000001f ||
       !(isnan(storedTemp7) && isnan(ph7TempC)) &&
       !(isfinite(storedTemp7) && isfinite(ph7TempC) &&
         fabsf(storedTemp7 - ph7TempC) <= 0.01f))) {
    return false;
  }
  if (havePh4 &&
      (isnan(storedPh4) || fabsf(storedPh4 - ph4InputV) > 0.000001f ||
       !(isnan(storedTemp4) && isnan(ph4TempC)) &&
       !(isfinite(storedTemp4) && isfinite(ph4TempC) &&
         fabsf(storedTemp4 - ph4TempC) <= 0.01f))) {
    return false;
  }

  return true;
}

static bool clearCalibration() {
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool ok = preferences.clear();
  preferences.end();

  if (!ok) return false;

  havePh7 = false;
  havePh4 = false;
  ph7InputV = NAN;
  ph4InputV = NAN;
  ph7TempC = NAN;
  ph4TempC = NAN;
  return true;
}

static void printHelp() {
  Serial.println();
  Serial.println(F("=== Grow-Controller Hardwaretest + pH-Kalibrierung ==="));
  Serial.println(F("ADS1115: A0 = PO pH, A1 = TDS, A2 = TO (roh); SDA D21, SCL D22"));
  Serial.println(F("PH4502C DO: GPIO32 (nur 3.3V-sicher, anfangs deaktiviert)"));
  Serial.println(F("DS18B20: D23 | DHT22: D25/26/27 | BH1750: D13/14"));
  Serial.println(F("OLED: 0x3C, D21/D22; SSR-Ausgaenge bleiben AUS"));
  Serial.println(F("7 = pH-7-Punkt erfassen + im NVS speichern"));
  Serial.println(F("4 = pH-4-Punkt erfassen + im NVS speichern"));
  Serial.println(F("s = Status + fertige Kalibrierwerte"));
  Serial.println(F("r = gespeicherte Kalibrierpunkte loeschen"));
  Serial.println(F("t = Hardware-Status (alle Sensoren)"));
  Serial.println(F("e = EC kalibrieren: A1 in 1413 uS/cm Referenzloesung @25 C"));
  Serial.println(F("h = Hilfe"));
  Serial.println(F("Instabile Messungen >10 mV werden nicht gespeichert. Temperatur optional."));
  Serial.println(F("----------------------------------------"));
}

static void printCalibration() {
  Serial.println();
  Serial.println(F("--- Kalibrierstatus ---"));
  if (ecCalibrated) Serial.printf("EC: kalibriert 1413uS/cm | A1 %.5f V | Faktor %.5f\n", ecReferenceInputV, ecFactor);
  else Serial.println(F("EC: NICHT KALIBRIERT"));

  if (havePh7) {
    Serial.printf("pH 7: ADS A0 = %.6f V | PH4502C PO ~= %.6f V | T = %.2f C\n",
                  ph7InputV, ph7InputV * DIVIDER_RECONSTRUCT, ph7TempC);
  } else {
    Serial.println(F("pH 7: noch nicht erfasst"));
  }

  if (havePh4) {
    Serial.printf("pH 4: ADS A0 = %.6f V | PH4502C PO ~= %.6f V | T = %.2f C\n",
                  ph4InputV, ph4InputV * DIVIDER_RECONSTRUCT, ph4TempC);
  } else {
    Serial.println(F("pH 4: noch nicht erfasst"));
  }

  if (calibrationReady()) {
    const float slope = calibrationSlope();
    const float offset = calibrationOffset();

    Serial.println();
    Serial.println(F("Werte fuer die Produktions-Messkette am gleichen 10k/20k-Knoten:"));
    Serial.printf("static constexpr float PH_CAL_VOLT_4 = %.6ff;\n", ph4InputV);
    Serial.printf("static constexpr float PH_CAL_VOLT_7 = %.6ff;\n", ph7InputV);

    Serial.println();
    Serial.println(F("Kalibriergerade bezogen auf die Spannung hinter dem Teiler:"));
    Serial.printf("PH_SLOPE  = %.6ff;\n", slope);
    Serial.printf("PH_OFFSET = %.6ff;\n", offset);
    Serial.println(F("Formel: pH = PH_SLOPE * input_voltage + PH_OFFSET"));
    Serial.println(F("Puffer-Temperaturkorrektur: nicht automatisch; Produkt-Tabelle verwenden."));
  } else if (havePh7 && havePh4) {
    Serial.println(F("FEHLER: Kalibrierpunkte liegen zu dicht beieinander."));
  }

  Serial.println(F("-----------------------"));
}

static bool captureStablePoint(float targetPh) {
  if (!adsOk) {
    Serial.println(F("ABGELEHNT: ADS1115 nicht erkannt."));
    return false;
  }
  updateTemperature(true);
  Serial.printf("\nErfasse pH %.0f; Sonde ruhig lassen.\n", targetPh);
  if (!validTemperature(liveTempC))
    Serial.println(F("HINWEIS: Temperatur nicht verfuegbar; Puffer-pH nach Herstellerangabe/Referenztemperatur bewerten."));

  const SampleStats s = sampleChannel(0, CAPTURE_SAMPLE_COUNT, SAMPLE_DELAY_MS);
  if (!isfinite(s.meanV) || !isfinite(s.stdV)) {
    Serial.println(F("ABGELEHNT: ADS1115-Auslesefehler."));
    return false;
  }

  updateTemperature(true); // fehlende DS18B20-Temperatur blockiert die Kalibrierung nicht

  Serial.printf("Mittelwert ADS A0: %.6f V\n", s.meanV);
  Serial.printf("Std-Abw.:          %.6f V\n", s.stdV);
  Serial.printf("PH4502C PO:        %.6f V (rueckgerechnet)\n",
                s.meanV * DIVIDER_RECONSTRUCT);
  Serial.printf("Temperatur:        %.2f C\n", liveTempC);
  Serial.printf("mV min/max:        %lu / %lu\n",
                static_cast<unsigned long>(s.minMv),
                static_cast<unsigned long>(s.maxMv));

  if (s.stdV > CAPTURE_MAX_STD_V) {
    Serial.println(F("ABGELEHNT: Messung schwankt >10 mV. Punkt wurde NICHT gespeichert."));
    return false;
  }

  const bool oldHave7 = havePh7;
  const bool oldHave4 = havePh4;
  const float oldPh7 = ph7InputV;
  const float oldPh4 = ph4InputV;
  const float oldTemp7 = ph7TempC;
  const float oldTemp4 = ph4TempC;

  if (targetPh == 7.0f) {
    ph7InputV = s.meanV;
    ph7TempC = liveTempC;
    havePh7 = true;
  } else if (targetPh == 4.0f) {
    ph4InputV = s.meanV;
    ph4TempC = liveTempC;
    havePh4 = true;
  } else {
    Serial.println(F("FEHLER: unbekannter Kalibrierpunkt."));
    return false;
  }

  if (!saveCalibration()) {
    havePh7 = oldHave7;
    havePh4 = oldHave4;
    ph7InputV = oldPh7;
    ph4InputV = oldPh4;
    ph7TempC = oldTemp7;
    ph4TempC = oldTemp4;
    Serial.println(F("FEHLER: NVS-Speicherung fehlgeschlagen; alter Zustand wiederhergestellt."));
    return false;
  }

  Serial.println(F("OK: Kalibrierpunkt dauerhaft gespeichert und verifiziert."));
  printCalibration();
  return true;
}

static void updateLive() {
  const SampleStats s = sampleChannel(0, LIVE_SAMPLE_COUNT, 0);
  liveInputV = s.meanV;
  livePoV = isfinite(liveInputV) ? liveInputV * DIVIDER_RECONSTRUCT : NAN;
  liveStdV = s.stdV;
  const SampleStats tdsSample = sampleChannel(1, EC_LIVE_SAMPLE_COUNT, 5);
  liveTdsV = tdsSample.meanV;
  liveTdsStdV = tdsSample.stdV;
  liveToV = adsOk && PH_TO_WIRED ? ads.computeVolts(ads.readADC_SingleEnded(2)) : NAN;
  if (PH_DO_WIRED) liveDoHigh = digitalRead(PIN_PH_DO) == HIGH;
}

static void drawOled() {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("HW ADS:"));
  display.print(adsOk ? F("OK") : F("--"));
  display.print(F(" DS:"));
  display.print(oneWireCount);
  display.setCursor(0, 10);
  display.print(F("pH A0:"));
  if (isfinite(liveInputV)) display.print(liveInputV, 4);
  else display.print(F("--"));
  display.setCursor(0, 20);
  display.print(F("TO A2:"));
  if (isfinite(liveToV)) display.print(liveToV, 4);
  else display.print(F("--"));
  display.setCursor(0, 30);
  display.print(F("DS:"));
  if (validTemperature(liveTempC)) display.print(liveTempC, 1);
  else display.print(F("--"));
  display.print(F("C "));
  if (calibrationReady() && isfinite(liveInputV)) {
    display.print(F("pH:"));
    display.print(phFromInputVoltage(liveInputV), 2);
  } else display.print(F("pH:--"));
  display.setCursor(0, 40);
  uint8_t dhtCount = 0;
  for (uint8_t i = 0; i < 3; ++i) if (air[i].ok) ++dhtCount;
  display.print(F("DHT:"));
  display.print(dhtCount);
  display.print(F("/3 BH:"));
  display.print(bhOk && isfinite(bhLux) ? F("OK") : F("--"));
  display.setCursor(0, 52);
  display.print(F("pH7:"));
  display.print(havePh7 ? F("OK") : F("--"));
  display.print(F(" pH4:"));
  display.print(havePh4 ? F("OK") : F("--"));
  display.display();
}

static void handleSerial() {
  while (Serial.available()) {
    const char c = Serial.read();

    if (c == '7') {
      captureStablePoint(7.0f);
    } else if (c == '4') {
      captureStablePoint(4.0f);
    } else if (c == 'e' || c == 'E') {
      captureEc1413();
    } else if (c == 's' || c == 'S') {
      printCalibration();
    } else if (c == 't' || c == 'T') {
      printHardwareStatus();
    } else if (c == 'r' || c == 'R') {
      if (clearCalibration()) {
        Serial.println(F("Kalibrierpunkte aus RAM und NVS geloescht."));
      } else {
        Serial.println(F("FEHLER: NVS konnte nicht geloescht werden."));
      }
      printCalibration();
    } else if (c == 'h' || c == 'H' || c == '?') {
      printHelp();
    }
  }
}

void setup() {
  // Kleinspannungs-SSR-Ausgaenge AUS halten; es wird kein Lasttest durchgefuehrt.
  const uint8_t ssrPins[] = {
      PIN_SSR_LIGHT, PIN_SSR_CIRC, PIN_SSR_EXH, PIN_SSR_HEAT
  };
  for (uint8_t pin : ssrPins) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }

  Serial.begin(115200);
  delay(400);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL, 100000);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 15);
    display.println(F("Grow HW Test"));
    display.println(F("ADS / DHT / DS / BH"));
    display.display();
  }

  ads.setGain(GAIN_ONE);
  adsOk = ads.begin(ADS_ADDR, &Wire);

  bhWire.begin(PIN_BH_SDA, PIN_BH_SCL, 50000);
  scanBh1750();
  dht1.begin(); dht2.begin(); dht3.begin();
  if (PH_DO_WIRED) pinMode(PIN_PH_DO, INPUT);

  temperatureSensors.begin();
  oneWireCount = temperatureSensors.getDeviceCount();
  Serial.printf("1-Wire: %u Geraet(e) gefunden.\n", static_cast<unsigned>(oneWireCount));
  for (uint8_t i = 0; i < oneWireCount; ++i) {
    DeviceAddress address;
    if (temperatureSensors.getAddress(address, i)) {
      Serial.printf("1-Wire Sensor %u ROM: ", static_cast<unsigned>(i));
      for (uint8_t j = 0; j < 8; ++j) {
        Serial.printf("%02X", address[j]);
        if (j < 7) Serial.print(":");
      }
      Serial.println();
    }
  }
  temperatureSensors.setResolution(10);
  updateTemperature(true);
  updateOptionalSensors(true);
  updateLive();

  loadEcCalibration();
  const bool nvsReadable = loadCalibration();
  printHelp();
  if (!nvsReadable) Serial.println(F("WARNUNG: ADS-Kalibrierspeicher nicht lesbar."));
  if (!adsOk) Serial.println(F("WARNUNG: ADS1115 auf 0x48 nicht erkannt."));
  if (!oledOk) Serial.println(F("WARNUNG: SSD1306-OLED auf 0x3C nicht erkannt."));
  if (!tempSensorPresent) Serial.println(F("WARNUNG: DS18B20 auf D23 nicht erkannt."));
  if (!bhOk) Serial.println(F("BH1750 noch nicht angeschlossen oder nicht erkannt."));
  if (havePh7 || havePh4) printCalibration();
  printHardwareStatus();
  Serial.println(F("Kalibrieren: nach stabilem LIVE-Wert '7' bzw. '4' senden."));
}

void loop() {
  handleSerial();
  updateTemperature();
  updateOptionalSensors();
  updateLive();

  const unsigned long now = millis();
  static unsigned long lastPrint = 0;
  if (now - lastPrint >= 1000) {
    lastPrint = now;
    Serial.printf("LIVE | ADS:%s | A0 pH %.5f V | PO %.5f V | std %.2f mV | A1 TDS %.5f V",
                  adsOk ? "OK" : "--", liveInputV, livePoV,
                  liveStdV * 1000.0f, liveTdsV);
    if (validTemperature(liveTempC)) Serial.printf(" | DS %.2f C", liveTempC);
    else Serial.print(F(" | DS --"));
    if (calibrationReady() && isfinite(liveInputV)) {
      Serial.printf(" | pH %.3f", phFromInputVoltage(liveInputV));
    }
    uint8_t n = 0;
    for (uint8_t i = 0; i < 3; ++i) if (air[i].ok) ++n;
    if (ecCalibrated) Serial.printf(" | EC %.3f mS/cm (25C)", ecFromVoltage(liveTdsV));
    else Serial.print(F(" | EC -- (nicht kalibriert)"));
    Serial.printf(" | TO A2 %.4f V | DO %s", liveToV, PH_DO_WIRED ? (liveDoHigh ? "HIGH" : "LOW") : "--");
    Serial.printf(" | DHT %u/3 | BH %s",
                  static_cast<unsigned>(n),
                  bhOk && isfinite(bhLux) ? "OK" : "--");
    Serial.println();
  }
  if (now - lastOledMs >= OLED_INTERVAL_MS) {
    lastOledMs = now;
    drawOled();
  }
  delay(10);
}
