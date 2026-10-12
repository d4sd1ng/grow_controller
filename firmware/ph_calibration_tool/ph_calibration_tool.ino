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
 * Kalibriert wird erst mit gueltiger DS18B20-Temperatur.
 */
 
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

static SampleStats sampleChannel(uint16_t count, uint16_t delayMs) {
  SampleStats empty{NAN, NAN, 0, 0};
  if (!adsOk || count == 0) return empty;
  double sum = 0.0;
  double sumSq = 0.0;
  uint32_t minMv = UINT32_MAX;
  uint32_t maxMv = 0;

  for (uint16_t i = 0; i < count; ++i) {
    const int16_t raw = ads.readADC_SingleEnded(0);
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

static void printHardwareStatus() {
  Serial.println(F("\n=== HARDWARE-STATUS ==="));
  Serial.printf("OLED 0x3C : %s\n", oledOk ? "OK" : "NICHT ERKANNT");
  Serial.printf("ADS  0x48: %s\n", adsOk ? "OK" : "NICHT ERKANNT");
  if (adsOk) {
    Serial.printf("ADS A0 pH: %.5f V | PO (geschaetzt): %.5f V | Std: %.2f mV\n",
                  liveInputV, livePoV, liveStdV * 1000.0f);
    Serial.printf("ADS A1 TDS: %.5f V (roh, keine EC-Kalibrierung)\n", liveTdsV);
  }
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
  Serial.println(F("=== Grow-Controller Hardwaretest + pH-Kalibrierung ==="));
  Serial.println(F("ADS1115: A0 = pH, A1 = TDS; SDA D21, SCL D22"));
  Serial.println(F("DS18B20: D23 | DHT22: D25/26/27 | BH1750: D13/14"));
  Serial.println(F("OLED: 0x3C, D21/D22; SSR-Ausgaenge bleiben AUS"));
  Serial.println(F("7 = pH-7-Punkt erfassen + im NVS speichern"));
  Serial.println(F("4 = pH-4-Punkt erfassen + im NVS speichern"));
  Serial.println(F("s = Status + fertige Kalibrierwerte"));
  Serial.println(F("r = gespeicherte Kalibrierpunkte loeschen"));
  Serial.println(F("t = Hardware-Status (alle Sensoren)"));
  Serial.println(F("h = Hilfe"));
  Serial.println(F("Instabile Messungen >10 mV oder fehlende Temperatur werden nicht gespeichert."));
  Serial.println(F("----------------------------------------"));
}

static void printCalibration() {
  Serial.println();
  Serial.println(F("--- Kalibrierstatus ---"));

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
  if (!validTemperature(liveTempC)) {
    Serial.println(F("ABGELEHNT: keine gueltige DS18B20-Temperaturmessung."));
    return false;
  }

  Serial.printf("\nErfasse pH %.0f bei %.2f C ... Sonde waehrend der Messung ruhig lassen.\n",
                targetPh, liveTempC);

  const SampleStats s = sampleChannel(CAPTURE_SAMPLE_COUNT, SAMPLE_DELAY_MS);
  if (!isfinite(s.meanV) || !isfinite(s.stdV)) {
    Serial.println(F("ABGELEHNT: ADS1115-Auslesefehler."));
    return false;
  }

  updateTemperature(true);
  if (!validTemperature(liveTempC)) {
    Serial.println(F("ABGELEHNT: Temperaturmessung nach der Erfassung ungueltig."));
    return false;
  }

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

  Serial.println(F("OK: Kalibrierpunkt mit Temperatur dauerhaft gespeichert und verifiziert."));
  printCalibration();
  return true;
}

static void updateLive() {
  const SampleStats s = sampleChannel(LIVE_SAMPLE_COUNT, 0);
  liveInputV = s.meanV;
  livePoV = isfinite(liveInputV) ? liveInputV * DIVIDER_RECONSTRUCT : NAN;
  liveStdV = s.stdV;
  liveTdsV = adsOk ? ads.computeVolts(ads.readADC_SingleEnded(1)) : NAN;
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
  display.print(F("TDS A1:"));
  if (isfinite(liveTdsV)) display.print(liveTdsV, 4);
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
