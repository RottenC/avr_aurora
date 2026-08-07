#include "hdd_activity.h"

#include "config.h"

namespace {

int32_t levelDeltaQ8_8(uint8_t rate, uint16_t updateMs,
                       uint8_t maximum, uint32_t elapsedMs,
                       uint16_t &divisionRemainder) {
  if (rate == 0 || updateMs == 0 || elapsedMs == 0) return 0;

  const uint32_t maximumQ8_8 = static_cast<uint32_t>(maximum) << 8;
  const uint32_t rateQ8_8 = static_cast<uint32_t>(rate) << 8;
  const uint32_t maximumNumerator = maximumQ8_8 * updateMs;
  if (elapsedMs >
      (maximumNumerator - divisionRemainder) / rateQ8_8) {
    divisionRemainder = 0;
    return static_cast<int32_t>(maximumQ8_8);
  }

  const uint32_t numerator =
      elapsedMs * rateQ8_8 + divisionRemainder;
  divisionRemainder = static_cast<uint16_t>(numerator % updateMs);
  return static_cast<int32_t>(numerator / updateMs);
}

}  // namespace

void HddActivity::update(SignalState state, int16_t contribution,
                         uint32_t elapsedMs) {
  int32_t deltaQ8_8 = contribution;
  if (signalIsHigh(state)) {
    deltaQ8_8 += levelDeltaQ8_8(Config::HddActiveRise, Config::HddUpdateMs,
                                Config::HddMax, elapsedMs,
                                activeRemainder_);
  } else if (state != SignalState::Blinking) {
    deltaQ8_8 -= levelDeltaQ8_8(Config::HddInactiveDecay,
                                Config::HddUpdateMs, Config::HddMax, elapsedMs,
                                inactiveRemainder_);
  }

  int32_t nextQ8_8 = valueQ8_8_ + deltaQ8_8;
  const int32_t maximumQ8_8 = static_cast<int32_t>(Config::HddMax) << 8;
  if (nextQ8_8 <= 0) {
    nextQ8_8 = 0;
    inactiveRemainder_ = 0;
  }
  if (nextQ8_8 >= maximumQ8_8) {
    nextQ8_8 = maximumQ8_8;
    activeRemainder_ = 0;
  }

  valueQ8_8_ = nextQ8_8;
}
