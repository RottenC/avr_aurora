#pragma once

#include <stdint.h>

#include "../core/rgb8.h"

namespace Aurora {

struct FieldConfig {
  uint16_t fixedStepMs;
  uint8_t spawnMinTicks;
  uint8_t spawnMaxTicks;
  uint8_t spawnMinCount;
  uint8_t spawnMaxCount;
  uint8_t ticksPerFade;
  uint8_t fadeStep;
  uint8_t colorProgressStep;
  uint8_t diffusionSideWeight;
  uint8_t diffusionCenterWeight;
  uint8_t diffusionKernelSum;
  uint32_t backgroundRgb;
  uint32_t color1Rgb;
  uint32_t color2Rgb;
  uint32_t zeroSeedFallback;
};

uint32_t xorshift32(uint32_t value);

class Field {
 public:
  explicit Field(const FieldConfig &config);

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

  FieldConfig config_;
  uint16_t brightness_[2][LedCount];
  uint8_t colorProgress_[2][LedCount];
  uint32_t prngState_;
  uint32_t fixedStepAccumulatorMs_;
  uint8_t ticksUntilNextSpawn_;
  uint8_t ticksUntilFade_;
  uint8_t currentBank_;
};

}  // namespace Aurora
