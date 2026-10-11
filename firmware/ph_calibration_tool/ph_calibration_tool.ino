/**
 * PH4502C Kalibrier-Servicegeraet ohne ADS1115
 *
 * Hardware:
 *   PH4502C VCC -> 5V
 *   PH4502C GND -> GND
 *   PH4502C PO  -> 10k -> VP
 *                         |
 *                        10k
 *                         |
 *                        10k
 *                         |
 *                        GND
 *
 *   DS18B20 DATA -> D23, 4.7k Pull-up nach 3.3V
 *   integriertes SSD1306 OLED -> SDA GPIO21 / SCL GPIO22, Adresse 0x3C
 *
 * Der Teiler aus 10k oben und 2x10k in Reihe nach GND reduziert das PH4502C-Signal auf 2/3.
 * VP ist GPIO36 / ADC1_CH0 und wird nur als Analogeingang benutzt.
 * Das Kalibrier-ESP misst diesen Knoten direkt mit seinem internen ADC.
 *
 * Temperatur:
 * - wird live mitgemessen und auf OLED/Serial angezeigt
 * - wird fuer jeden pH-7-/pH-4-Kalibrierpunkt mitgespeichert
 * - ein Kalibrierpunkt wird ohne gueltige Temperaturmessung nicht akzeptiert
 *
 * WICHTIG:
 * Eine automatische Korrektur des Soll-pH der Pufferloesung wird absichtlich
 * nicht vorgenommen. Diese Korrektur ist vom konkreten Pufferprodukt und
 * dessen Temperaturtabelle abhaengig.
 *
 * Serial Monitor: 115200 Baud
 *   7 = stabilen Mittelwert als pH-7-Punkt erfassen und speichern
 *   4 = stabilen Mittelwert als pH-4-Punkt erfassen und speichern
 *   s = Status / Kalibrierwerte ausgeben
 *   r = gespeicherte Kalibrierpunkte loeschen
 *   h = Hilfe
 */

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

static constexpr uint8_t PIN_PH_ADC = 36;
static constexpr uint8_t PIN_DS18B20 = 23;
static constexpr uint8_t PIN_OLED_SDA = 21;
static constexpr uint8_t PIN_OLED_SCL = 22;
static constexpr uint8_t OLED_ADDR = 0x3C;

static constexpr float R_TOP_OHM = 10000.0f;
static constexpr float R_BOTTOM_OHM = 20000.0f;
static constexpr float DIVIDER_RATIO =
    R_BOTTOM_OHM / (R_TOP_OHM + R_BOTTOM_OHM); // 2/3
static constexpr float DIVIDER_RECONSTRUCT = 1.0f / DIVIDER_RATIO; // 1.5

static constexpr uint16_t LIVE_SAMPLE_COUNT = 32;
static constexpr uint16_t CAPTURE_SAMPLE_COUNT = 160;
static constexpr uint16_t SAMPLE_DELAY_MS = 20;
static constexpr unsigned long TEMP_INTERVAL_MS = 2000;
static constexpr unsigned long OLED_INTERVAL_MS = 500;

static constexpr float CAPTURE_MAX_STD_V = 0.010f;
static constexpr float MIN_CAL_POINT_DISTANCE_V = 0.001f;

static constexpr char NVS_NAMESPACE[] = "phcal";
static constexpr char NVS_HAVE_7[] = "have7";
static constexpr char NVS_HAVE_4[] = "have4";
static constexpr char NVS_VOLT_7[] = "volt7";
static constexpr char NVS_VOLT_4[] = "volt4";
static constexpr char NVS_TEMP_7[] = "temp7";
static constexpr char NVS_TEMP_4[] = "temp4";

Preferences preferences;
OneWire oneWire(PIN_DS18B20);
DallasTemperature temperatureSensors(&oneWire);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

bool oledOk = false;
bool tempSensorPresent = false;

float liveInputV = NAN;
float livePoV = NAN;
float liveStdV = NAN;
float liveTempC = NAN;

bool havePh7 = false;
bool havePh4 = false;
float ph7InputV = NAN;
float ph4InputV = NAN;
float ph7TempC = NAN;
float ph4TempC = NAN;

unsigned long lastTempMs = 0;
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

static SampleStats sampleChannel(uint16_t count, uint16_t delayMs) {
  double sum = 0.0;
  double sumSq = 0.0;
  uint32_t minMv = UINT32_MAX;
  uint32_t maxMv = 0;

  for (uint16_t i = 0; i < count; ++i) {
    const uint32_t mv = analogReadMilliVolts(PIN_PH_ADC);
    const float v = static_cast<float>(mv) / 1000.0f;

    sum += v;
    sumSq += static_cast<double>(v) * static_cast<double>(v);

    if (mv < minMv) minMv = mv;
    if (mv > maxMv) maxMv = mv;

    if (delayMs > 0) delay(delayMs);
  }

  const double mean = sum / count;
  double variance = (sumSq / count) - (mean * mean);
  if (variance < 0.0) variance = 0.0;

  SampleStats s;
  s.meanV = static_cast<float>(mean);
  s.stdV = static_cast<float>(sqrt(variance));
  s.minMv = minMv;
  s.maxMv = maxMv;
  return s;
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
  const float t = temperatureSensors.getTempCByIndex(0);

  if (validTemperature(t)) {
    liveTempC = t;
    tempSensorPresent = true;
  } else {
    liveTempC = NAN;
    tempSensorPresent = false;
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
    ok = ok && (preferences.putFloat(NVS_TEMP_7, ph7TempC) == sizeof(float));
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
       isnan(storedTemp7) || fabsf(storedTemp7 - ph7TempC) > 0.01f)) {
    return false;
  }
  if (havePh4 &&
      (isnan(storedPh4) || fabsf(storedPh4 - ph4InputV) > 0.000001f ||
       isnan(storedTemp4) || fabsf(storedTemp4 - ph4TempC) > 0.01f)) {
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
  Serial.println(F("=== PH4502C Kalibrier-Servicegeraet ==="));
  Serial.println(F("ADC: ESP32 intern an Board-Pin VP, KEIN ADS1115"));
  Serial.println(F("Temperatur: DS18B20 DATA an Board-Pin D23"));
  Serial.println(F("OLED: SSD1306 128x64 auf GPIO21/22"));
  Serial.println(F("7 = pH-7-Punkt erfassen + im NVS speichern"));
  Serial.println(F("4 = pH-4-Punkt erfassen + im NVS speichern"));
  Serial.println(F("s = Status + fertige Kalibrierwerte"));
  Serial.println(F("r = gespeicherte Kalibrierpunkte loeschen"));
  Serial.println(F("h = Hilfe"));
  Serial.println(F("Instabile Messungen >10 mV oder fehlende Temperatur werden nicht gespeichert."));
  Serial.println(F("----------------------------------------"));
}

static void printCalibration() {
  Serial.println();
  Serial.println(F("--- Kalibrierstatus ---"));

  if (havePh7) {
    Serial.printf("pH 7: VP = %.6f V | PH4502C PO ~= %.6f V | T = %.2f C\n",
                  ph7InputV, ph7InputV * DIVIDER_RECONSTRUCT, ph7TempC);
  } else {
    Serial.println(F("pH 7: noch nicht erfasst"));
  }

  if (havePh4) {
    Serial.printf("pH 4: VP = %.6f V | PH4502C PO ~= %.6f V | T = %.2f C\n",
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
  updateTemperature(true);
  if (!validTemperature(liveTempC)) {
    Serial.println(F("ABGELEHNT: keine gueltige DS18B20-Temperaturmessung."));
    return false;
  }

  Serial.printf("\nErfasse pH %.0f bei %.2f C ... Sonde waehrend der Messung ruhig lassen.\n",
                targetPh, liveTempC);

  const SampleStats s = sampleChannel(CAPTURE_SAMPLE_COUNT, SAMPLE_DELAY_MS);

  updateTemperature(true);
  if (!validTemperature(liveTempC)) {
    Serial.println(F("ABGELEHNT: Temperaturmessung nach der Erfassung ungueltig."));
    return false;
  }

  Serial.printf("Mittelwert VP: %.6f V\n", s.meanV);
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

  Serial.println(F("OK: Kalibrierpunkt mit Temperatur dauerhaft gespeichert und verifiziert."));
  printCalibration();
  return true;
}

static void updateLive() {
  const SampleStats s = sampleChannel(LIVE_SAMPLE_COUNT, 0);
  liveInputV = s.meanV;
  livePoV = liveInputV * DIVIDER_RECONSTRUCT;
  liveStdV = s.stdV;
}

static void drawOled() {
  if (!oledOk) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("pH CAL "));
  if (validTemperature(liveTempC)) {
    display.print(liveTempC, 1);
    display.print(F("C"));
  } else {
    display.print(F("T:--"));
  }

  display.setCursor(0, 11);
  display.print(F("ADC "));
  if (isnan(liveInputV)) display.print(F("--"));
  else display.print(liveInputV, 4);
  display.print(F("V"));

  display.setCursor(0, 22);
  display.print(F("PO  "));
  if (isnan(livePoV)) display.print(F("--"));
  else display.print(livePoV, 4);
  display.print(F("V"));

  display.setCursor(0, 33);
  display.print(F("std "));
  if (isnan(liveStdV)) display.print(F("--"));
  else display.print(liveStdV * 1000.0f, 2);
  display.print(F("mV"));

  display.setCursor(0, 44);
  display.print(F("7:"));
  display.print(havePh7 ? F("OK") : F("--"));
  display.print(F(" 4:"));
  display.print(havePh4 ? F("OK") : F("--"));

  display.setCursor(0, 55);
  if (calibrationReady()) {
    const float ph = phFromInputVoltage(liveInputV);
    display.print(F("pH "));
    if (isnan(ph)) display.print(F("--"));
    else display.print(ph, 2);
  } else {
    display.print(F("Serial 7 / 4 / s"));
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

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_PH_ADC, ADC_11db);

  temperatureSensors.begin();
  const uint8_t oneWireCount = temperatureSensors.getDeviceCount();
  Serial.printf("1-Wire: %u Geraet(e) gefunden.\n", static_cast<unsigned>(oneWireCount));
  for (uint8_t i = 0; i < oneWireCount; ++i) {
    DeviceAddress address;
    if (temperatureSensors.getAddress(address, i)) {
      Serial.printf("1-Wire Sensor %u Adresse: ", static_cast<unsigned>(i));
      for (uint8_t j = 0; j < 8; ++j) {
        Serial.printf("%02X", address[j]);
        if (j < 7) Serial.print(":");
      }
      Serial.println();
    } else {
      Serial.printf("1-Wire Sensor %u: Adresse nicht lesbar.\n", static_cast<unsigned>(i));
    }
  }
  temperatureSensors.setResolution(10);
  updateTemperature(true);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL, 100000);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println(F("pH calibration"));
    display.setCursor(0, 34);
    display.println(F("ADC + Temp"));
    display.display();
  }

  const bool nvsReadable = loadCalibration();
  printHelp();

  if (!nvsReadable) {
    Serial.println(F("WARNUNG: NVS konnte nicht geoeffnet werden."));
  }

  if (!tempSensorPresent) {
    Serial.println(F("WARNUNG: DS18B20 auf D23 nicht erkannt."));
  }

  if (!oledOk) {
    Serial.println(F("WARNUNG: SSD1306 OLED auf 0x3C nicht erkannt."));
  }

  if (havePh7 || havePh4) {
    Serial.println(F("Gespeicherte NVS-Kalibrierung geladen."));
    printCalibration();
  } else {
    Serial.println(F("Keine gespeicherten Kalibrierpunkte vorhanden."));
  }

  Serial.println(F("Warte bis LIVE-Wert und Temperatur stabil sind, dann '7' bzw. '4' senden."));
}

void loop() {
  handleSerial();
  updateTemperature();
  updateLive();

  const unsigned long now = millis();

  static unsigned long lastPrint = 0;
  if (now - lastPrint >= 1000) {
    lastPrint = now;

    Serial.printf("LIVE | VP %.6f V | PO %.6f V | std %.3f mV",
                  liveInputV,
                  livePoV,
                  liveStdV * 1000.0f);

    if (validTemperature(liveTempC)) {
      Serial.printf(" | T %.2f C", liveTempC);
    } else {
      Serial.print(F(" | T --"));
    }

    if (calibrationReady()) {
      Serial.printf(" | pH %.3f", phFromInputVoltage(liveInputV));
    }

    Serial.println();
  }

  if (now - lastOledMs >= OLED_INTERVAL_MS) {
    lastOledMs = now;
    drawOled();
  }

  delay(10);
}
