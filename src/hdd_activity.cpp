#include "hdd_activity.h"
#include "config_values.h"

namespace {
uint16_t scaleByElapsed(uint8_t amount, uint16_t elapsedMs) {
  const uint16_t updates = elapsedMs == 0 ? 1 : (elapsedMs + FirmwareConfig::HddUpdateMs - 1) / FirmwareConfig::HddUpdateMs;
  return static_cast<uint16_t>(amount) * updates;
}
}

void HddActivity::update(bool active, uint8_t edgeCount, uint16_t elapsedMs) {
  uint16_t next = value_;
  next += static_cast<uint16_t>(edgeCount) * FirmwareConfig::HddEdgeBoost;
  if (active && !last_ && edgeCount == 0) next += FirmwareConfig::HddEdgeBoost;
  if (active) {
    next += scaleByElapsed(FirmwareConfig::HddActiveRise, elapsedMs);
  } else {
    const uint16_t decay = scaleByElapsed(FirmwareConfig::HddInactiveDecay, elapsedMs);
    next = next > decay ? next - decay : 0;
  }
  value_ = static_cast<uint8_t>(next > FirmwareConfig::HddMax ? FirmwareConfig::HddMax : next);
  last_ = active;
}
