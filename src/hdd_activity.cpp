#include "hdd_activity.h"

void HddActivity::update(bool active, uint8_t edgeCount, uint32_t elapsedMs) {
  uint32_t next = value_;
  next += static_cast<uint32_t>(edgeCount) * config_.edgeBoost;

  const uint32_t ticks = elapsedMs / config_.updateMs;
  const uint32_t delta = ticks * (active ? config_.activeRise : config_.inactiveDecay);
  if (active) next += delta;
  else next = next > delta ? next - delta : 0;

  value_ = static_cast<uint8_t>(next > config_.maximum ? config_.maximum : next);
}
