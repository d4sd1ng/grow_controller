/**
 * Bestehendes PH4502C + ADS1115 Kalibrier-Tool, auf aktuellen
 * Sensor-ESP32 angepasst. Historische Quelle: hardware-v2-firmware.
 *
 * ADS1115: I2C SDA21/SCL22, Adresse 0x48; A0=pH, A1=EC/TDS.
 * Wassersensor: genau ein DS18B20 an GPIO23.
 * Kein OLED, WLAN oder Aktorcode. Messungen laufen exklusiv und
 * synchron, ohne konkurrierende Hintergrundabfragen.
 *
 * pH 7/4: 160 Abtastungen bei 20ms Abstand, max. 10mV Std.
 * EC 1413uS/cm: 8 x 30 Samples bei 40ms Abstand,
 * median Std <=10mV, median Spanne <=30mV.
 * NVS kompatibel mit Sensor.ino: phads_v1 / ecads_v1.
 * Wichtig: niemals "Erase All Flash"; identisches Partitionsschema
 * beim anschliessenden Flashen der Sensor-Firmware verwenden.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>
#include "calibration_capture.h"

static constexpr int PIN_I2C_SDA = 21;
static constexpr int PIN_I2C_SCL = 22;
static constexpr uint8_t ADS_ADDR = 0x48;
static constexpr uint8_t PIN_ONEWIRE = 23;
static constexpr uint16_t CAPTURE_SAMPLE_COUNT = grow_cal::PH_SAMPLES;
static constexpr uint16_t SAMPLE_DELAY_MS = grow_cal::PH_INTERVAL_MS;

Adafruit_ADS1115 ads;
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature ds18b20(&oneWire);
bool adsOk = false;

bool havePh7 = false, havePh4 = false;
float ph7AdsV = NAN, ph4AdsV = NAN;
float ph7TempC = NAN, ph4TempC = NAN;
bool haveEc = false;
float ecRefAdsV = NAN, ecTempC = NAN, ecFactor = NAN;

struct SampleStats {
  float meanV = NAN;
  float stdV = NAN;
  int16_t minRaw = INT16_MAX;
  int16_t maxRaw = INT16_MIN;
  bool valid = false;
};

static float rawToVolts(int16_t raw) {
  // ADS1115 GAIN_ONE = +/-4.096V, 125uV je Bit.
  return static_cast<float>(raw) * 0.000125f;
}

// Urspruengliche Mittelwert-/Streuungsmessung; Kanalwahl und
// Plausibilitaetskontrolle fuer A1 ergaenzt.
static SampleStats sampleChannel(uint8_t channel, uint16_t count,
                                 uint16_t delayMs, float minV, float maxV,
                                 float *samples = nullptr) {
  SampleStats stats;
  if (!adsOk || count == 0) return stats;
  double sum = 0.0, sumSq = 0.0;
  stats.valid = true;
  for (uint16_t i = 0; i < count; ++i) {
    const int16_t raw = ads.readADC_SingleEnded(channel);
    const float v = rawToVolts(raw);
    if (!isfinite(v) || v < minV || v > maxV) stats.valid = false;
    if (samples) samples[i] = v;
    sum += v;
    sumSq += static_cast<double>(v) * v;
    if (raw < stats.minRaw) stats.minRaw = raw;
    if (raw > stats.maxRaw) stats.maxRaw = raw;
    if (delayMs) delay(delayMs);
  }
  stats.meanV = static_cast<float>(sum / count);
  const double mean = sum / count;
  const double variance = sumSq / count - mean * mean;
  stats.stdV = static_cast<float>(sqrt(variance > 0.0 ? variance : 0.0));
  return stats;
}

static bool validWaterTemp(float t) {
  return isfinite(t) && t > -20.0f && t < 80.0f &&
         t != DEVICE_DISCONNECTED_C;
}
static bool readWaterTemp(float &out) {
  out = NAN;
  ds18b20.begin();
  const uint8_t count = ds18b20.getDeviceCount();
  if (count != 1) {
    Serial.printf("ABGELEHNT: genau 1 DS18B20 GPIO23 erforderlich, erkannt: %u\n",
                  static_cast<unsigned>(count));
    return false;
  }
  ds18b20.requestTemperatures();
  out = ds18b20.getTempCByIndex(0);
  if (!validWaterTemp(out)) {
    Serial.println(F("ABGELEHNT: DS18B20 Temperatur ungueltig."));
    return false;
  }
  return true;
}

static bool calibrationReady() {
  return havePh7 && havePh4 && fabsf(ph4AdsV - ph7AdsV) >= 0.001f;
}

static float calibrationSlope() {
  if (!calibrationReady()) return NAN;
  return (4.0f - 7.0f) / (ph4AdsV - ph7AdsV);
}

static float calibrationOffset() {
  const float slope = calibrationSlope();
  if (isnan(slope)) return NAN;
  return 7.0f - slope * ph7AdsV;
}

static float phFromAdsVoltage(float adsV) {
  const float slope = calibrationSlope();
  const float offset = calibrationOffset();
  if (isnan(slope) || isnan(offset)) return NAN;
  return slope * adsV + offset;
}

static void loadCalibration() {
  havePh7 = havePh4 = haveEc = false;
  ph7AdsV = ph4AdsV = ph7TempC = ph4TempC = NAN;
  ecRefAdsV = ecTempC = ecFactor = NAN;
  Preferences p;
  if (p.begin("phads_v1", true)) {
    const bool flag7 = p.getBool("have7", false);
    const bool flag4 = p.getBool("have4", false);
    ph7AdsV = p.getFloat("volt7", NAN);
    ph4AdsV = p.getFloat("volt4", NAN);
    ph7TempC = p.getFloat("temp7", NAN);
    ph4TempC = p.getFloat("temp4", NAN);
    havePh7 = flag7 && isfinite(ph7AdsV) && ph7AdsV >= 0.0f &&
              ph7AdsV <= 3.3f && validWaterTemp(ph7TempC);
    havePh4 = flag4 && isfinite(ph4AdsV) && ph4AdsV >= 0.0f &&
              ph4AdsV <= 3.3f && validWaterTemp(ph4TempC);
    p.end();
  }
  if (p.begin("ecads_v1", true)) {
    const bool flag = p.getBool("valid", false);
    ecFactor = p.getFloat("factor", NAN);
    ecRefAdsV = p.getFloat("ref_v", NAN);
    ecTempC = p.getFloat("temp_c", NAN);
    haveEc = flag && isfinite(ecFactor) && ecFactor >= 0.1f &&
             ecFactor <= 10.0f && isfinite(ecRefAdsV) &&
             ecRefAdsV > 0.05f && ecRefAdsV <= 2.3f &&
             validWaterTemp(ecTempC);
    p.end();
  }
}
static bool savePhPoint(bool isSeven, float voltage, float tempC) {
  if (!isfinite(voltage) || voltage < 0.05f || voltage > 3.3f ||
      !validWaterTemp(tempC)) return false;
  const bool hasOther = isSeven ? havePh4 : havePh7;
  const float otherV = isSeven ? ph4AdsV : ph7AdsV;
  if (hasOther && fabsf(otherV - voltage) < 0.001f) {
    Serial.println(F("ABGELEHNT: pH7 und pH4 Spannungsabstand <1mV."));
    return false;
  }
  const char *vk = isSeven ? "volt7" : "volt4";
  const char *tk = isSeven ? "temp7" : "temp4";
  const char *hk = isSeven ? "have7" : "have4";
  Preferences p;
  if (!p.begin("phads_v1", false)) return false;
  const bool written = p.putFloat(vk, voltage) == sizeof(float) &&
                       p.putFloat(tk, tempC) == sizeof(float) &&
                       p.putBool(hk, true) == sizeof(bool);
  p.end();
  if (!written) return false;
  if (!p.begin("phads_v1", true)) return false;
  const float checkV = p.getFloat(vk, NAN);
  const float checkT = p.getFloat(tk, NAN);
  const bool verified = p.getBool(hk, false) &&
                        isfinite(checkV) && isfinite(checkT) &&
                        fabsf(checkV - voltage) <= 0.000001f &&
                        fabsf(checkT - tempC) <= 0.01f;
  p.end();
  loadCalibration();
  return verified && (isSeven ? havePh7 : havePh4);
}
static bool saveEcPoint(float voltage, float tempC, float factor) {
  if (!isfinite(voltage) || voltage <= 0.05f || voltage > 2.3f ||
      !validWaterTemp(tempC) || !isfinite(factor) ||
      factor < 0.1f || factor > 10.0f) return false;
  Preferences p;
  if (!p.begin("ecads_v1", false)) return false;
  const bool written = p.putFloat("factor", factor) == sizeof(float) &&
                       p.putFloat("ref_v", voltage) == sizeof(float) &&
                       p.putFloat("temp_c", tempC) == sizeof(float) &&
                       p.putBool("valid", true) == sizeof(bool);
  p.end();
  if (!written) return false;
  if (!p.begin("ecads_v1", true)) return false;
  const float checkFactor = p.getFloat("factor", NAN);
  const float checkV = p.getFloat("ref_v", NAN);
  const float checkT = p.getFloat("temp_c", NAN);
  const bool verified = p.getBool("valid", false) &&
                        isfinite(checkFactor) && isfinite(checkV) &&
                        isfinite(checkT) &&
                        fabsf(checkFactor - factor) <= 0.00001f &&
                        fabsf(checkV - voltage) <= 0.000001f &&
                        fabsf(checkT - tempC) <= 0.01f;
  p.end();
  loadCalibration();
  return verified && haveEc;
}

static void printHelp() {
  Serial.println(F("=== PH4502C/ADS1115 KALIBRIERTOOL ==="));
  Serial.println(F("7 = pH 7,00 auf A0 kalibrieren"));
  Serial.println(F("4 = pH 4,00 auf A0 kalibrieren"));
  Serial.println(F("e = EC 1413uS/cm auf A1 kalibrieren"));
  Serial.println(F("s = NVS-Kalibrierstatus"));
  Serial.println(F("m = Diagnose-Spannungen einmal messen"));
  Serial.println(F("h = Hilfe"));
  Serial.println(F("Keine Loeschfunktion; fehlgeschlagene Messung aendert NVS nicht."));
}
static void printCalibration() {
  Serial.println(F("--- NVS-Kalibrierstatus ---"));
  if (havePh7) Serial.printf("pH7: %.6f V | %.2f C | gespeichert\n", ph7AdsV, ph7TempC);
  else Serial.println(F("pH7: FEHLT"));
  if (havePh4) Serial.printf("pH4: %.6f V | %.2f C | gespeichert\n", ph4AdsV, ph4TempC);
  else Serial.println(F("pH4: FEHLT"));
  if (calibrationReady()) {
    Serial.printf("pH Steigung %.6f pH/V | Offset %.6f\n", calibrationSlope(), calibrationOffset());
  } else Serial.println(F("pH: UNVOLLSTAENDIG"));
  if (haveEc)
    Serial.printf("EC: %.6f V | %.2f C | Faktor %.6f | gespeichert\n",
                  ecRefAdsV, ecTempC, ecFactor);
  else Serial.println(F("EC: FEHLT oder ungueltig"));
}
static void capturePoint(float targetPh) {
  if (!adsOk) { Serial.println(F("ABGELEHNT: ADS1115 nicht erreichbar.")); return; }
  float beforeC = NAN, afterC = NAN;
  if (!readWaterTemp(beforeC)) return;
  Serial.printf("KALIBRIERUNG START: pH %.0f | DS18B20 %.2f C\n", targetPh, beforeC);
  // Die alte Mittelwertroutine arbeitet hier exklusiv, ohne Hintergrundloop.
  const SampleStats stats = sampleChannel(0, CAPTURE_SAMPLE_COUNT,
                                         SAMPLE_DELAY_MS, 0.05f, 3.3f);
  Serial.printf("A0 %.6f V | Std %.2f mV | raw %d..%d\n",
                stats.meanV, stats.stdV * 1000.0f, stats.minRaw, stats.maxRaw);
  if (!stats.valid || !isfinite(stats.stdV) ||
      stats.stdV > grow_cal::PH_MAX_STD_V) {
    Serial.println(F("ABGELEHNT: pH-ADC ungueltig oder Streuung >10mV; nichts gespeichert."));
    return;
  }
  if (!readWaterTemp(afterC) || fabsf(afterC - beforeC) > 3.0f) {
    Serial.println(F("ABGELEHNT: DS18B20 fehlt oder Temperaturabweichung >3C."));
    return;
  }
  const bool saved = savePhPoint(targetPh == 7.0f, stats.meanV, afterC);
  Serial.println(saved ? F("OK: pH-Punkt in NVS gespeichert und geprueft.")
                       : F("NICHT GESPEICHERT: unplausibel oder NVS-Fehler."));
  printCalibration();
}
static void captureEc() {
  if (!adsOk) { Serial.println(F("ABGELEHNT: ADS1115 nicht erreichbar.")); return; }
  float beforeC = NAN, afterC = NAN;
  if (!readWaterTemp(beforeC)) return;
  Serial.printf("KALIBRIERUNG START: EC 1413uS/cm | DS18B20 %.2f C\n", beforeC);
  grow_cal::Stats medians;
  for (uint8_t w = 0; w < grow_cal::EC_WINDOWS; ++w) {
    float readings[grow_cal::EC_SAMPLES_PER_WINDOW] = {};
    const SampleStats stats = sampleChannel(1, grow_cal::EC_SAMPLES_PER_WINDOW,
      grow_cal::EC_INTERVAL_MS, 0.0f, 2.3f, readings);
    if (!stats.valid) {
      Serial.println(F("EC ABBRUCH: ADC A1 ungueltig; nichts gespeichert."));
      return;
    }
    const float medianV = grow_cal::median30(readings);
    if (!medians.add(medianV, 2.3f)) {
      Serial.println(F("EC ABBRUCH: Fenster-Median ungueltig; nichts gespeichert."));
      return;
    }
    Serial.printf("EC Fenster %u/%u | Median %.6f V | Roh-Std %.2f mV\n",
        static_cast<unsigned>(w+1), static_cast<unsigned>(grow_cal::EC_WINDOWS),
        medianV, stats.stdV * 1000.0f);
    if (w + 1 < grow_cal::EC_WINDOWS) delay(grow_cal::EC_WINDOW_GAP_MS);
  }
  const float meanV = medians.mean();
  const float stdV = medians.stdDev();
  const float spanV = medians.maxV - medians.minV;
  Serial.printf("EC A1 | Referenz %.6f V | Median-Std %.2f mV | Spanne %.2f mV\n",
      meanV, stdV * 1000.0f, spanV * 1000.0f);
  if (!isfinite(stdV) || stdV > grow_cal::EC_MAX_MEDIAN_STD_V ||
      spanV > grow_cal::EC_MAX_MEDIAN_SPAN_V) {
    Serial.println(F("ABGELEHNT: EC-Mediane instabil (>10mV Std / >30mV Spanne)."));
    return;
  }
  if (!readWaterTemp(afterC) || fabsf(afterC - beforeC) > 3.0f) {
    Serial.println(F("ABGELEHNT: DS18B20 fehlt oder Temperaturabweichung >3C."));
    return;
  }
  const float factor = grow_cal::ecCalibrationFactor(meanV, afterC);
  if (!isfinite(factor)) {
    Serial.println(F("ABGELEHNT: EC-Referenzspannung oder Faktor unplausibel."));
    return;
  }
  const bool saved = saveEcPoint(meanV, afterC, factor);
  Serial.println(saved ? F("OK: EC in NVS gespeichert und geprueft.")
                       : F("NICHT GESPEICHERT: EC-NVS-Fehler."));
  printCalibration();
}
static void printVoltageDiagnostic() {
  if (!adsOk) { Serial.println(F("ADS1115 nicht erkannt.")); return; }
  const SampleStats ph = sampleChannel(0, 32, 20, 0.0f, 3.3f);
  const SampleStats ec = sampleChannel(1, 32, 20, 0.0f, 2.3f);
  Serial.printf("LIVE A0 pH: %.6f V | Std %.2f mV\n", ph.meanV, ph.stdV * 1000.0f);
  Serial.printf("LIVE A1 EC: %.6f V | Std %.2f mV\n", ec.meanV, ec.stdV * 1000.0f);
}
static void handleSerial() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '7') capturePoint(7.0f);
    else if (c == '4') capturePoint(4.0f);
    else if (c == 'e' || c == 'E') captureEc();
    else if (c == 's' || c == 'S') { loadCalibration(); printCalibration(); }
    else if (c == 'm' || c == 'M') printVoltageDiagnostic();
    else if (c == 'h' || c == 'H' || c == '?') printHelp();
    else if (c == 'r' || c == 'R')
      Serial.println(F("NVS-Loeschung deaktiviert. Kalibrierung mit 7/4/e ersetzen."));
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  ads.setGain(GAIN_ONE);
  adsOk = ads.begin(ADS_ADDR, &Wire);
  ds18b20.begin();
  loadCalibration();
  printHelp();
  Serial.printf("ADS1115 0x48: %s\n", adsOk ? "OK" : "NICHT ERKANNT");
  Serial.printf("DS18B20 an D23 erkannt: %u\n",
                static_cast<unsigned>(ds18b20.getDeviceCount()));
  printCalibration();
}
void loop() {
  // Keine Hintergrund-Sensorabfrage, kein Display oder WLAN.
  // Waehrend eines Kalibrierungsbefehls gehoert der ADC nur der Messroutine.
  handleSerial();
  delay(2);
}
