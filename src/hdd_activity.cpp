#include "hdd_activity.h"
#include "config_values.h"

void HddActivity::update(bool active, uint8_t edgeCount) {
  uint16_t next = value_;
  next += static_cast<uint16_t>(edgeCount) * FirmwareConfig::HddEdgeBoost;
  if (active && !last_ && edgeCount == 0) next += FirmwareConfig::HddEdgeBoost;
  next = active ? next + FirmwareConfig::HddActiveRise : (next > FirmwareConfig::HddInactiveDecay ? next - FirmwareConfig::HddInactiveDecay : 0);
  value_ = static_cast<uint8_t>(next > FirmwareConfig::HddMax ? FirmwareConfig::HddMax : next);
  last_ = active;
}
