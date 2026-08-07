#pragma once

#include <stdint.h>

#include "../core/geometry.h"
#include "../core/rgb8.h"

namespace Aurora
{

  struct FieldCellDiagnostics
  {
    uint16_t brightnessQ8_8 = 0;
    uint16_t backgroundBrightnessQ8_8 = 0;
    uint8_t colorProgress = 0;
    Rgb8 color{};
  };

  uint32_t xorshift32(uint32_t value);

  class Field
  {
  public:
    Field();

    void reset(uint32_t seed);
    void advance(uint32_t elapsedMs, uint8_t hddActivity = 0,
                 uint32_t nowMs = 0);
    Rgb8 pixel(uint8_t index) const;
    FieldCellDiagnostics diagnostics(uint8_t index) const;

    uint16_t brightnessQ8_8(uint8_t index) const;
    uint16_t backgroundBrightnessQ8_8(uint8_t index) const;
    uint8_t colorProgress(uint8_t index) const;
    uint32_t prngState() const { return prngState_; }
    uint8_t ticksUntilNextSpawn() const { return ticksUntilNextSpawn_; }
    uint8_t spawnRateRemainderQ0_8() const
    {
      return spawnRateRemainderQ0_8_;
    }
    uint32_t fixedStepAccumulatorMs() const { return fixedStepAccumulatorMs_; }

#ifdef PIO_UNIT_TESTING
    void setCellForTest(uint8_t index, uint16_t brightnessQ8_8,
                        uint8_t colorProgress);
#endif

  private:
    void fixedTick(uint8_t hddActivity);
    void diffuseTick();
    void applyFadeAndColorTick(bool applyFade);
    void updateBackground(uint32_t elapsedMs, uint8_t hddActivity,
                          uint32_t nowMs);
    void updateBackgroundActivity(uint32_t elapsedMs, uint8_t hddActivity);
    void advanceSpawnCountdown(uint8_t hddActivity);
    void spawnStars();
    void scheduleNextSpawn();
    uint32_t nextU32();
    uint8_t rangeInclusive(uint8_t minimum, uint8_t maximum);

    uint16_t brightness_[2][LedCount];
    uint16_t backgroundBrightness_[LedCount];
    uint8_t colorProgress_[2][LedCount];
    uint32_t prngState_;
    uint32_t fixedStepAccumulatorMs_;
    uint16_t backgroundActivityQ8_8_;
    uint16_t backgroundReleaseRemainder_;
    uint8_t ticksUntilNextSpawn_;
    uint8_t ticksUntilFade_;
    uint8_t spawnRateRemainderQ0_8_;
    uint8_t currentBank_;
  };

} // namespace Aurora
