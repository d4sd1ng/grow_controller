#include "../firmware/Sensor/phase_clock_math.h"
#include <cassert>
#include <cstdio>

int main() {
  using namespace grow_clock;
  constexpr uint32_t t = 1750000000U;
  assert(!evaluate(false, 0, t).valid);
  assert(!evaluate(false, t, t - 1).valid);
  assert(evaluate(false, t, t).totalWeek == 1);
  assert(evaluate(false, t, t + SECONDS_PER_WEEK - 1).totalWeek == 1);
  assert(evaluate(false, t, t + SECONDS_PER_WEEK).totalWeek == 2);
  assert(!evaluate(false, t, t + 2 * SECONDS_PER_WEEK - 1).finished);
  assert(evaluate(false, t, t + 2 * SECONDS_PER_WEEK).finished);
  assert(evaluate(false, t, t + 2 * SECONDS_PER_WEEK).totalWeek == 2);
  assert(evaluate(true, t, t).totalWeek == 3);
  assert(evaluate(true, t, t + SECONDS_PER_WEEK).totalWeek == 4);
  assert(evaluate(true, t, t + 2 * SECONDS_PER_WEEK).totalWeek == 5);
  assert(evaluate(true, t, t + 10 * SECONDS_PER_WEEK).totalWeek == 13);
  assert(!evaluate(true, t, t + 11 * SECONDS_PER_WEEK - 1).finished);
  assert(evaluate(true, t, t + 11 * SECONDS_PER_WEEK).finished);
  assert(evaluate(true, t, t + 15 * SECONDS_PER_WEEK).totalWeek == 13);
  // End-to-end full 13-week cycle (start W1, automatic W3, final STOP).
  assert(!evaluateCycle(0, t).valid);
  assert(!evaluateCycle(t, t - 1).valid);
  assert(evaluateCycle(t, t).valid);
  assert(evaluateCycle(t, t).totalWeek == 1);
  assert(!evaluateCycle(t, t).bloom);
  assert(!evaluateCycle(t, t + 2 * SECONDS_PER_WEEK - 1).bloom);
  assert(evaluateCycle(t, t + 2 * SECONDS_PER_WEEK).bloom);
  assert(evaluateCycle(t, t + 2 * SECONDS_PER_WEEK).totalWeek == 3);
  assert(evaluateCycle(t, t + 4 * SECONDS_PER_WEEK).totalWeek == 5);
  assert(evaluateCycle(t, t + 12 * SECONDS_PER_WEEK).totalWeek == 13);
  assert(!evaluateCycle(t, t + 13 * SECONDS_PER_WEEK - 1).finished);
  assert(evaluateCycle(t, t + 13 * SECONDS_PER_WEEK).finished);
  assert(evaluateCycle(t, t + 14 * SECONDS_PER_WEEK).finished);
  assert(evaluateCycle(t, t + 18 * SECONDS_PER_WEEK).totalWeek == 13);
  std::puts("phase clock boundaries: PASS");
  return 0;
}
