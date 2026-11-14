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
  std::puts("phase clock boundaries: PASS");
  return 0;
}
