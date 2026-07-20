#pragma once
#include <Arduino.h>
#include "pc_state.h"

enum class TransitionEffect : uint8_t { None, Startup, Shutdown, ForcedShutdown, Reset };
class EffectController {
public:
  void request(TransitionEffect effect, uint32_t nowMs);
  void update(PcStateMachine &pc, bool stripPowerPresent, uint32_t nowMs);
  TransitionEffect current() const { return current_; }
  uint32_t startedAt() const { return startedAt_; }
  bool active() const { return current_ != TransitionEffect::None; }
private:
  TransitionEffect current_=TransitionEffect::None;
  uint32_t startedAt_=0;
};
const __FlashStringHelper *transitionName(TransitionEffect effect);
