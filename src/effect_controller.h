#pragma once

#include <stdint.h>
#include "state_types.h"

struct EffectControllerConfig {
  uint32_t startupDurationMs;
  uint32_t shutdownDurationMs;
  uint32_t resetDurationMs;
};

class EffectController {
public:
  explicit EffectController(const EffectControllerConfig &config) : config_(config) {}
  void request(TransitionEffect effect, uint32_t nowMs);
  void restart(TransitionEffect effect, uint32_t nowMs);
  void cancel(TransitionEffect effect);
  void cancelAll();
  void reconcile(PcState state);
  void update(uint32_t nowMs);
  TransitionEffect consumeFinished();
  TransitionEffect current() const { return current_; }
  uint32_t startedAt() const { return startedAt_; }
  bool active() const { return current_ != TransitionEffect::None; }

private:
  uint32_t duration(TransitionEffect effect) const;
  static uint8_t priority(TransitionEffect effect);
  static bool isCompatible(TransitionEffect effect, PcState state);

  EffectControllerConfig config_;
  TransitionEffect current_ = TransitionEffect::None;
  TransitionEffect finished_ = TransitionEffect::None;
  uint32_t startedAt_ = 0;
};
