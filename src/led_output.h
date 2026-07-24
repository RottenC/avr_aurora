#pragma once
#include <Arduino.h>
#include "fastled_compat.h"
#include "config.h"
#include "pc_state.h"
#include "effect_controller.h"
#include "effects/aurora_field.h"

class LedOutput {
public:
  void begin(uint32_t auroraSeed);
  void render(PcState state, TransitionEffect transition, uint32_t transitionStartMs, uint8_t hddActivity, bool stripPowerPresent, uint32_t nowMs);
  CRGB *buffer() { return leds_; }
private:
  void deactivateAurora();
  void renderAuroraField(uint32_t nowMs);

  CRGB leds_[Config::LedCount];
  Aurora::Field aurora_;
  uint32_t auroraSeed_ = Config::AuroraZeroSeedFallback;
  uint32_t lastAuroraUpdateMs_ = 0;
  bool auroraActive_ = false;
};
