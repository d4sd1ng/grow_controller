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
} // namespace grow_clock
