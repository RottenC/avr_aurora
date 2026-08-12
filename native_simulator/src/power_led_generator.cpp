#include "power_led_generator.h"

#include <algorithm>

void PowerLedGenerator::reset() {
  phaseMs_ = 0;
  level_ = false;
  reconcilePending_ = false;
}

void PowerLedGenerator::setMode(PowerLedSourceMode mode) {
  if (mode == mode_) return;
  const bool involvedBlinking = mode_ == PowerLedSourceMode::Blinking ||
                                mode == PowerLedSourceMode::Blinking;
  mode_ = mode;
  if (mode_ == PowerLedSourceMode::Blinking) phaseMs_ = 0;
  reconcilePending_ = involvedBlinking;
}

void PowerLedGenerator::setHalfPeriodMs(uint32_t value) {
  value = std::max<uint32_t>(100, std::min<uint32_t>(5000, value));
  if (value == halfPeriodMs_) return;
  halfPeriodMs_ = value;
  if (mode_ == PowerLedSourceMode::Blinking) {
    phaseMs_ = 0;
    reconcilePending_ = true;
  }
}

void PowerLedGenerator::append(PowerLedUpdate &result, uint32_t offset,
                               bool active) {
  if (result.transitionCount < result.transitions.size()) {
    result.transitions[result.transitionCount++] = {offset, active};
  }
  level_ = active;
}

PowerLedUpdate PowerLedGenerator::update(uint32_t deltaMs, bool manualLevel) {
  PowerLedUpdate result{};
  bool desired = false;
  if (mode_ == PowerLedSourceMode::Manual) desired = manualLevel;
  if (mode_ == PowerLedSourceMode::On) desired = true;
  if (mode_ == PowerLedSourceMode::Blinking) desired = phaseMs_ >= halfPeriodMs_;
  if (reconcilePending_) {
    reconcilePending_ = false;
    if (desired != level_) append(result, 0, desired);
  }

  if (mode_ != PowerLedSourceMode::Blinking) {
    if (desired != level_) append(result, 0, desired);
    result.level = level_;
    return result;
  }

  const uint32_t period = halfPeriodMs_ * 2U;
  uint32_t elapsed = 0;
  while (elapsed < deltaMs) {
    const uint32_t boundary = halfPeriodMs_ - phaseMs_ % halfPeriodMs_;
    if (boundary > deltaMs - elapsed) {
      phaseMs_ = (phaseMs_ + deltaMs - elapsed) % period;
      elapsed = deltaMs;
    } else {
      elapsed += boundary;
      phaseMs_ = (phaseMs_ + boundary) % period;
      const bool active = phaseMs_ >= halfPeriodMs_;
      if (active != level_) append(result, elapsed, active);
    }
  }
  level_ = phaseMs_ >= halfPeriodMs_;
  result.level = level_;
  return result;
}
