#pragma once

#include <stdint.h>

#include "../config.h"

namespace Aurora {

struct Rgb8 {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  bool operator==(const Rgb8 &other) const {
    return r == other.r && g == other.g && b == other.b;
  }
};

uint32_t xorshift32(uint32_t value);

class Field {
 public:
  Field();

  void reset(uint32_t seed);
  void advance(uint32_t elapsedMs);
  Rgb8 pixel(uint8_t index) const;

  uint16_t brightnessQ8_8(uint8_t index) const;
  uint8_t colorProgress(uint8_t index) const;
  uint32_t prngState() const { return prngState_; }
  uint8_t ticksUntilNextSpawn() const { return ticksUntilNextSpawn_; }
  uint32_t fixedStepAccumulatorMs() const { return fixedStepAccumulatorMs_; }

#ifdef PIO_UNIT_TESTING
  void setCellForTest(uint8_t index, uint16_t brightnessQ8_8,
                      uint8_t colorProgress);
#endif

 private:
  void fixedTick();
  void diffuseTick();
  void applyFadeAndColorTick(bool applyFade);
  void spawnStars();
  void scheduleNextSpawn();
  uint32_t nextU32();
  uint8_t rangeInclusive(uint8_t minimum, uint8_t maximum);

  uint16_t brightness_[2][Config::LedCount];
  uint8_t colorProgress_[2][Config::LedCount];
  uint32_t prngState_;
  uint32_t fixedStepAccumulatorMs_;
  uint8_t ticksUntilNextSpawn_;
  uint8_t ticksUntilFade_;
  uint8_t currentBank_;
};

}  // namespace Aurora
