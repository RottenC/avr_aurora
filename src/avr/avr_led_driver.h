#pragma once

#include <Arduino.h>

#include "../core/rgb8.h"
#include "../fastled_compat.h"

class AvrLedDriver {
 public:
  void begin();
  void output(const Aurora::Rgb8 *frame, uint8_t count,
              bool stripPowerPresent, bool frameChanged);

 private:
  void enterSafeState();

  CRGB leds_[Aurora::LedCount];
  bool stripPowerWasPresent_ = false;
};
