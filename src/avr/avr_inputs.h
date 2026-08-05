#pragma once

#include <Arduino.h>

#include "../core/aurora_runtime.h"

class AvrInputs {
 public:
  void begin();
  void update(uint32_t nowMs);
  const AuroraInputFrame &frame() const { return frame_; }

 private:
  struct Debounced {
    bool stable = false;
    bool candidate = false;
    uint32_t changedAt = 0;
  };

  void updateOne(Debounced &input, bool raw, uint32_t nowMs, bool &pressed,
                 bool &released);
  static uint8_t consumeHddEdges();

  uint32_t lastPollMs_ = 0;
  Debounced powerLed_;
  Debounced powerButton_;
  Debounced resetButton_;
  Debounced stripPower_;
  Debounced debugButton_;
  AuroraInputFrame frame_{};
};
