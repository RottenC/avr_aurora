#include "power_led_tracker.h"
#include "config.h"

void PowerLedTracker::update(bool active, uint32_t nowMs) {
  if (active) lastOnMs_ = nowMs;
  if (active != last_) {
    const uint32_t interval = nowMs - lastChangeMs_;
    if (interval >= Config::PowerLedBlinkMinHalfPeriodMs && interval <= Config::PowerLedBlinkMaxHalfPeriodMs) {
      if (blinkEdges_ < Config::PowerLedBlinkEdgesRequired) ++blinkEdges_;
    } else {
      blinkEdges_ = 0;
    }
    last_ = active;
    lastChangeMs_ = nowMs;
  }
}
PowerLedMode PowerLedTracker::mode(uint32_t nowMs) const {
  if (blinkEdges_ >= Config::PowerLedBlinkEdgesRequired) return PowerLedMode::Blinking;
  if (last_) return PowerLedMode::On;
  return (nowMs - lastOnMs_ < Config::ShortPowerLedOffIgnoreMs) ? PowerLedMode::On : PowerLedMode::Off;
}
