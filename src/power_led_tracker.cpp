#include "power_led_tracker.h"

void PowerLedTracker::update(bool active, uint32_t nowMs) {
  if (!initialized_) {
    initialized_ = true;
    last_ = active;
    lastChangeMs_ = nowMs;
    if (active) {
      seenOn_ = true;
      lastOnMs_ = nowMs;
    }
    return;
  }

  if (active) {
    seenOn_ = true;
    lastOnMs_ = nowMs;
  }
  if (active != last_) {
    const uint32_t interval = nowMs - lastChangeMs_;
    if (interval >= config_.blinkMinHalfPeriodMs && interval <= config_.blinkMaxHalfPeriodMs) {
      if (blinkEdges_ < config_.blinkEdgesRequired) ++blinkEdges_;
      lastValidBlinkEdgeMs_ = nowMs;
    } else {
      blinkEdges_ = 0;
    }
    last_ = active;
    lastChangeMs_ = nowMs;
  }
}

PowerLedMode PowerLedTracker::mode(uint32_t nowMs) const {
  if (blinkEdges_ >= config_.blinkEdgesRequired &&
      nowMs - lastValidBlinkEdgeMs_ <= config_.blinkStaleMs) {
    return PowerLedMode::Blinking;
  }
  if (last_) return PowerLedMode::On;
  return seenOn_ && nowMs - lastOnMs_ < config_.shortOffIgnoreMs ? PowerLedMode::On : PowerLedMode::Off;
}
