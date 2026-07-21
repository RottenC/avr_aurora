#include "effect_controller.h"
#include "config_values.h"

uint8_t EffectController::priority(TransitionEffect effect) const {
  switch (effect) {
    case TransitionEffect::ForcedShutdown: return 4;
    case TransitionEffect::Shutdown: return 3;
    case TransitionEffect::Reset: return 2;
    case TransitionEffect::Startup: return 1;
    case TransitionEffect::None: return 0;
  }
  return 0;
}

uint32_t EffectController::duration(TransitionEffect effect) const {
  switch (effect) {
    case TransitionEffect::Startup: return FirmwareConfig::StartupDurationMs;
    case TransitionEffect::Shutdown: return FirmwareConfig::ShutdownDurationMs;
    case TransitionEffect::Reset: return FirmwareConfig::ResetDurationMs;
    case TransitionEffect::ForcedShutdown: return FirmwareConfig::PowerHoldForcedMs;
    case TransitionEffect::None: return 0;
  }
  return 0;
}

void EffectController::request(TransitionEffect effect, uint32_t nowMs) {
  if (effect == TransitionEffect::None) { cancel(); return; }
  if (current_ != TransitionEffect::None && priority(effect) < priority(current_)) return;
  if (current_ == effect && effect == TransitionEffect::ForcedShutdown) return;
  current_ = effect;
  startedAt_ = nowMs;
  finished_ = false;
}

void EffectController::cancel(TransitionEffect effect) { if (current_ == effect) cancel(); }
void EffectController::cancel() { current_ = TransitionEffect::None; finished_ = false; }

void EffectController::update(uint32_t nowMs) {
  if (current_ == TransitionEffect::None || current_ == TransitionEffect::ForcedShutdown) return;
  if (nowMs - startedAt_ >= duration(current_)) { current_ = TransitionEffect::None; finished_ = true; }
}

bool EffectController::consumeFinished() {
  const bool wasFinished = finished_;
  finished_ = false;
  return wasFinished;
}
