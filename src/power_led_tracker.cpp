#include "power_led_tracker.h"
#include "config_values.h"

void PowerLedTracker::update(bool active, uint32_t nowMs) {
  if (active) lastOnMs_ = nowMs;
  if (active != last_) {
    const uint32_t interval = nowMs - lastChangeMs_;
    if (interval >= FirmwareConfig::PowerLedBlinkMinHalfPeriodMs && interval <= FirmwareConfig::PowerLedBlinkMaxHalfPeriodMs) {
      if (blinkEdges_ < FirmwareConfig::PowerLedBlinkEdgesRequired) ++blinkEdges_;
    } else {
      blinkEdges_ = 0;
    }
    last_ = active;
    lastChangeMs_ = nowMs;
  }
}

PowerLedMode PowerLedTracker::mode(uint32_t nowMs) const {
  if (blinkEdges_ >= FirmwareConfig::PowerLedBlinkEdgesRequired && nowMs - lastChangeMs_ <= FirmwareConfig::PowerLedBlinkStaleMs) return PowerLedMode::Blinking;
  if (last_) return PowerLedMode::On;
  return (nowMs - lastOnMs_ < FirmwareConfig::ShortPowerLedOffIgnoreMs) ? PowerLedMode::On : PowerLedMode::Off;
}
