#pragma once
#include <stdint.h>

namespace grow_clock {
static constexpr uint32_t SECONDS_PER_WEEK = 7UL * 24UL * 3600UL;

// Vollstaendiger Durchlauf: W1-2 Wachstum, W3-4 Blueteeinleitung,
// W5-13 Bluete; nach exakt 13 Wochen STOP.
struct CycleState {
  uint8_t totalWeek;
  bool bloom;
  bool finished;
  bool valid;
};

inline CycleState evaluateCycle(uint32_t cycleStartUnix, uint32_t nowUnix) {
  if (!cycleStartUnix || nowUnix < cycleStartUnix)
    return {0, false, false, false};
  const uint32_t weeks = (nowUnix - cycleStartUnix) / SECONDS_PER_WEEK;
  if (weeks >= 13U) return {13, true, true, true};
  return {static_cast<uint8_t>(weeks + 1U), weeks >= 2U, false, true};
}
} // namespace grow_clock
