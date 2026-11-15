#pragma once
#include <math.h>
#include <stdint.h>

namespace grow_cal {
// Die Schwellwerte entsprechen dem bisherigen ph_calibration_tool.ino.
static constexpr uint16_t PH_SAMPLES = 160;
static constexpr uint32_t PH_INTERVAL_MS = 20;
static constexpr float PH_MAX_STD_V = 0.010f;
static constexpr uint16_t EC_SAMPLES_PER_WINDOW = 30;
static constexpr uint8_t EC_WINDOWS = 8;
static constexpr uint32_t EC_INTERVAL_MS = 40;
static constexpr uint32_t EC_WINDOW_GAP_MS = 200;
static constexpr float EC_MAX_MEDIAN_STD_V = 0.010f;
static constexpr float EC_MAX_MEDIAN_SPAN_V = 0.030f;
static constexpr float EC_REFERENCE_MS_CM = 1.413f; // 1413 uS/cm bei 25 C

struct Stats {
  uint16_t count = 0;
  double sum = 0.0;
  double sumSq = 0.0;
  float minV = INFINITY;
  float maxV = -INFINITY;
  bool add(float voltage, float upperBound) {
    if (!isfinite(voltage) || voltage < 0.0f || voltage > upperBound) return false;
    ++count;
    sum += voltage;
    sumSq += static_cast<double>(voltage) * voltage;
    if (voltage < minV) minV = voltage;
    if (voltage > maxV) maxV = voltage;
    return true;
  }
  float mean() const { return count ? static_cast<float>(sum / count) : NAN; }
  float stdDev() const {
    if (!count) return NAN;
    const double m = sum / count;
    const double variance = sumSq / count - m * m;
    return static_cast<float>(sqrt(variance > 0.0 ? variance : 0.0));
  }
};

inline float median30(float (&values)[EC_SAMPLES_PER_WINDOW]) {
  for (uint16_t i = 1; i < EC_SAMPLES_PER_WINDOW; ++i) {
    const float v = values[i];
    uint16_t j = i;
    while (j > 0 && values[j - 1] > v) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = v;
  }
  return (values[EC_SAMPLES_PER_WINDOW / 2 - 1] +
          values[EC_SAMPLES_PER_WINDOW / 2]) * 0.5f;
}

inline float ecRawMsCm(float voltageV, float measuredWaterC) {
  if (!isfinite(voltageV) || voltageV < 0.0f || voltageV > 2.3f ||
      !isfinite(measuredWaterC) || measuredWaterC <= -20.0f ||
      measuredWaterC >= 80.0f) return NAN;
  const float compensation = 1.0f + 0.02f * (measuredWaterC - 25.0f);
  if (compensation <= 0.1f) return NAN;
  const float v = voltageV / compensation;
  const float raw = (133.42f * v * v * v - 255.86f * v * v + 857.39f * v) / 1000.0f;
  return raw < 0.0f ? 0.0f : raw;
}

inline float ecCalibrationFactor(float referenceVoltageV, float waterTempC) {
  const float raw = ecRawMsCm(referenceVoltageV, waterTempC);
  if (!isfinite(raw) || raw < 0.05f) return NAN;
  const float factor = EC_REFERENCE_MS_CM / raw;
  return isfinite(factor) && factor >= 0.1f && factor <= 10.0f ? factor : NAN;
}
} // namespace grow_cal
