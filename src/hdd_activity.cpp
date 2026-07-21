#include "hdd_activity.h"

void HddActivity::update(bool active, uint8_t edgeCount, uint16_t elapsedMs) {
  uint16_t next = value_;
  next += static_cast<uint16_t>(edgeCount) * config_.edgeBoost;

  const uint16_t ticks = elapsedMs / config_.updateMs;
  const uint16_t delta = ticks * (active ? config_.activeRise : config_.inactiveDecay);
  if (active) next += delta;
  else next = next > delta ? next - delta : 0;

  value_ = static_cast<uint8_t>(next > config_.maximum ? config_.maximum : next);
}
