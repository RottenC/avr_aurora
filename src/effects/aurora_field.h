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
    struct IgnitionTestState
    {
      bool active = false;
      uint8_t position = 0;
      uint8_t peakBrightness = 0;
      uint8_t durationTicks = 0;
      uint8_t elapsedTicks = 0;
      uint8_t radius = 0;
    };

    void setCellForTest(uint8_t index, uint16_t brightnessQ8_8,
                        uint8_t colorProgress);
    void spawnIgnitionForTest(uint8_t position, uint8_t peakBrightness,
                              uint8_t durationTicks, uint8_t radius);
    void advanceIgnitionsForTest();
    void setSpawnCountdownForTest(uint8_t ticksUntilNextSpawn);
    uint8_t activeIgnitionCountForTest() const;
    IgnitionTestState ignitionForTest(uint8_t slot) const;
    static uint8_t ignitionEaseForTest(uint8_t elapsedTicks,
                                       uint8_t durationTicks);
    static uint8_t ignitionSpatialWeightForTest(uint8_t radius,
                                                uint8_t distance);
    static uint16_t ignitionTargetBrightnessForTest(
        uint8_t peakBrightness, uint8_t easedProgress);
#endif

  private:
    struct Ignition
    {
      uint8_t position;
      uint8_t peakBrightness;
      uint8_t durationTicks;
      uint8_t elapsedTicks;
      uint8_t radius;
    };

    void fixedTick(uint8_t hddActivity);
    void diffuseTick();
    void applyFadeAndColorTick(bool applyFade);
    void advanceIgnitions();
    void applyIgnitionColorCap(const Ignition &ignition,
                               uint8_t easedProgress);
    void updateBackground(uint32_t elapsedMs, uint8_t hddActivity,
                          uint32_t nowMs);
    void updateBackgroundActivity(uint32_t elapsedMs, uint8_t hddActivity);
    void advanceSpawnCountdown(uint8_t hddActivity);
    void spawnStars();
    void spawnIgnition(uint8_t position, uint8_t peakBrightness,
                       uint8_t durationTicks, uint8_t radius);
    uint8_t selectIgnitionSlot() const;
    void scheduleNextSpawn();
    uint32_t nextU32();
    uint8_t rangeInclusive(uint8_t minimum, uint8_t maximum);

    uint16_t brightness_[2][LedCount];
    uint16_t backgroundBrightness_[LedCount];
    uint8_t colorProgress_[2][LedCount];
    Ignition ignitions_[Config::AuroraIgnitionCapacity];
    uint32_t prngState_;
    uint32_t fixedStepAccumulatorMs_;
    uint16_t backgroundActivityQ8_8_;
    uint16_t backgroundReleaseRemainder_;
    uint8_t ticksUntilNextSpawn_;
    uint8_t ticksUntilFade_;
    uint8_t spawnRateRemainderQ0_8_;
    uint8_t nextIgnitionSlot_;
    uint8_t currentBank_;
  };

} // namespace Aurora
