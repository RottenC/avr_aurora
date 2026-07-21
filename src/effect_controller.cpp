#include "effect_controller.h"

uint8_t EffectController::priority(TransitionEffect effect) {
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
    case TransitionEffect::Startup: return config_.startupDurationMs;
    case TransitionEffect::Shutdown: return config_.shutdownDurationMs;
    case TransitionEffect::Reset: return config_.resetDurationMs;
    case TransitionEffect::ForcedShutdown:
    case TransitionEffect::None: return 0;
  }
  return 0;
}

bool EffectController::isCompatible(TransitionEffect effect, PcState state) {
  switch (effect) {
    case TransitionEffect::Startup:
      return state == PcState::Starting;
    case TransitionEffect::Reset:
      return state == PcState::Running;
    case TransitionEffect::Shutdown:
      return state == PcState::ShuttingDown;
    case TransitionEffect::ForcedShutdown:
      return state == PcState::Running || state == PcState::ShuttingDown;
    case TransitionEffect::None:
      return true;
  }
  return false;
}

void EffectController::request(TransitionEffect effect, uint32_t nowMs) {
  if (effect == TransitionEffect::None) {
    cancelAll();
    return;
  }
  if (current_ == effect) return;
  if (priority(effect) < priority(current_)) return;
  restart(effect, nowMs);
}

void EffectController::restart(TransitionEffect effect, uint32_t nowMs) {
  if (effect == TransitionEffect::None) {
    cancelAll();
    return;
  }
  if (priority(effect) < priority(current_)) return;
  current_ = effect;
  finished_ = TransitionEffect::None;
  startedAt_ = nowMs;
}

void EffectController::cancel(TransitionEffect effect) {
  if (current_ == effect) cancelAll();
}

void EffectController::cancelAll() {
  current_ = TransitionEffect::None;
  finished_ = TransitionEffect::None;
}

void EffectController::reconcile(PcState state) {
  if (!isCompatible(current_, state)) cancelAll();
}

void EffectController::update(uint32_t nowMs) {
  if (current_ == TransitionEffect::None || current_ == TransitionEffect::ForcedShutdown) return;
  if (nowMs - startedAt_ >= duration(current_)) {
    finished_ = current_;
    current_ = TransitionEffect::None;
  }
}

TransitionEffect EffectController::consumeFinished() {
  const TransitionEffect result = finished_;
  finished_ = TransitionEffect::None;
  return result;
}
