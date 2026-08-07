#pragma once

#include <stdint.h>
#include "state_types.h"

class EffectController {
public:
  void reset();
  void request(TransitionEffect effect, uint32_t nowMs);
  void restart(TransitionEffect effect, uint32_t nowMs);
  void cancel(TransitionEffect effect);
  void cancelAll();
  void reconcile(PcState state);
  void update(uint32_t nowMs);
  TransitionEffect consumeFinished();
  TransitionEffect current() const { return current_; }
  uint32_t startedAt() const { return startedAt_; }
  uint32_t durationFor(TransitionEffect effect) const;
  bool active() const { return current_ != TransitionEffect::None; }

private:
  static uint8_t priority(TransitionEffect effect);
  static bool isCompatible(TransitionEffect effect, PcState state);

  TransitionEffect current_ = TransitionEffect::None;
  TransitionEffect finished_ = TransitionEffect::None;
  uint32_t startedAt_ = 0;
};
