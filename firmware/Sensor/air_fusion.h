#pragma once
#include <math.h>
#include <stdint.h>

namespace grow_air {

struct Reading {
  float tempC;
  float humidityPct;
  bool valid;
};

struct Fusion {
  float tempC;
  float humidityPct;
  uint8_t validCount;
  bool valid;
};

inline float medianThree(float a, float b, float c) {
  if (a > b) { const float t = a; a = b; b = t; }
  if (b > c) { const float t = b; b = c; c = t; }
  if (a > b) { const float t = a; a = b; b = t; }
  return b;
}

// Drei gueltige DHT22 -> Median beider Messgroessen.
// Zwei -> arithmetisches Mittel; einer -> Einzelwert; keiner -> ungueltig.
// Temperatur und Feuchte stammen immer aus denselben gueltigen Sensorpaaren.
inline Fusion fuseThree(const Reading (&readings)[3]) {
  float temps[3] = {};
  float humidities[3] = {};
  uint8_t count = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    const Reading &r = readings[i];
    if (!r.valid || !isfinite(r.tempC) || !isfinite(r.humidityPct) ||
        r.tempC <= -20.0f || r.tempC >= 70.0f ||
        r.humidityPct < 0.0f || r.humidityPct > 100.0f) continue;
    temps[count] = r.tempC;
    humidities[count] = r.humidityPct;
    ++count;
  }
  if (count == 0) return {NAN, NAN, 0, false};
  if (count == 1) return {temps[0], humidities[0], 1, true};
  if (count == 2)
    return {0.5f * (temps[0] + temps[1]),
            0.5f * (humidities[0] + humidities[1]), 2, true};
  return {medianThree(temps[0], temps[1], temps[2]),
          medianThree(humidities[0], humidities[1], humidities[2]), 3, true};
}
} // namespace grow_air
