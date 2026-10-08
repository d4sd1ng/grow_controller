/**
 * PH4502C Kalibrier-Servicegeraet ohne ADS1115
 *
 * Hardware:
 *   PH4502C VCC -> 5V
 *   PH4502C GND -> GND
 *   PH4502C PO  -> 10k -> GPIO34
 *                         |
 *                        20k
 *                         |
 *                        GND
 *
 * Der 10k/20k-Teiler reduziert das PH4502C-Signal auf 2/3.
 * Das Kalibrier-ESP misst diesen Knoten direkt mit seinem internen ADC.
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
#include <math.h>

static constexpr uint8_t PIN_PH_ADC = 34;

static constexpr float R_TOP_OHM = 10000.0f;
static constexpr float R_BOTTOM_OHM = 20000.0f;
static constexpr float DIVIDER_RATIO =
    R_BOTTOM_OHM / (R_TOP_OHM + R_BOTTOM_OHM); // 2/3
static constexpr float DIVIDER_RECONSTRUCT = 1.0f / DIVIDER_RATIO; // 1.5

static constexpr uint16_t LIVE_SAMPLE_COUNT = 32;
static constexpr uint16_t CAPTURE_SAMPLE_COUNT = 160;
static constexpr uint16_t SAMPLE_DELAY_MS = 20;

static constexpr float CAPTURE_MAX_STD_V = 0.010f;
static constexpr float MIN_CAL_POINT_DISTANCE_V = 0.001f;

static constexpr char NVS_NAMESPACE[] = "phcal";
static constexpr char NVS_HAVE_7[] = "have7";
static constexpr char NVS_HAVE_4[] = "have4";
static constexpr char NVS_VOLT_7[] = "volt7";
static constexpr char NVS_VOLT_4[] = "volt4";

Preferences preferences;

float liveInputV = NAN;
float livePoV = NAN;
float liveStdV = NAN;

bool havePh7 = false;
bool havePh4 = false;
float ph7InputV = NAN;
float ph4InputV = NAN;

struct SampleStats {
  float meanV;
  float stdV;
  uint32_t minMv;
  uint32_t maxMv;
};

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

static bool loadCalibration() {
  if (!preferences.begin(NVS_NAMESPACE, true)) {
    havePh7 = false;
    havePh4 = false;
    ph7InputV = NAN;
    ph4InputV = NAN;
    return false;
  }

  havePh7 = preferences.getBool(NVS_HAVE_7, false);
  havePh4 = preferences.getBool(NVS_HAVE_4, false);
  ph7InputV = preferences.getFloat(NVS_VOLT_7, NAN);
  ph4InputV = preferences.getFloat(NVS_VOLT_4, NAN);
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
  } else if (preferences.isKey(NVS_VOLT_7)) {
    ok = ok && preferences.remove(NVS_VOLT_7);
  }

  if (havePh4) {
    ok = ok && (preferences.putFloat(NVS_VOLT_4, ph4InputV) == sizeof(float));
  } else if (preferences.isKey(NVS_VOLT_4)) {
    ok = ok && preferences.remove(NVS_VOLT_4);
  }

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
  if (havePh7 &&
      (isnan(storedPh7) || fabsf(storedPh7 - ph7InputV) > 0.000001f)) {
    return false;
  }
  if (havePh4 &&
      (isnan(storedPh4) || fabsf(storedPh4 - ph4InputV) > 0.000001f)) {
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
  return true;
}

static void printHelp() {
  Serial.println();
  Serial.println(F("=== PH4502C Kalibrier-Servicegeraet ==="));
  Serial.println(F("ADC: ESP32 intern auf GPIO34, KEIN ADS1115"));
  Serial.println(F("7 = pH-7-Punkt erfassen + im NVS speichern"));
  Serial.println(F("4 = pH-4-Punkt erfassen + im NVS speichern"));
  Serial.println(F("s = Status + fertige Kalibrierwerte"));
  Serial.println(F("r = gespeicherte Kalibrierpunkte loeschen"));
  Serial.println(F("h = Hilfe"));
  Serial.println(F("Instabile Messungen >10 mV Std-Abw. werden nicht gespeichert."));
  Serial.println(F("----------------------------------------"));
}

static void printCalibration() {
  Serial.println();
  Serial.println(F("--- Kalibrierstatus ---"));

  if (havePh7) {
    Serial.printf("pH 7: GPIO34 = %.6f V | PH4502C PO ~= %.6f V\n",
                  ph7InputV, ph7InputV * DIVIDER_RECONSTRUCT);
  } else {
    Serial.println(F("pH 7: noch nicht erfasst"));
  }

  if (havePh4) {
    Serial.printf("pH 4: GPIO34 = %.6f V | PH4502C PO ~= %.6f V\n",
                  ph4InputV, ph4InputV * DIVIDER_RECONSTRUCT);
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
  } else if (havePh7 && havePh4) {
    Serial.println(F("FEHLER: Kalibrierpunkte liegen zu dicht beieinander."));
  }

  Serial.println(F("-----------------------"));
}

static bool captureStablePoint(float targetPh) {
  Serial.printf("\nErfasse pH %.0f ... Sonde waehrend der Messung ruhig lassen.\n",
                targetPh);

  const SampleStats s = sampleChannel(CAPTURE_SAMPLE_COUNT, SAMPLE_DELAY_MS);

  Serial.printf("Mittelwert GPIO34: %.6f V\n", s.meanV);
  Serial.printf("Std-Abw.:          %.6f V\n", s.stdV);
  Serial.printf("PH4502C PO:        %.6f V (rueckgerechnet)\n",
                s.meanV * DIVIDER_RECONSTRUCT);
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

  if (targetPh == 7.0f) {
    ph7InputV = s.meanV;
    havePh7 = true;
  } else if (targetPh == 4.0f) {
    ph4InputV = s.meanV;
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
    Serial.println(F("FEHLER: NVS-Speicherung fehlgeschlagen; alter Zustand wiederhergestellt."));
    return false;
  }

  Serial.println(F("OK: Kalibrierpunkt dauerhaft gespeichert und verifiziert."));
  printCalibration();
  return true;
}

static void updateLive() {
  const SampleStats s = sampleChannel(LIVE_SAMPLE_COUNT, 0);
  liveInputV = s.meanV;
  livePoV = liveInputV * DIVIDER_RECONSTRUCT;
  liveStdV = s.stdV;
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

  const bool nvsReadable = loadCalibration();
  printHelp();

  if (!nvsReadable) {
    Serial.println(F("WARNUNG: NVS konnte nicht geoeffnet werden."));
  }

  if (havePh7 || havePh4) {
    Serial.println(F("Gespeicherte NVS-Kalibrierung geladen."));
    printCalibration();
  } else {
    Serial.println(F("Keine gespeicherten Kalibrierpunkte vorhanden."));
  }

  Serial.println(F("Warte bis der LIVE-Wert stabil ist, dann '7' bzw. '4' senden."));
}

void loop() {
  handleSerial();
  updateLive();

  static unsigned long lastPrint = 0;
  const unsigned long now = millis();

  if (now - lastPrint >= 1000) {
    lastPrint = now;

    Serial.printf("LIVE | GPIO34 %.6f V | PO %.6f V | std %.3f mV",
                  liveInputV,
                  livePoV,
                  liveStdV * 1000.0f);

    if (calibrationReady()) {
      Serial.printf(" | pH %.3f", phFromInputVoltage(liveInputV));
    }

    Serial.println();
  }

  delay(10);
}
