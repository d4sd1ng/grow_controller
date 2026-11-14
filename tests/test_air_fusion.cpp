#include "../firmware/Sensor/air_fusion.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float actual, float expected) {
  return std::fabs(actual - expected) < 0.0001f;
}
int main() {
  using namespace grow_air;

  // Drei gueltige Sensoren: Median fuer Temperatur und RH, Ausreisser unterdrueckt.
  const Reading all[3] = {{23.0f, 60.0f, true},
                          {24.0f, 70.0f, true},
                          {31.0f, 50.0f, true}};
  const Fusion m = fuseThree(all);
  assert(m.valid && m.validCount == 3);
  assert(near(m.tempC, 24.0f) && near(m.humidityPct, 60.0f));

  // Zwei gueltige: Mittelwert und Ausfallmeldung durch count=2.
  const Reading two[3] = {{22.0f, 56.0f, true},
                          {NAN, NAN, false},
                          {26.0f, 64.0f, true}};
  const Fusion avg = fuseThree(two);
  assert(avg.valid && avg.validCount == 2);
  assert(near(avg.tempC, 24.0f) && near(avg.humidityPct, 60.0f));

  // Einer gueltig: Einzelmessung, Warnstatus wird per Count=1 signalisiert.
  const Reading one[3] = {{NAN, NAN, false},
                          {25.0f, 58.0f, true},
                          {NAN, NAN, false}};
  const Fusion single = fuseThree(one);
  assert(single.valid && single.validCount == 1);
  assert(near(single.tempC, 25.0f) && near(single.humidityPct, 58.0f));

  // Keiner gueltig: NAN, keine Klimaregelung.
  const Reading none[3] = {{NAN, NAN, false},
                           {NAN, NAN, false},
                           {NAN, NAN, false}};
  const Fusion empty = fuseThree(none);
  assert(!empty.valid && empty.validCount == 0);
  assert(std::isnan(empty.tempC) && std::isnan(empty.humidityPct));

  // Nichtfinite und unplausible Werte werden selbst bei valid=true verworfen.
  const Reading invalid[3] = {{24.0f, NAN, true},
                              {80.0f, 55.0f, true},
                              {24.0f, 62.0f, true}};
  const Fusion filtered = fuseThree(invalid);
  assert(filtered.valid && filtered.validCount == 1);
  assert(near(filtered.tempC, 24.0f) && near(filtered.humidityPct, 62.0f));

  std::puts("DHT22 fusion regression tests: PASS");
  return 0;
}
