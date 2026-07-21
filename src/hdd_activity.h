#pragma once
#include <Arduino.h>

class HddActivity {
public:
  void update(bool active, bool edgeSeen, uint16_t elapsedMs);
  uint8_t value() const { return value_; }
private:
  uint8_t value_=0;
};