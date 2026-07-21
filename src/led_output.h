#pragma once
#include <Arduino.h>
#include "fastled_compat.h"
#include "config.h"
#include "state_types.h"

class LedOutput {
public:
  void begin();
  void render(PcState state, TransitionEffect transition, uint32_t transitionStartMs, uint8_t hddActivity, bool stripPowerPresent, uint32_t nowMs);
  CRGB *buffer() { return leds_; }
private:
  CRGB leds_[Config::LedCount];
};
