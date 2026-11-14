#include "../firmware/Sensor/phase_clock_math.h"
#include <cassert>
#include <cstdio>

int main() {
  using namespace grow_clock;
  constexpr uint32_t t = 1750000000U;

  assert(!evaluateCycle(0, t).valid);
  assert(!evaluateCycle(t, t - 1).valid);
  assert(evaluateCycle(t, t).valid);
  assert(evaluateCycle(t, t).totalWeek == 1);
  assert(!evaluateCycle(t, t).bloom);

  // W1/W2 Wachstum, nach exakt 14 Tagen automatischer Bluetewechsel.
  assert(evaluateCycle(t, t + SECONDS_PER_WEEK - 1).totalWeek == 1);
  assert(evaluateCycle(t, t + SECONDS_PER_WEEK).totalWeek == 2);
  assert(!evaluateCycle(t, t + 2 * SECONDS_PER_WEEK - 1).bloom);
  assert(evaluateCycle(t, t + 2 * SECONDS_PER_WEEK).bloom);
  assert(evaluateCycle(t, t + 2 * SECONDS_PER_WEEK).totalWeek == 3);

  // Erste zwei Bluetewochen Uebergang, danach eigentliche Bluete.
  assert(evaluateCycle(t, t + 3 * SECONDS_PER_WEEK).totalWeek == 4);
  assert(evaluateCycle(t, t + 4 * SECONDS_PER_WEEK).totalWeek == 5);

  // Voller Durchlauf dauert 13 Wochen bzw. 91 Tage.
  assert(evaluateCycle(t, t + 12 * SECONDS_PER_WEEK).totalWeek == 13);
  assert(!evaluateCycle(t, t + 13 * SECONDS_PER_WEEK - 1).finished);
  assert(evaluateCycle(t, t + 13 * SECONDS_PER_WEEK).finished);
  assert(evaluateCycle(t, t + 14 * SECONDS_PER_WEEK).finished);
  assert(evaluateCycle(t, t + 18 * SECONDS_PER_WEEK).totalWeek == 13);
  std::puts("automatic 13-week cycle boundaries: PASS");
  return 0;
}
