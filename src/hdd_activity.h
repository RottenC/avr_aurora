#pragma once

#include <stdint.h>
#include <stdbool.h>

class HddActivity {
public:
  void update(bool active, uint8_t edgeCount);
  uint8_t value() const { return value_; }
private:
  bool last_ = false;
  uint8_t value_ = 0;
};
