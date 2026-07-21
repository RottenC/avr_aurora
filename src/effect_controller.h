#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "state_types.h"

class EffectController {
public:
  void request(TransitionEffect effect, uint32_t nowMs);
  void restart(TransitionEffect effect, uint32_t nowMs);
  void cancel(TransitionEffect effect);
  void cancel();
  void update(uint32_t nowMs);
  TransitionEffect current() const { return current_; }
  uint32_t startedAt() const { return startedAt_; }
  bool active() const { return current_ != TransitionEffect::None; }
  bool finished() const { return finished_; }
  bool consumeFinished();
private:
  uint32_t duration(TransitionEffect effect) const;
  uint8_t priority(TransitionEffect effect) const;
  TransitionEffect current_ = TransitionEffect::None;
  uint32_t startedAt_ = 0;
  bool finished_ = false;
};
