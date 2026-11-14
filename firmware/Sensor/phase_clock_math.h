#pragma once
#include <stdint.h>

namespace grow_clock {
static constexpr uint32_t SECONDS_PER_WEEK = 7UL * 24UL * 3600UL;

struct PhaseWeek {
  uint8_t totalWeek;
  bool finished;
  bool valid;
};

// Total weeks: 1-2 growth, 3-4 transition, 5-13 actual bloom.
// A finished phase stays in its final week until explicitly changed.
inline PhaseWeek evaluate(bool bloom, uint32_t phaseStartUnix, uint32_t nowUnix) {
  if (!phaseStartUnix || nowUnix < phaseStartUnix) return {0, false, false};
  const uint32_t elapsedWeeks = (nowUnix - phaseStartUnix) / SECONDS_PER_WEEK;
  const uint32_t duration = bloom ? 11U : 2U;
  const uint8_t first = bloom ? 3U : 1U;
  const uint32_t bounded = elapsedWeeks < duration ? elapsedWeeks : duration - 1U;
  return {static_cast<uint8_t>(first + bounded), elapsedWeeks >= duration, true};
}
// Ein kompletter Durchlauf: 2 Wochen Wachstum + 11 Wochen Bluete.
// `finished` wird bei >= 13 vollen Wochen gesetzt; danach KEIN Restart.
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
