#pragma once

#include <Arduino.h>

#include "../core/geometry.h"
#include "../core/rgb8.h"
#include "../fastled_compat.h"
#include "strip_power_safety.h"

class AvrLedDriver {
 public:
  AvrLedDriver();

  void begin();
  bool updatePowerState(bool rawStripPowerPresent,
                        bool logicalStripPowerPresent, uint32_t nowMs);
  void output(const Aurora::Rgb8 *frame, uint8_t count, bool frameUpdated);

 private:
  void enterSafeState();

  CRGB leds_[Aurora::LedCount];
  StripPowerSafety stripPowerSafety_;
  bool outputAllowed_ = false;
  bool waitingForFreshFrame_ = true;
  bool dataOutputWasEnabled_ = false;
};
