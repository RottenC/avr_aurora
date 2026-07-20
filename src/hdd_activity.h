#pragma once
#include <Arduino.h>
class HddActivity {
public:
  void update(bool active);
  uint8_t value() const { return value_; }
private:
  bool last_=false;
  uint8_t value_=0;
};
