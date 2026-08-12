#pragma once

#include <Arduino.h>

#include "../core/aurora_runtime.h"

class AvrInputs {
 public:
  void begin();
  void update(uint32_t nowMs);
  const AuroraInputFrame &frame() const { return frame_; }
  bool rawStripPowerPresent() const { return rawStripPowerPresent_; }

 private:
  struct Debounced {
    bool stable = false;
    bool candidate = false;
    uint32_t changedAt = 0;
  };

  SignalState updateOne(Debounced &input, bool raw, uint32_t nowMs);

  uint32_t lastPollMs_ = 0;
  Debounced powerLed_;
  Debounced powerButton_;
  Debounced resetButton_;
  Debounced stripPower_;
  AuroraInputFrame frame_{};
  bool hddHigh_ = false;
  bool rawStripPowerPresent_ = false;
};
