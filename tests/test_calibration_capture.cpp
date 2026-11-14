#include "../firmware/Sensor/calibration_capture.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float a, float b, float eps=0.0001f) {
  return std::fabs(a-b) <= eps;
}

int main() {
  using namespace grow_cal;
  Stats ph;
  for (int i=0; i<PH_SAMPLES; ++i) assert(ph.add(2.5f + (i%2 ? 0.001f : -0.001f), 3.3f));
  assert(ph.count==160);
  assert(near(ph.mean(),2.5f));
  assert(ph.stdDev() < PH_MAX_STD_V);
  assert(near(ph.maxV-ph.minV, 0.002f));
  assert(!ph.add(NAN,3.3f));
  assert(!ph.add(3.4f,3.3f));

  Stats unstable;
  for (int i=0; i<20; ++i) assert(unstable.add(i%2 ? 2.55f : 2.45f, 3.3f));
  assert(unstable.stdDev() > PH_MAX_STD_V);

  float values[EC_SAMPLES_PER_WINDOW];
  for(int i=0;i<EC_SAMPLES_PER_WINDOW;++i) values[i]=static_cast<float>(30-i);
  assert(near(median30(values),15.5f));

  const float v=0.65f;
  const float base=ecRawMsCm(v,25.0f);
  assert(std::isfinite(base)&&base>0.05f);
  const float factor=ecCalibrationFactor(v,25.0f);
  assert(std::isfinite(factor)&&factor>=0.1f&&factor<=10.0f);
  assert(near(base*factor,EC_REFERENCE_MS_CM,0.00001f));
  assert(std::isnan(ecCalibrationFactor(0.0f,25.0f)));
  assert(std::isnan(ecCalibrationFactor(2.4f,25.0f)));
  assert(std::isnan(ecCalibrationFactor(v,NAN)));
  assert(std::isnan(ecCalibrationFactor(v,90.0f)));

  std::puts("calibration math: PASS");
  return 0;
}
