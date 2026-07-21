#include "hdd_activity.h"
#include "config.h"

void HddActivity::update(bool active, bool edgeSeen, uint16_t elapsedMs) {
  uint16_t next = value_;
  if (edgeSeen) next += Config::HddEdgeBoost;

  const uint16_t ticks = elapsedMs / Config::HddUpdateMs;
  const uint16_t delta = ticks * (active ? Config::HddActiveRise : Config::HddInactiveDecay);
  if (active) next += delta;
  else next = next > delta ? next - delta : 0;

  value_ = static_cast<uint8_t>(next > Config::HddMax ? Config::HddMax : next);
}