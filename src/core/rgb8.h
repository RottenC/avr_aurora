#pragma once

#include <stdint.h>

namespace Aurora {

struct Rgb8 {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  bool operator==(const Rgb8 &other) const {
    return r == other.r && g == other.g && b == other.b;
  }

  bool operator!=(const Rgb8 &other) const { return !(*this == other); }
};

}  // namespace Aurora
