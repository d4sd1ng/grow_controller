/**
 * PH4502C + ADS1115 Kalibrier-Servicegeraet
 *
 * Zweck:
 * - misst PH4502C PO ueber den 10k/20k-Spannungsteiler am ADS1115 A0
 * - zeigt Rohwert, ADS-Spannung und rueckgerechnete PH4502C-PO-Spannung
 * - erfasst stabile pH-7- und pH-4-Punkte
 * - speichert beide Punkte dauerhaft im ESP32-NVS
 * - gibt exakt die Konstanten aus, die die Sensor-Firmware verwendet
 *
 * Hardware gemaess wiring_plan_v2.md:
 *   PH4502C PO -> 10k -> ADS1115 A0 -> 20k -> GND
 *   ADS1115 VDD -> 3.3V
 *   ADS1115 GND -> GND
 *   ADS1115 SDA -> GPIO21
 *   ADS1115 SCL -> GPIO22
 *   ADS1115 ADDR -> GND (0x48)
 *
 * Optional:
 *   integriertes SSD1306 OLED auf demselben I2C-Bus (0x3C)
 *
 * Serial Monitor: 115200 Baud
 *   7 = stabilen Mittelwert als pH-7-Punkt erfassen und speichern
 *   4 = stabilen Mittelwert als pH-4-Punkt erfassen und speichern
 *   s = Status / Kalibrierwerte / Sensor-Firmware-Konstanten ausgeben
 *   r = beide gespeicherten Kalibrierpunkte loeschen
 *   h = Hilfe
 */

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

static constexpr int PIN_I2C_SDA = 21;
static constexpr int PIN_I2C_SCL = 22;
static constexpr uint8_t ADS_ADDR = 0x48;
static constexpr uint8_t OLED_ADDR = 0x3C;

static constexpr float R_TOP_OHM = 10000.0f;
static constexpr float R_BOTTOM_OHM = 20000.0f;
static constexpr float DIVIDER_RATIO = R_BOTTOM_OHM / (R_TOP_OHM + R_BOTTOM_OHM);
static constexpr float DIVIDER_RECONSTRUCT = 1.0f / DIVIDER_RATIO;

static constexpr uint16_t LIVE_SAMPLE_COUNT = 32;
static constexpr uint16_t CAPTURE_SAMPLE_COUNT = 160;
static constexpr uint16_t SAMPLE_DELAY_MS = 20;
static constexpr unsigned long DISPLAY_INTERVAL_MS = 500;

static constexpr float CAPTURE_MAX_STD_V = 0.010f;
static constexpr float MIN_CAL_POINT_DISTANCE_V = 0.001f;

static constexpr char NVS_NAMESPACE[] = "phcal";
static constexpr char NVS_HAVE_7[] = "have7";
static constexpr char NVS_HAVE_4[] = "have4";
static constexpr char NVS_VOLT_7[] = "volt7";
static constexpr char NVS_VOLT_4[] = "volt4";

Adafruit_ADS1115 ads;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Preferences preferences;

bool adsOk = false;
bool oledOk = false;

float liveAdsV = NAN;
float livePoV = NAN;
float liveStdV = NAN;

bool havePh7 = false;
bool havePh4 = false;
float ph7AdsV = NAN;
float ph4AdsV = NAN;

unsigned long lastDisplayMs = 0;

struct SampleStats {
  float meanV;
  float stdV;
  int16_t minRaw;
  int16_t maxRaw;
};

static float rawToVolts(int16_t raw) {
  // ADS1115 GAIN_ONE: +/-4.096 V -> 125 uV/bit
  return static_cast<float>(raw) * 0.000125f;
}

static SampleStats sampleChannel(uint16_t count, uint16_t delayMs) {
  double sum = 0.0;
  double sumSq = 0.0;
  int16_t minRaw = INT16_MAX;
  int16_t maxRaw = INT16_MIN;

  for (uint16_t i = 0; i < count; ++i) {
    const int16_t raw = ads.readADC_SingleEnded(0);
    const float v = rawToVolts(raw);

    sum += v;
    sumSq += static_cast<double>(v) * static_cast<double>(v);

    if (raw < minRaw) minRaw = raw;
    if (raw > maxRaw) maxRaw = raw;

    if (delayMs > 0) delay(delayMs);
  }

  const double mean = sum / count;
  double variance = (sumSq / count) - (mean * mean);
  if (variance < 0.0) variance = 0.0;

  SampleStats s;
  s.meanV = static_cast<float>(mean);
  s.stdV = static_cast<float>(sqrt(variance));
  s.minRaw = minRaw;
  s.maxRaw = maxRaw;
  return s;
}

static bool calibrationReady() {
  return havePh7 &&
         havePh4 &&
         !isnan(ph7AdsV) &&
         !isnan(ph4AdsV) &&
         fabsf(ph4AdsV - ph7AdsV) >= MIN_CAL_POINT_DISTANCE_V;
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
  if (isnan(slope) || isnan(offset) || isnan(adsV)) return NAN;
  return slope * adsV + offset;
}

static bool loadCalibration() {
  if (!preferences.begin(NVS_NAMESPACE, true)) {
    havePh7 = false;
    havePh4 = false;
    ph7AdsV = NAN;
    ph4AdsV = NAN;
    return false;
  }

  havePh7 = preferences.getBool(NVS_HAVE_7, false);
  havePh4 = preferences.getBool(NVS_HAVE_4, false);
  ph7AdsV = preferences.getFloat(NVS_VOLT_7, NAN);
  ph4AdsV = preferences.getFloat(NVS_VOLT_4, NAN);
  preferences.end();

  if (havePh7 && isnan(ph7AdsV)) havePh7 = false;
  if (havePh4 && isnan(ph4AdsV)) havePh4 = false;
  return true;
}

static bool saveCalibration() {
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;

  bool ok = true;

  if (havePh7) ok = ok && (preferences.putFloat(NVS_VOLT_7, ph7AdsV) == sizeof(float));
  else if (preferences.isKey(NVS_VOLT_7)) ok = ok && preferences.remove(NVS_VOLT_7);

  if (havePh4) ok = ok && (preferences.putFloat(NVS_VOLT_4, ph4AdsV) == sizeof(float));
  else if (preferences.isKey(NVS_VOLT_4)) ok = ok && preferences.remove(NVS_VOLT_4);

  // Flags last: a stored point is only marked valid after its voltage write succeeded.
  ok = ok && (preferences.putBool(NVS_HAVE_7, havePh7) == sizeof(bool));
  ok = ok && (preferences.putBool(NVS_HAVE_4, havePh4) == sizeof(bool));
  preferences.end();

  if (!ok || !preferences.begin(NVS_NAMESPACE, true)) return false;

  const bool storedHave7 = preferences.getBool(NVS_HAVE_7, false);
  const bool storedHave4 = preferences.getBool(NVS_HAVE_4, false);
  const float storedPh7 = preferences.getFloat(NVS_VOLT_7, NAN);
  const float storedPh4 = preferences.getFloat(NVS_VOLT_4, NAN);
  preferences.end();

  if (storedHave7 != havePh7 || storedHave4 != havePh4) return false;
  if (havePh7 && (isnan(storedPh7) || fabsf(storedPh7 - ph7AdsV) > 0.000001f)) return false;
  if (havePh4 && (isnan(storedPh4) || fabsf(storedPh4 - ph4AdsV) > 0.000001f)) return false;

  return true;
}

static bool clearCalibration() {
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool ok = preferences.clear();
  preferences.end();

  if (!ok) return false;

  havePh7 = false;
  havePh4 = false;
  ph7AdsV = NAN;
  ph4AdsV = NAN;
  return true;
}

static void printHelp() {
  Serial.println();
  Serial.println(F("=== PH4502C / ADS1115 Kalibrier-Servicegeraet ==="));
  Serial.println(F("7 = pH-7-Punkt erfassen + im NVS speichern"));
  Serial.println(F("4 = pH-4-Punkt erfassen + im NVS speichern"));
  Serial.println(F("s = Status + fertige Sensor-Firmware-Konstanten"));
  Serial.println(F("r = gespeicherte Kalibrierpunkte loeschen"));
  Serial.println(F("h = Hilfe"));
  Serial.println(F("Hinweis: instabile Messungen >10 mV Std-Abw. werden NICHT gespeichert."));
  Serial.println(F("-------------------------------------------------"));
}

static void printCalibration() {
  Serial.println();
  Serial.println(F("--- Kalibrierstatus ---"));

  if (havePh7) {
    Serial.printf("pH 7: ADS = %.6f V | PH4502C PO ~= %.6f V\n",
                  ph7AdsV, ph7AdsV * DIVIDER_RECONSTRUCT);
  } else {
    Serial.println(F("pH 7: noch nicht erfasst"));
  }

  if (havePh4) {
    Serial.printf("pH 4: ADS = %.6f V | PH4502C PO ~= %.6f V\n",
                  ph4AdsV, ph4AdsV * DIVIDER_RECONSTRUCT);
  } else {
    Serial.println(F("pH 4: noch nicht erfasst"));
  }

  if (calibrationReady()) {
    const float slope = calibrationSlope();
    const float offset = calibrationOffset();

    Serial.println();
    Serial.println(F("Direkt in growroom_telemetry_client.ino uebernehmen:"));
    Serial.printf("static constexpr float PH_CAL_VOLT_4 = %.6ff;\n", ph4AdsV);
    Serial.printf("static constexpr float PH_CAL_VOLT_7 = %.6ff;\n", ph7AdsV);

    Serial.println();
    Serial.println(F("Diagnose der daraus resultierenden Geraden:"));
    Serial.printf("PH_SLOPE  = %.6ff;\n", slope);
    Serial.printf("PH_OFFSET = %.6ff;\n", offset);
    Serial.println(F("Formel: pH = PH_SLOPE * ads_voltage + PH_OFFSET"));
  } else if (havePh7 && havePh4) {
    Serial.println(F("FEHLER: Kalibrierpunkte liegen zu dicht beieinander."));
  }

  Serial.println(F("-----------------------"));
}

static bool captureStablePoint(float targetPh) {
  if (!adsOk) {
    Serial.println(F("FEHLER: ADS1115 nicht gefunden."));
    return false;
  }

  Serial.printf("\nErfasse pH %.0f ... Sonde waehrend der Messung ruhig lassen.\n", targetPh);
  const SampleStats s = sampleChannel(CAPTURE_SAMPLE_COUNT, SAMPLE_DELAY_MS);

  Serial.printf("Mittelwert ADS: %.6f V\n", s.meanV);
  Serial.printf("Std-Abw.:       %.6f V\n", s.stdV);
  Serial.printf("PH4502C PO:     %.6f V (rueckgerechnet)\n", s.meanV * DIVIDER_RECONSTRUCT);
  Serial.printf("Raw min/max:    %d / %d\n", s.minRaw, s.maxRaw);

  if (s.stdV > CAPTURE_MAX_STD_V) {
    Serial.println(F("ABGELEHNT: Messung schwankt >10 mV. Punkt wurde NICHT gespeichert."));
    return false;
  }

  if (targetPh == 7.0f) {
    ph7AdsV = s.meanV;
    havePh7 = true;
  } else if (targetPh == 4.0f) {
    ph4AdsV = s.meanV;
    havePh4 = true;
  } else {
    Serial.println(F("FEHLER: unbekannter Kalibrierpunkt."));
    return false;
  }

  if (!saveCalibration()) {
    Serial.println(F("FEHLER: Kalibrierpunkt konnte nicht dauerhaft im NVS verifiziert werden."));
    Serial.println(F("Der Messwert ist nur im RAM aktiv; bitte NVS/Flash pruefen und erneut erfassen."));
    printCalibration();
    return false;
  }

  Serial.println(F("OK: stabiler Kalibrierpunkt dauerhaft gespeichert und verifiziert."));
  printCalibration();
  return true;
}

static void updateLive() {
  if (!adsOk) return;
  const SampleStats s = sampleChannel(LIVE_SAMPLE_COUNT, 0);
  liveAdsV = s.meanV;
  livePoV = liveAdsV * DIVIDER_RECONSTRUCT;
  liveStdV = s.stdV;
}

static void drawOled() {
  if (!oledOk) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(F("pH CAL / ADS1115"));

  display.setCursor(0, 13);
  display.print(F("ADS "));
  if (isnan(liveAdsV)) display.print(F("--"));
  else display.print(liveAdsV, 4);
  display.print(F("V"));

  display.setCursor(0, 25);
  display.print(F("std "));
  if (isnan(liveStdV)) display.print(F("--"));
  else display.print(liveStdV * 1000.0f, 2);
  display.print(F("mV"));

  display.setCursor(0, 37);
  display.print(F("7:"));
  display.print(havePh7 ? F("OK") : F("--"));
  display.print(F(" 4:"));
  display.print(havePh4 ? F("OK") : F("--"));

  display.setCursor(0, 50);
  if (calibrationReady()) {
    const float ph = phFromAdsVoltage(liveAdsV);
    display.print(F("pH "));
    if (isnan(ph)) display.print(F("--"));
    else display.print(ph, 2);
  } else {
    display.print(F("Serial: 7 / 4 / s"));
  }

  display.display();
}

static void handleSerial() {
  while (Serial.available()) {
    const char c = Serial.read();

    if (c == '7') {
      captureStablePoint(7.0f);
    } else if (c == '4') {
      captureStablePoint(4.0f);
    } else if (c == 's' || c == 'S') {
      printCalibration();
    } else if (c == 'r' || c == 'R') {
      if (clearCalibration()) {
        Serial.println(F("Kalibrierpunkte aus RAM und NVS geloescht."));
      } else {
        Serial.println(F("FEHLER: NVS konnte nicht geloescht werden; RAM bleibt unveraendert."));
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

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);

  ads.setGain(GAIN_ONE);
  adsOk = ads.begin(ADS_ADDR, &Wire);

  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println(F("pH calibration"));
    display.setCursor(0, 34);
    display.println(F("service ESP"));
    display.display();
  }

  const bool nvsReadable = loadCalibration();
  printHelp();

  if (!nvsReadable) {
    Serial.println(F("WARNUNG: NVS konnte nicht geoeffnet werden; Persistenz ist derzeit nicht verfuegbar."));
  }

  if (!adsOk) {
    Serial.println(F("FEHLER: ADS1115 @ 0x48 nicht gefunden."));
  } else {
    Serial.println(F("ADS1115 @ 0x48 gefunden."));
    Serial.println(F("Warte bis der Wert stabil ist, dann '7' bzw. '4' senden."));
  }

  if (havePh7 || havePh4) {
    Serial.println(F("Gespeicherte NVS-Kalibrierung geladen."));
    printCalibration();
  } else {
    Serial.println(F("Keine gespeicherten Kalibrierpunkte vorhanden."));
  }
}

void loop() {
  handleSerial();

  if (adsOk) {
    updateLive();

    static unsigned long lastPrint = 0;
    const unsigned long now = millis();
    if (now - lastPrint >= 1000) {
      lastPrint = now;
      Serial.printf("LIVE | ADS %.6f V | PO %.6f V | std %.3f mV",
                    liveAdsV,
                    livePoV,
                    liveStdV * 1000.0f);

      if (calibrationReady()) {
        Serial.printf(" | pH %.3f", phFromAdsVoltage(liveAdsV));
      }
      Serial.println();
    }

    if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
      lastDisplayMs = now;
      drawOled();
    }
  }

  delay(10);
}
