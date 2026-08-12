#include "power_led_tracker.h"

#include "config.h"

void PowerLedTracker::reset() {
  last_ = false;
  initialized_ = false;
  seenOn_ = false;
  lastChangeMs_ = 0;
  lastOnMs_ = 0;
  lastValidBlinkEdgeMs_ = 0;
  blinkEdges_ = 0;
}

void PowerLedTracker::update(SignalState state, uint32_t nowMs) {
  if (state == SignalState::Blinking) {
    initialized_ = true;
    seenOn_ = true;
    lastOnMs_ = nowMs;
    lastValidBlinkEdgeMs_ = nowMs;
    blinkEdges_ = Config::PowerLedBlinkEdgesRequired;
    return;
  }

  const bool active = signalIsHigh(state);
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
    if (interval >= Config::PowerLedBlinkMinHalfPeriodMs &&
        interval <= Config::PowerLedBlinkMaxHalfPeriodMs) {
      if (blinkEdges_ < Config::PowerLedBlinkEdgesRequired) ++blinkEdges_;
      lastValidBlinkEdgeMs_ = nowMs;
    } else {
      blinkEdges_ = 0;
    }
    last_ = active;
    lastChangeMs_ = nowMs;
  }
}

PowerLedMode PowerLedTracker::mode(uint32_t nowMs) const {
  if (blinkEdges_ >= Config::PowerLedBlinkEdgesRequired &&
      nowMs - lastValidBlinkEdgeMs_ <= Config::PowerLedBlinkStaleMs) {
    return PowerLedMode::Blinking;
  }
  if (last_) return PowerLedMode::On;
  return seenOn_ && nowMs - lastOnMs_ < Config::ShortPowerLedOffIgnoreMs
             ? PowerLedMode::On
             : PowerLedMode::Off;
}
