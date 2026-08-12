#pragma once
#include <stdint.h>

#include "state_types.h"

class HddActivity {
 public:
  void reset() {
    valueQ8_8_ = 0;
    activeRemainder_ = 0;
    inactiveRemainder_ = 0;
  }
  void update(SignalState state, int16_t contribution,
              uint32_t elapsedMs);
  uint8_t value() const { return static_cast<uint8_t>(valueQ8_8_ >> 8); }

 private:
  int32_t valueQ8_8_ = 0;
  uint16_t activeRemainder_ = 0;
  uint16_t inactiveRemainder_ = 0;
};
