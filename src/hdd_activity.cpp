#include "hdd_activity.h"
#include "config.h"
void HddActivity::update(bool active) {
  uint16_t next = value_;
  if (active && !last_) next += Config::HddEdgeBoost;
  next = active ? next + Config::HddActiveRise : (next > Config::HddInactiveDecay ? next - Config::HddInactiveDecay : 0);
  value_ = static_cast<uint8_t>(next > Config::HddMax ? Config::HddMax : next);
  last_ = active;
}
