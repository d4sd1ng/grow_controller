/**
 * Hardware-Diagnose und pH-Kalibrierung auf dem EINZELNEN ESP32.
 * Aufgebaut: OLED GPIO21/22, ADS1115 A0 pH / A1 TDS, DS18B20 GPIO23.
 * Noch nicht angeschlossene Sensoren (3x DHT22 GPIO25/26/27,
 * BH1750 GPIO13/14) werden als FEHLEND angezeigt, ohne die anderen
 * Tests und die pH-Kalibrierung zu blockieren.
 *
 * Sensor-ESP32 hat KEINE Relais-Ausgaenge; separater ESP32-C3 SuperMini.
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// Optional: MLX90640 nach Lieferung + Adafruit_MLX90640-Library aktivieren.
#ifndef GROW_ENABLE_MLX90640
#define GROW_ENABLE_MLX90640 0
#endif
#if GROW_ENABLE_MLX90640
#include <Adafruit_MLX90640.h>
#endif

static constexpr uint8_t PIN_DS18B20 = 23;
static constexpr uint8_t PIN_PH_DO = 32;
static constexpr bool PH_DO_WIRED = false; // erst nach 3.3-V-Pegelpruefung aktivieren
// TO erst aktivieren, nachdem sein maximaler Ausgangspegel nachweislich <= ADS-VDD (3.3 V) ist.
static constexpr bool PH_TO_WIRED = false;
static constexpr uint8_t PIN_OLED_SDA = 21;
static constexpr uint8_t PIN_OLED_SCL = 22;
static constexpr uint8_t PIN_BH_SDA = 13;
static constexpr uint8_t PIN_BH_SCL = 14;
static constexpr uint8_t PIN_DHT1 = 25;
static constexpr uint8_t PIN_DHT2 = 26;
static constexpr uint8_t PIN_DHT3 = 27;
static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr uint8_t ADS_ADDR = 0x48;

static constexpr float PH7_PO_REFERENCE_V = 2.50f; // vorgegebene PH4502C-PO-Referenz bei pH 7

static constexpr uint16_t LIVE_SAMPLE_COUNT = 32;
static constexpr uint16_t CAPTURE_SAMPLE_COUNT = 160;
static constexpr uint16_t EC_LIVE_SAMPLE_COUNT = 30; // CQRobot: 30 samples
static constexpr uint8_t EC_CAPTURE_WINDOWS = 8;
static constexpr uint16_t EC_CAPTURE_WINDOW_GAP_MS = 200;
static constexpr unsigned long EC_SAMPLE_PERIOD_MS = 40; // CQRobot: sample once each 40 ms
static constexpr float EC_REFERENCE_MS_CM = 1.413f; // 1413 uS/cm bei 25 C
static constexpr float EC_MAX_STD_V = 0.010f;  // Streuung der Median-Fenster
static constexpr float EC_MAX_SPAN_V = 0.030f; // Spanne der Median-Fenster
static constexpr uint16_t SAMPLE_DELAY_MS = 20;
static constexpr unsigned long TEMP_INTERVAL_MS = 2000;
static constexpr unsigned long OLED_INTERVAL_MS = 500;

static constexpr float PH_CAL_WATER_TEMP_C = 21.4f; // vom Nutzer gemessene Temperatur fuer beide pH-Puffer
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
static constexpr char TO_NVS_NAMESPACE[] = "toads_v1";
static constexpr char WATER_ROM_NVS_NAMESPACE[] = "waterds_v1";
static constexpr float TO_CAL_MIN_TEMP_SPAN_C = 5.0f;
static constexpr float TO_CAL_MIN_VOLT_SPAN_V = 0.025f;

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
bool mlx90640Ok = false;
float thermalRoiTempC = NAN; // nur mittlere Bildregion, nicht als Blatt verifiziert
float leafVpdCandidate = NAN;
float liveInputV = NAN;
float liveStdV = NAN;
float liveTdsV = NAN;
float liveTdsStdV = NAN;
float ecFactor = NAN;
float ecReferenceInputV = NAN;
bool ecCalibrated = false;
float liveToV = NAN;
float liveToStdV = NAN;
bool liveDoHigh = false;
float toPointV[2] = {NAN, NAN};
float toPointC[2] = {NAN, NAN};
bool toPointSet[2] = {false, false};
bool toCalibrated = false;
bool toNewPoint1Captured = false; // Session: neuer Punkt 1 erforderlich vor Punkt 2
float ecCalTempC = NAN;
float liveWaterTempC = NAN;
uint8_t waterSensorRom[8] = {};
bool waterSensorRomSelected = false;
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

/* Dieselbe Median-Ermittlung fuer LIVE und EC-Kalibrierung.
 * Nur A1: pH auf ADS A0 bleibt beim bisherigen Mittelwertverfahren.
 * Die Rohwertstreuung wird weiter ausgewiesen, statt verschwiegen.
 */
struct EcWindowStats {
  float medianV;
  float rawMinV;
  float rawMaxV;
  float rawStdV;
};
static EcWindowStats sampleEcWindow() {
  const EcWindowStats invalid{NAN, NAN, NAN, NAN};
  if (!adsOk) return invalid;
  float points[EC_LIVE_SAMPLE_COUNT];
  double sum = 0.0, sumSq = 0.0;
  float minV = INFINITY, maxV = -INFINITY;
  for (uint8_t i = 0; i < EC_LIVE_SAMPLE_COUNT; ++i) {
    const unsigned long sampleStartMs = millis();
    const float v = ads.computeVolts(ads.readADC_SingleEnded(1));
    if (!isfinite(v) || v < 0.0f || v > 2.3f) return invalid;
    points[i] = v;
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;
    sum += v;
    sumSq += static_cast<double>(v) * v;
    // Abtastperiode einschliesslich ADS1115-Wandlungszeit.
    while (millis() - sampleStartMs < EC_SAMPLE_PERIOD_MS) delay(1);
  }
  for (uint8_t i = 1; i < EC_LIVE_SAMPLE_COUNT; ++i) {
    float key = points[i];
    uint8_t j = i;
    while (j > 0 && points[j - 1] > key) {
      points[j] = points[j - 1];
      --j;
    }
    points[j] = key;
  }
  double variance = sumSq / EC_LIVE_SAMPLE_COUNT -
                    (sum / EC_LIVE_SAMPLE_COUNT) * (sum / EC_LIVE_SAMPLE_COUNT);
  if (variance < 0.0) variance = 0.0;
  const float medianV = 0.5f * (points[EC_LIVE_SAMPLE_COUNT / 2 - 1] +
                                  points[EC_LIVE_SAMPLE_COUNT / 2]);
  return EcWindowStats{medianV, minV, maxV,
                       static_cast<float>(sqrt(variance))};
}

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
// Gleiche 2%-pro-C-Kompensation wie CQRobot (Spannung vor Kennlinie korrigieren).
// KEINE 25-C-Ersatztemperatur. Fehlt DS18B20, bleibt EC ungueltig.
static float ecCompensatedVoltage(float voltageV, float waterTempC) {
  if (!isfinite(voltageV) || !validTemperature(waterTempC)) return NAN;
  const float coefficient = 1.0f + 0.02f * (waterTempC - 25.0f);
  return coefficient > 0.1f ? voltageV / coefficient : NAN;
}
static float ecUncalibratedMs(float v) {
  if (!isfinite(v) || v < 0.0f || v > 2.3f) return NAN;
  return (133.42f*v*v*v - 255.86f*v*v + 857.39f*v) / 1000.0f;
}
static float ecFromVoltage(float v, float waterTempC) {
  const float base = ecUncalibratedMs(ecCompensatedVoltage(v, waterTempC));
  return ecCalibrated && isfinite(base) ? base * ecFactor : NAN;
}
static void loadEcCalibration() {
  ecCalibrated = false;
  Preferences ecPrefs;
  if (!ecPrefs.begin(EC_NAMESPACE, true)) return;
  const bool valid = ecPrefs.getBool("valid", false);
  ecFactor = ecPrefs.getFloat(EC_NVS_FACTOR, NAN);
  ecReferenceInputV = ecPrefs.getFloat(EC_NVS_REFERENCE_V, NAN);
  ecCalTempC = ecPrefs.getFloat("temp_c", NAN);
  ecPrefs.end();
  ecCalibrated = valid && isfinite(ecFactor) &&
                 ecFactor >= 0.1f && ecFactor <= 10.0f &&
                 isfinite(ecReferenceInputV) && ecReferenceInputV > 0.05f &&
                 ecReferenceInputV <= 2.3f;
}
static void updateTemperature(bool force); // forward declaration for EC reference capture
static bool captureEc1413() {
  if (!adsOk) { Serial.println(F("EC ABGELEHNT: ADS1115 fehlt.")); return false; }
  updateTemperature(true);
  if (!validTemperature(liveWaterTempC)) {
    Serial.println(F("EC NICHT kalibriert: Wasser-DS18B20 fehlt oder Zuordnung unklar; alter Faktor bleibt erhalten."));
    return false;
  }
  Serial.printf("EC Referenz 1413 uS/cm bei 25 C | Wasser %.2f C | 8 Medianfenster, 30x40ms ...\n", liveWaterTempC);
  double sum = 0.0, sumSq = 0.0;
  float minMedianV = INFINITY, maxMedianV = -INFINITY;
  float rawMinV = INFINITY, rawMaxV = -INFINITY;
  for (uint8_t i = 0; i < EC_CAPTURE_WINDOWS; ++i) {
    const EcWindowStats w = sampleEcWindow();
    if (!isfinite(w.medianV)) {
      Serial.println(F("EC NICHT gespeichert: ADC-Einzelmessung ungueltig."));
      return false;
    }
    sum += w.medianV;
    sumSq += static_cast<double>(w.medianV) * w.medianV;
    if (w.medianV < minMedianV) minMedianV = w.medianV;
    if (w.medianV > maxMedianV) maxMedianV = w.medianV;
    if (w.rawMinV < rawMinV) rawMinV = w.rawMinV;
    if (w.rawMaxV > rawMaxV) rawMaxV = w.rawMaxV;
    Serial.printf("  Fenster %u/%u: Median %.5f V | Roh-Std %.1f mV\n",
                  static_cast<unsigned>(i + 1),
                  static_cast<unsigned>(EC_CAPTURE_WINDOWS),
                  w.medianV, w.rawStdV * 1000.0f);
    if (i + 1 < EC_CAPTURE_WINDOWS) delay(EC_CAPTURE_WINDOW_GAP_MS);
  }
  const float meanMedianV = static_cast<float>(sum / EC_CAPTURE_WINDOWS);
  double var = sumSq / EC_CAPTURE_WINDOWS -
      (sum / EC_CAPTURE_WINDOWS) * (sum / EC_CAPTURE_WINDOWS);
  if (var < 0.0) var = 0.0;
  const float medianStdV = static_cast<float>(sqrt(var));
  const float medianSpanV = maxMedianV - minMedianV;
  Serial.printf("EC A1 gefiltert: %.5f V | Std %.2f mV | Spanne %.3f V\n",
                meanMedianV, medianStdV * 1000.0f, medianSpanV);
  Serial.printf("EC A1 Rohbereich: %.3f-%.3f V (Spanne %.3f V)\n",
                rawMinV, rawMaxV, rawMaxV - rawMinV);
  if (rawMaxV - rawMinV > 0.090f) {
    Serial.println(F("HINWEIS: grosse Rohwertschwankung; Versorgung, gemeinsame GND-Verbindung und Sondenlage ggf. pruefen."));
  }
  if (!isfinite(meanMedianV) || !isfinite(medianStdV) ||
      medianStdV > EC_MAX_STD_V || medianSpanV > EC_MAX_SPAN_V) {
    Serial.println(F("EC NICHT gespeichert: auch gefilterte Fenster sind instabil (Std >10 mV oder Spanne >30 mV)."));
    return false;
  }
  const float ecRaw = ecUncalibratedMs(ecCompensatedVoltage(meanMedianV, liveWaterTempC));
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
  ok = (ecPrefs.putFloat(EC_NVS_REFERENCE_V, meanMedianV) == sizeof(float)) && ok;
  ok = (ecPrefs.putFloat("temp_c", liveWaterTempC) == sizeof(float)) && ok;
  ok = (ecPrefs.putBool("valid", true) == sizeof(bool)) && ok;
  ecPrefs.end();
  if (!ok) { Serial.println(F("EC NICHT gespeichert: NVS Schreibfehler.")); return false; }
  loadEcCalibration();
  if (!ecCalibrated || fabsf(ecFactor - factor) > 0.00001f) {
    Serial.println(F("EC NICHT bestaetigt: NVS Lesefehler."));
    return false;
  }
  Serial.printf("EC KALIBRIERT: 1.413 mS/cm bei 25 C | A1 %.5f V | T-Wasser %.2f C | Faktor %.5f\n",
                ecReferenceInputV, ecCalTempC, ecFactor);
  Serial.println(F("EC: Wassertemperatur des DS18B20 wurde fuer Kalibrierung erfasst."));
  return true;
}

static void loadToCalibration() {
  toCalibrated = false;
  toNewPoint1Captured = false;
  Preferences p;
  if (!p.begin(TO_NVS_NAMESPACE, true)) return;
  const bool valid = p.getBool("valid", false);
  const float v1 = p.getFloat("v1", NAN), v2 = p.getFloat("v2", NAN);
  const float c1 = p.getFloat("c1", NAN), c2 = p.getFloat("c2", NAN);
  p.end();
  if (!valid || !isfinite(v1) || !isfinite(v2) ||
      !isfinite(c1) || !isfinite(c2) ||
      v1 <= 0.05f || v1 >= 3.25f || v2 <= 0.05f || v2 >= 3.25f ||
      c1 < -5.0f || c1 > 60.0f || c2 < -5.0f || c2 > 60.0f ||
      fabsf(v1 - v2) < TO_CAL_MIN_VOLT_SPAN_V ||
      fabsf(c1 - c2) < TO_CAL_MIN_TEMP_SPAN_C) return;
  toPointV[0] = v1; toPointV[1] = v2;
  toPointC[0] = c1; toPointC[1] = c2;
  toPointSet[0] = toPointSet[1] = true;
  toCalibrated = true;
}
static float toCaseTempC(float v) {
  if (!PH_TO_WIRED || !toCalibrated || !isfinite(v)) return NAN;
  if (v < fminf(toPointV[0], toPointV[1]) ||
      v > fmaxf(toPointV[0], toPointV[1])) return NAN;
  const float t = toPointC[0] + (v - toPointV[0]) *
      (toPointC[1] - toPointC[0]) / (toPointV[1] - toPointV[0]);
  return t >= -5.0f && t <= 60.0f ? t : NAN;
}
static bool captureToPoint(uint8_t point, float referenceC) {
  if (!PH_TO_WIRED || !adsOk) {
    Serial.println(F("TO NICHT AKTIV: A2 erst nach nachgewiesener 3.3-V-Pegelsicherheit freigeben."));
    return false;
  }
  if (point < 1 || point > 2 || !isfinite(referenceC) ||
      referenceC < -5.0f || referenceC > 60.0f) {
    Serial.println(F("TO FEHLER: Referenztemperatur -5..60 C erforderlich."));
    return false;
  }
  const SampleStats st = sampleChannel(2, 32, 20);
  if (!isfinite(st.meanV) || !isfinite(st.stdV) ||
      st.meanV <= 0.05f || st.meanV >= 3.25f ||
      st.stdV > 0.010f) {
    Serial.println(F("TO NICHT erfasst: Signal ungueltig, gesaettigt oder instabil."));
    return false;
  }
  // Nie alte NVS-Punkte mit einem neuen Referenzpunkt mischen.
  if (point == 2 && !toNewPoint1Captured) {
    Serial.println(F("TO: erst neuen Punkt 1 (a <C>) erfassen, dann Punkt 2 (b <C>)."));
    return false;
  }
  if (point == 1) {
    toPointSet[0] = toPointSet[1] = false;
    toNewPoint1Captured = true;
  }
  const uint8_t i = point - 1;
  toPointV[i] = st.meanV;
  toPointC[i] = referenceC;
  toPointSet[i] = true;
  toCalibrated = false;
  Serial.printf("TO Punkt %u: A2 %.5f V bei %.2f C (Std %.2f mV)\n",
                static_cast<unsigned>(point), st.meanV, referenceC, st.stdV * 1000.0f);
  if (!toPointSet[0] || !toPointSet[1]) {
    Serial.println(F("TO: zweiter Punkt mit mindestens 5 C Abstand benoetigt."));
    return true;
  }
  toNewPoint1Captured = false;
  if (fabsf(toPointV[0] - toPointV[1]) < TO_CAL_MIN_VOLT_SPAN_V ||
      fabsf(toPointC[0] - toPointC[1]) < TO_CAL_MIN_TEMP_SPAN_C) {
    Serial.println(F("TO NICHT kalibriert: Temperatur- oder Spannungsabstand zu klein."));
    return false;
  }
  Preferences p;
  if (!p.begin(TO_NVS_NAMESPACE, false)) return false;
  bool ok = p.putFloat("v1", toPointV[0]) == sizeof(float);
  ok = (p.putFloat("c1", toPointC[0]) == sizeof(float)) && ok;
  ok = (p.putFloat("v2", toPointV[1]) == sizeof(float)) && ok;
  ok = (p.putFloat("c2", toPointC[1]) == sizeof(float)) && ok;
  ok = (p.putBool("valid", true) == sizeof(bool)) && ok;
  p.end();
  if (!ok) { Serial.println(F("TO NVS-Speicherung fehlgeschlagen.")); return false; }
  loadToCalibration();
  Serial.println(toCalibrated ? F("TO GEHAEUSETEMPERATUR KALIBRIERT") :
                               F("TO NVS-Lesen fehlgeschlagen."));
  return toCalibrated;
}


/* MLX90640 optional auf OLED/ADS-I2C GPIO21/22, 0x33.
 * Reiner Lesetest; keine NVS-Schreibzugriffe und keine Aktoraktionen.
 */
static float vpdSatKpa(float tempC) {
  return 0.61078f * expf(17.27f * tempC / (tempC + 237.3f));
}
static void computeLeafVpdCandidate() {
  leafVpdCandidate = NAN;
  if (!mlx90640Ok || !isfinite(thermalRoiTempC)) return;
  float vp[3];
  uint8_t count = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    if (!air[i].ok) continue;
    vp[count++] = vpdSatKpa(thermalRoiTempC) -
                  vpdSatKpa(air[i].tempC) * air[i].humidity / 100.0f;
  }
  if (count < 2) return;
  for (uint8_t i = 1; i < count; ++i) {
    float x = vp[i];
    uint8_t j = i;
    while (j > 0 && vp[j - 1] > x) { vp[j] = vp[j - 1]; --j; }
    vp[j] = x;
  }
  leafVpdCandidate = count == 3 ? vp[1] : (vp[0] + vp[1]) * 0.5f;
}
#if GROW_ENABLE_MLX90640
static Adafruit_MLX90640 mlx90640;
static float mlxPixels[32 * 24];
static bool mlxReady = false;
static unsigned long lastMlxMs = 0;
static void initMlx90640() {
  Wire.setClock(400000);
  mlxReady = mlx90640.begin(0x33, &Wire);
  if (mlxReady) {
    mlx90640.setMode(MLX90640_CHESS);
    mlx90640.setResolution(MLX90640_ADC_18BIT);
    mlx90640.setRefreshRate(MLX90640_2_HZ);
  }
  Wire.setClock(100000);
  Serial.printf("MLX90640 I2C 0x33: %s\n", mlxReady ? "OK" : "NICHT ERKANNT");
  lastMlxMs = millis();
}
static void sampleMlx90640(unsigned long nowMs) {
  if (!mlxReady || nowMs - lastMlxMs < 10000UL) return;
  lastMlxMs = nowMs;
  Wire.setClock(400000);
  const int err = mlx90640.getFrame(mlxPixels);
  Wire.setClock(100000);
  mlx90640Ok = false;
  thermalRoiTempC = NAN;
  if (err != 0) {
    computeLeafVpdCandidate();
    Serial.printf("MLX90640 Bildfehler: %d\n", err);
    return;
  }
  float roi[192];
  uint16_t n = 0;
  for (uint8_t y = 6; y < 18; ++y) {
    for (uint8_t x = 8; x < 24; ++x) {
      float v = mlxPixels[y * 32 + x];
      if (isfinite(v) && v >= 0.0f && v <= 60.0f) roi[n++] = v;
    }
  }
  if (n < 96) { computeLeafVpdCandidate(); return; }
  for (uint16_t i = 1; i < n; ++i) {
    const float v = roi[i];
    uint16_t j = i;
    while (j > 0 && roi[j - 1] > v) { roi[j] = roi[j - 1]; --j; }
    roi[j] = v;
  }
  thermalRoiTempC = n & 1 ? roi[n / 2] :
                    (roi[n / 2 - 1] + roi[n / 2]) * 0.5f;
  mlx90640Ok = true;
  computeLeafVpdCandidate();
  Serial.printf("MLX90640 ROI: %.2f C | VPD-Blatt-Kandidat %.3f kPa | %u Pixel\n",
                thermalRoiTempC, leafVpdCandidate, static_cast<unsigned>(n));
}
#else
static void initMlx90640() {}
static void sampleMlx90640(unsigned long) {}
#endif

static void printHardwareStatus() {
  Serial.println(F("\n=== HARDWARE-STATUS ==="));
  Serial.printf("OLED 0x3C : %s\n", oledOk ? "OK" : "NICHT ERKANNT");
  Serial.printf("ADS  0x48: %s\n", adsOk ? "OK" : "NICHT ERKANNT");
  if (adsOk) {
    Serial.printf("ADS A0 pH: %.5f V | Std: %.2f mV | PO-Soll bei pH7: %.2f V\n",
                  liveInputV, liveStdV * 1000.0f, PH7_PO_REFERENCE_V);
    Serial.printf("ADS A1 TDS: %.5f V | Std %.2f mV\n", liveTdsV, liveTdsStdV * 1000.0f);
    if (ecCalibrated && validTemperature(liveWaterTempC))
      Serial.printf("EC auf 25C normiert: %.3f mS/cm | DS %.2f C\n",
                    ecFromVoltage(liveTdsV, liveWaterTempC), liveWaterTempC);
    else Serial.println(F("EC: NICHT KALIBRIERT (e = 1413 uS/cm Referenz erfassen)"));
    if (PH_TO_WIRED) {
      Serial.printf("ADS A2 TO: %.5f V | Std %.2f mV\n", liveToV, liveToStdV * 1000.0f);
      if (toCalibrated) Serial.printf("TO Gehaeuse: %.2f C (2-Punkt-Interpolation)\n", toCaseTempC(liveToV));
    }
    else Serial.println(F("ADS A2 TO: NOCH NICHT VERKABELT/GEPRUEFT"));
  }
  Serial.printf("PH4502C DO GPIO32: %s\n", PH_DO_WIRED ? (liveDoHigh ? "HIGH" : "LOW") : "NICHT VERKABELT/DEAKTIVIERT");
  Serial.printf("DS18B20 GPIO23: %u Geraet(e) | Wasser: %.2f C | ROM-Zuordnung: %s\n",
                static_cast<unsigned>(oneWireCount), liveWaterTempC,
                waterSensorRomSelected ? "gespeichert" : (oneWireCount == 1 ? "ein Sensor" : "fehlt"));
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
  Serial.printf("MLX90640 0x33: %s | ROI %.2f C | VPD-Blatt-Kandidat %.3f kPa\n",
                mlx90640Ok ? "OK" : "AUS / NICHT ERKANNT / NOCH NICHT MONTIERT",
                thermalRoiTempC, leafVpdCandidate);
  if (bhOk && isfinite(bhLux)) Serial.printf("BH1750: %.0f lux\n", bhLux);
  else Serial.println(F("BH1750: NICHT ERKANNT / NOCH NICHT VERKABELT"));
  Serial.println(F("Aktoren: ESP32-C3 separat, hier keine Ausgangssignale"));
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

static bool phTemperatureCompensationReady() {
  return calibrationReady() && validTemperature(liveWaterTempC) &&
      validTemperature(ph7TempC) && validTemperature(ph4TempC) &&
      fabsf(ph7TempC - ph4TempC) <= 3.0f;
}
static float phFromInputVoltage(float inputV) {
  const float slope = calibrationSlope();
  if (!isfinite(slope) || !isfinite(inputV)) return NAN;
  float deltaFromPh7 = slope * (inputV - ph7InputV);
  // Nernst-Korrektur der Elektrodensteilheit, nur mit gemessener Kalibriertemperatur.
  if (phTemperatureCompensationReady()) {
    const float calibrationKelvin = 273.15f + 0.5f * (ph7TempC + ph4TempC);
    deltaFromPh7 *= calibrationKelvin / (273.15f + liveWaterTempC);
  }
  return 7.0f + deltaFromPh7;
}

static void loadWaterSensorRom() {
  waterSensorRomSelected = false;
  Preferences p;
  if (!p.begin(WATER_ROM_NVS_NAMESPACE, true)) return;
  if (p.getBytesLength("rom") == 8) {
    p.getBytes("rom", waterSensorRom, 8);
    waterSensorRomSelected =
        waterSensorRom[0] == 0x28 &&
        OneWire::crc8(waterSensorRom, 7) == waterSensorRom[7];
  }
  p.end();
}
static bool selectWaterSensorRom(String raw) {
  raw.trim();
  raw.replace(":", "");
  raw.replace("-", "");
  if (raw.length() != 16) {
    Serial.println(F("Wasser-DS: w 28XXXXXXXXXXXXXX (16 Hex-Zeichen aus 1-Wire-Liste)"));
    return false;
  }
  uint8_t candidate[8];
  for (uint8_t i = 0; i < 8; ++i) {
    const String part = raw.substring(i * 2, i * 2 + 2);
    for (uint8_t j = 0; j < 2; ++j) {
      const char ch = part[j];
      if (!isxdigit(static_cast<unsigned char>(ch))) {
        Serial.println(F("Wasser-DS: ROM enthaelt ungueltiges Hex."));
        return false;
      }
    }
    candidate[i] = static_cast<uint8_t>(strtoul(part.c_str(), nullptr, 16));
  }
  if (candidate[0] != 0x28 || OneWire::crc8(candidate, 7) != candidate[7]) {
    Serial.println(F("Wasser-DS: keine gueltige DS18B20-ROM oder CRC-Fehler."));
    return false;
  }
  bool found = false;
  for (uint8_t i = 0; i < temperatureSensors.getDeviceCount(); ++i) {
    DeviceAddress addr;
    if (temperatureSensors.getAddress(addr, i) &&
        memcmp(addr, candidate, 8) == 0) found = true;
  }
  if (!found) {
    Serial.println(F("Wasser-DS: ROM ist nicht auf dem Bus vorhanden."));
    return false;
  }
  Preferences p;
  if (!p.begin(WATER_ROM_NVS_NAMESPACE, false)) return false;
  const bool saved = p.putBytes("rom", candidate, 8) == 8;
  p.end();
  if (!saved) return false;
  loadWaterSensorRom();
  Serial.println(F("Wasser-DS18B20 nach ROM gespeichert."));
  return waterSensorRomSelected;
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
  // Nur eine vorhandene Sonde: automatisch Wasser. Bei mehreren eindeutige ROM erforderlich.
  if (waterSensorRomSelected) {
    const float tWater = temperatureSensors.getTempC(waterSensorRom);
    liveWaterTempC = validTemperature(tWater) ? tWater : NAN;
  } else {
    liveWaterTempC = oneWireCount == 1 ? liveTempC : NAN;
  }
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
  Serial.println(F("OLED: 0x3C, D21/D22; keine Relais-GPIOs auf Sensor-ESP32"));
  Serial.println(F("7 = pH-7-Punkt erfassen + im NVS speichern"));
  Serial.println(F("4 = pH-4-Punkt erfassen + im NVS speichern"));
  Serial.printf("pH-Puffer-Temperatur: %.1f C | PH4502C PO-Soll bei pH7: %.2f V\n",
                PH_CAL_WATER_TEMP_C, PH7_PO_REFERENCE_V);
  Serial.println(F("s = Status + fertige Kalibrierwerte"));
  Serial.println(F("r = gespeicherte Kalibrierpunkte loeschen"));
  Serial.println(F("t = Hardware-Status (alle Sensoren)"));
  Serial.println(F("MLX90640 optional: I2C 0x33 an D21/D22; 10s-Bild-ROI; keine Kalibrierung"));
  Serial.println(F("p = automatischer Alarm-Selbsttest (PASS/FAIL)"));
  Serial.println(F("1 = TEST pH 5.40 -> Alarm UNTER 5.5 auf OLED/Serial"));
  Serial.println(F("2 = TEST pH 6.60 -> Alarm UEBER 6.5 auf OLED/Serial"));
  Serial.println(F("3 = TEST pH 6.00 -> Normalstatus auf OLED/Serial"));
  Serial.println(F("0 = TEST beenden; echte pH-Messung/Alarmanzeige aktiv"));
  Serial.println(F("e = EC kalibrieren: A1 in 1413 uS/cm Referenzloesung, DS18B20 erforderlich"));
  Serial.println(F("w <ROM> = Wasser-DS18B20 aus mehreren Sensoren anhand 16-stelliger ROM festlegen"));
  Serial.println(F("a <C> = TO/Gehaeuse Referenzpunkt 1 bei C Grad Celsius"));
  Serial.println(F("b <C> = TO/Gehaeuse Referenzpunkt 2 (Abstand >=5 C)"));
  Serial.println(F("h = Hilfe"));
  Serial.println(F("Instabile Messungen >10 mV werden nicht gespeichert. Temperatur optional."));
  Serial.println(F("----------------------------------------"));
}

static void printCalibration() {
  Serial.println();
  Serial.println(F("--- Kalibrierstatus ---"));
  if (ecCalibrated) Serial.printf("EC: Kalibrierfaktor %.5f | Ref A1 %.5f V | Ref-DS %s\n",
                                 ecFactor, ecReferenceInputV,
                                 validTemperature(ecCalTempC) ? "vorhanden" : "ALT: ohne Wasser-DS");
  else Serial.println(F("EC: NICHT KALIBRIERT"));
  if (toCalibrated) Serial.printf("TO: %.2f V=%.1f C, %.2f V=%.1f C\n",
                                  toPointV[0], toPointC[0], toPointV[1], toPointC[1]);
  else Serial.println(F("TO: NICHT KALIBRIERT"));

  if (havePh7) {
    Serial.printf("pH 7: ADS A0 = %.6f V | PO-Soll %.2f V | T = %.2f C\n",
                  ph7InputV, PH7_PO_REFERENCE_V, ph7TempC);
  } else {
    Serial.println(F("pH 7: noch nicht erfasst"));
  }

  if (havePh4) {
    Serial.printf("pH 4: ADS A0 = %.6f V | T = %.2f C\n",
                  ph4InputV, ph4TempC);
  } else {
    Serial.println(F("pH 4: noch nicht erfasst"));
  }

  if (calibrationReady()) {
    const float slope = calibrationSlope();
    const float offset = calibrationOffset();

    Serial.println();
    Serial.println(F("Kalibrierwerte in ADS1115-A0-Eingangsspannung:"));
    Serial.printf("static constexpr float PH_CAL_VOLT_4 = %.6ff;\n", ph4InputV);
    Serial.printf("static constexpr float PH_CAL_VOLT_7 = %.6ff;\n", ph7InputV);

    Serial.println();
    Serial.println(F("Kalibriergerade bezogen auf gemessene ADS1115-A0-Spannung:"));
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
  Serial.printf("\nErfasse pH %.0f bei manuell vorgegebener Puffer-Temperatur %.1f C; Sonde ruhig lassen.\n",
                targetPh, PH_CAL_WATER_TEMP_C);

  const SampleStats s = sampleChannel(0, CAPTURE_SAMPLE_COUNT, SAMPLE_DELAY_MS);
  if (!isfinite(s.meanV) || !isfinite(s.stdV)) {
    Serial.println(F("ABGELEHNT: ADS1115-Auslesefehler."));
    return false;
  }

  Serial.printf("Mittelwert ADS A0: %.6f V\n", s.meanV);
  Serial.printf("Std-Abw.:          %.6f V\n", s.stdV);
  if (targetPh == 7.0f)
    Serial.printf("PH4502C PO-Soll:    %.5f V (pH 7, nicht aus A0 rueckgerechnet)\n",
                  PH7_PO_REFERENCE_V);
  Serial.printf("pH-Puffer-Temp:    %.2f C (manuell)\n", PH_CAL_WATER_TEMP_C);
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
    ph7TempC = PH_CAL_WATER_TEMP_C;
    havePh7 = true;
  } else if (targetPh == 4.0f) {
    ph4InputV = s.meanV;
    ph4TempC = PH_CAL_WATER_TEMP_C;
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
  liveStdV = s.stdV;
  const EcWindowStats tdsSample = sampleEcWindow();
  liveTdsV = tdsSample.medianV;
  liveTdsStdV = tdsSample.rawStdV;
  if (adsOk && PH_TO_WIRED) {
    const SampleStats toSample = sampleChannel(2, 16, 5);
    liveToV = toSample.meanV;
    liveToStdV = toSample.stdV;
  } else {
    liveToV = NAN;
    liveToStdV = NAN;
  }
  if (PH_DO_WIRED) liveDoHigh = digitalRead(PIN_PH_DO) == HIGH;
}

/* Reiner pH-Alarm-Selbsttest: identische Schwellen/Hysterese/Bestaetigung
 * wie growroom_telemetry_client; keine Aenderung an Sensoren, NVS oder Ausgaengen.
 * Testet nur die lokale Zustandslogik, nicht WLAN/CYD/Server-Zustellung.
 */
enum class PhTestAlarm : uint8_t { Normal, Low, High, Fault };
struct PhTestMachine {
  PhTestAlarm alarm = PhTestAlarm::Fault;
  PhTestAlarm pending = PhTestAlarm::Fault;
  uint8_t samples = 0;

  void update(float ph, bool calibrated = true, bool adcOk = true) {
    if (!calibrated || !adcOk || !isfinite(ph) || ph < 0.0f || ph > 14.0f) {
      samples = 0;
      alarm = PhTestAlarm::Fault;
      return;
    }
    PhTestAlarm next = PhTestAlarm::Normal;
    if (ph < 5.5f || (alarm == PhTestAlarm::Low && ph < 5.6f))
      next = PhTestAlarm::Low;
    else if (ph > 6.5f || (alarm == PhTestAlarm::High && ph > 6.4f))
      next = PhTestAlarm::High;
    if (next == alarm) { samples = 0; return; }
    if (next != pending) { pending = next; samples = 1; }
    else if (samples < 3) ++samples;
    if (samples >= 3) { samples = 0; alarm = next; }
  }
};

/* Live-Alarm und manuell ausloesbarer Test verwenden dieselbe Zustandslogik
 * wie der automatisierte p-Selbsttest. Testwerte ersetzen keine Sensorwerte,
 * werden nicht in NVS geschrieben und loesen keine Aktoren aus.
 */
static PhTestMachine livePhAlarm;
static bool phSimulationActive = false;
static float phSimulatedValue = NAN;

static const char *phAlarmLabel(PhTestAlarm a) {
  switch (a) {
    case PhTestAlarm::Normal: return "NORMAL";
    case PhTestAlarm::Low: return "LOW <5.5";
    case PhTestAlarm::High: return "HIGH >6.5";
    default: return "SENSORFEHLER";
  }
}
static void updateLivePhAlarm() {
  const PhTestAlarm old = livePhAlarm.alarm;
  if (phSimulationActive) {
    livePhAlarm.update(phSimulatedValue, true, true);
  } else {
    const float measured = phFromInputVoltage(liveInputV);
    livePhAlarm.update(measured, calibrationReady(), adsOk && isfinite(liveInputV));
  }
  if (old != livePhAlarm.alarm) {
    Serial.printf("%s pH-ALARM: %s\n",
                  phSimulationActive ? "[TEST]" : "[LIVE]",
                  phAlarmLabel(livePhAlarm.alarm));
  }
}
static void setPhSimulation(char command) {
  if (command == '0') {
    phSimulationActive = false;
    phSimulatedValue = NAN;
    livePhAlarm = PhTestMachine{};
    Serial.println(F("[TEST AUS] Reale pH-Messung wieder aktiv; Alarmstatus wird neu ermittelt."));
    updateLivePhAlarm();
    return;
  }
  phSimulatedValue = command == '1' ? 5.40f : command == '2' ? 6.60f : 6.00f;
  phSimulationActive = true;
  livePhAlarm = PhTestMachine{};
  Serial.printf("[TEST EIN] Simulierter pH %.2f; REALER A0-Wert bleibt unveraendert.\n",
                phSimulatedValue);
  for (uint8_t sample = 0; sample < 3; ++sample) updateLivePhAlarm();
  Serial.printf("[TEST] Alarmanzeige: %s (nach 3 simulierten Messungen)\n",
                phAlarmLabel(livePhAlarm.alarm));
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
  display.print(F("TO:"));
  if (isfinite(toCaseTempC(liveToV))) {
    display.print(toCaseTempC(liveToV), 1);
    display.print(F("C"));
  } else if (isfinite(liveToV)) display.print(liveToV, 3);
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
  display.print(phSimulationActive ? F("TEST ") : F("pH "));
  display.print(F("ALARM:"));
  switch (livePhAlarm.alarm) {
    case PhTestAlarm::Normal: display.print(F("OK")); break;
    case PhTestAlarm::Low: display.print(F("LOW")); break;
    case PhTestAlarm::High: display.print(F("HIGH")); break;
    default: display.print(F("SENSOR")); break;
  }
  display.display();
}


static void runPhAlarmSelfTest() {
  Serial.println(F("\n=== pH-ALARM SOFTWARETEST (KEIN REALER ALARM) ==="));
  PhTestMachine sm;
  uint8_t passed = 0, failed = 0;
  auto check = [&](bool ok, const char *name) {
    Serial.printf("%s | %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) ++passed; else ++failed;
  };
  sm.update(6.0f); sm.update(6.0f); sm.update(6.0f);
  check(sm.alarm == PhTestAlarm::Normal, "Normal 6.0 nach 3 Messungen");
  sm.update(5.4f); sm.update(5.4f);
  check(sm.alarm == PhTestAlarm::Normal, "Untergrenze noch nicht nach 2 Messungen");
  sm.update(5.4f);
  check(sm.alarm == PhTestAlarm::Low, "pH 5.4 nach 3 Messungen: LOW");
  sm.update(5.55f);
  check(sm.alarm == PhTestAlarm::Low, "pH 5.55: LOW bleibt (Hysterese)");
  sm.update(5.61f); sm.update(5.61f); sm.update(5.61f);
  check(sm.alarm == PhTestAlarm::Normal, "pH 5.61 nach 3 Messungen: Normal");
  sm.update(6.6f); sm.update(6.6f);
  check(sm.alarm == PhTestAlarm::Normal, "Obergrenze noch nicht nach 2 Messungen");
  sm.update(6.6f);
  check(sm.alarm == PhTestAlarm::High, "pH 6.6 nach 3 Messungen: HIGH");
  sm.update(6.45f);
  check(sm.alarm == PhTestAlarm::High, "pH 6.45: HIGH bleibt (Hysterese)");
  sm.update(6.39f); sm.update(6.39f); sm.update(6.39f);
  check(sm.alarm == PhTestAlarm::Normal, "pH 6.39 nach 3 Messungen: Normal");
  sm.update(NAN);
  check(sm.alarm == PhTestAlarm::Fault, "Fehler bei NAN sofort");
  sm.update(6.0f); sm.update(6.0f); sm.update(6.0f);
  sm.update(6.0f, false);
  check(sm.alarm == PhTestAlarm::Fault, "Fehler ohne Kalibrierung sofort");
  Serial.printf("pH-Alarmtestergebnis: %u PASS / %u FAIL\n",
                static_cast<unsigned>(passed), static_cast<unsigned>(failed));
  Serial.println(F("Messung, Alarmspeicher, SSR und Server wurden NICHT veraendert."));
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
    } else if (c == 'w' || c == 'W') {
      selectWaterSensorRom(Serial.readStringUntil('\n'));
      updateTemperature(true);
    } else if (c == 'a' || c == 'b') {
      String arg = Serial.readStringUntil('\n');
      arg.trim();
      const char *begin = arg.c_str();
      char *end = nullptr;
      const float tempC = strtof(begin, &end);
      if (begin == end || *end != '\0' || !isfinite(tempC)) {
        Serial.println(F("TO Syntax: a 20.0 bzw. b 30.0 (numerische Celsius-Referenz)."));
      } else {
        captureToPoint(c == 'a' ? 1 : 2, tempC);
      }
    } else if (c == 'p' || c == 'P') {
      runPhAlarmSelfTest();
    } else if (c >= '0' && c <= '3') {
      setPhSimulation(c);
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
  initMlx90640();

  bhWire.begin(PIN_BH_SDA, PIN_BH_SCL, 50000);
  scanBh1750();
  dht1.begin(); dht2.begin(); dht3.begin();
  if (PH_DO_WIRED) pinMode(PIN_PH_DO, INPUT);

  temperatureSensors.begin();
  loadWaterSensorRom();
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
  loadToCalibration();
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
  computeLeafVpdCandidate();
  sampleMlx90640(millis());
  updateLive();
  updateLivePhAlarm();

  const unsigned long now = millis();
  static unsigned long lastPrint = 0;
  if (now - lastPrint >= 1000) {
    lastPrint = now;
    Serial.printf("LIVE | ADS:%s | A0 pH %.5f V | std %.2f mV | A1 TDS %.5f V",
                  adsOk ? "OK" : "--", liveInputV, liveStdV * 1000.0f, liveTdsV);
    if (validTemperature(liveWaterTempC)) Serial.printf(" | DS-Wasser %.2f C", liveWaterTempC);
    else Serial.print(F(" | DS --"));
    if (calibrationReady() && isfinite(liveInputV)) {
      Serial.printf(" | pH %.3f", phFromInputVoltage(liveInputV));
    }
    uint8_t n = 0;
    for (uint8_t i = 0; i < 3; ++i) if (air[i].ok) ++n;
    if (ecCalibrated && validTemperature(liveWaterTempC))
      Serial.printf(" | EC %.3f mS/cm@25C (DS %.2fC)", ecFromVoltage(liveTdsV, liveWaterTempC), liveWaterTempC);
    else Serial.print(F(" | EC -- (Kalibrierung/DS fehlt)"));
    Serial.printf(" | A1 RohStd %.1f mV | TO A2 %.4f V | Gehaeuse %.2f C | pH-TK %s | DO %s",
      liveTdsStdV * 1000.0f, liveToV, toCaseTempC(liveToV),
      phTemperatureCompensationReady() ? "DS" : "--",
      PH_DO_WIRED ? (liveDoHigh ? "HIGH" : "LOW") : "--");
    if (mlx90640Ok) Serial.printf(" | MLX ROI %.1f C | BlattVPD-Kandidat %.3f kPa",
                                thermalRoiTempC, leafVpdCandidate);
    else Serial.print(F(" | MLX --"));
    Serial.printf(" | DHT %u/3 | BH %s",
                  static_cast<unsigned>(n),
                  bhOk && isfinite(bhLux) ? "OK" : "--");
    Serial.printf(" | %s pH-ALARM %s",
                  phSimulationActive ? "TEST" : "LIVE", phAlarmLabel(livePhAlarm.alarm));
    if (phSimulationActive) Serial.printf(" | TEST pH %.2f", phSimulatedValue);
    Serial.println();
  }
  if (now - lastOledMs >= OLED_INTERVAL_MS) {
    lastOledMs = now;
    drawOled();
  }
  delay(10);
}
