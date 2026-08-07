#include "aurora_field.h"

#include "../config.h"
#include "../core/portable_math.h"

namespace {

constexpr uint16_t kQ8_8Max = static_cast<uint16_t>(UINT8_MAX) << 8;
constexpr uint16_t kOneQ8_8 = 1U << 8;

uint16_t maxQ8_8(uint16_t left, uint16_t right) {
  return left > right ? left : right;
}

Aurora::Rgb8 unpackRgb(uint32_t packed) {
  return {static_cast<uint8_t>(packed >> 16),
          static_cast<uint8_t>(packed >> 8), static_cast<uint8_t>(packed)};
}

uint8_t lerpU8(uint8_t a, uint8_t b, uint8_t amount) {
  const uint32_t numerator =
      static_cast<uint32_t>(a) * (UINT8_MAX - amount) +
      static_cast<uint32_t>(b) * amount;
  return static_cast<uint8_t>(numerator / UINT8_MAX);
}

Aurora::Rgb8 lerpRgb(const Aurora::Rgb8 &a, const Aurora::Rgb8 &b,
                     uint8_t amount) {
  return {lerpU8(a.r, b.r, amount), lerpU8(a.g, b.g, amount),
          lerpU8(a.b, b.b, amount)};
}

}  // namespace

namespace Aurora {

uint32_t xorshift32(uint32_t value) {
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  return value;
}

Field::Field()
    : prngState_(1),
      fixedStepAccumulatorMs_(0),
      backgroundActivityQ8_8_(0),
      backgroundReleaseRemainder_(0),
      ticksUntilNextSpawn_(1),
      ticksUntilFade_(Config::AuroraTicksPerFade),
      spawnRateRemainderQ0_8_(0),
      currentBank_(0) {
  reset(1);
}

void Field::reset(uint32_t seed) {
  for (uint8_t bank = 0; bank < 2; ++bank) {
    for (uint8_t index = 0; index < LedCount; ++index) {
      brightness_[bank][index] = 0;
      colorProgress_[bank][index] = 0;
    }
  }
  for (uint8_t index = 0; index < LedCount; ++index) {
    backgroundBrightness_[index] = 0;
  }

  currentBank_ = 0;
  prngState_ = seed == 0 ? Config::AuroraZeroSeedFallback : seed;
  fixedStepAccumulatorMs_ = 0;
  backgroundActivityQ8_8_ = 0;
  backgroundReleaseRemainder_ = 0;
  ticksUntilFade_ = Config::AuroraTicksPerFade;
  spawnRateRemainderQ0_8_ = 0;
  scheduleNextSpawn();
}

void Field::advance(uint32_t elapsedMs, uint8_t hddActivity,
                    uint32_t nowMs) {
  updateBackground(elapsedMs, hddActivity, nowMs);

  const uint32_t untilNextTick =
      Config::AuroraFixedStepMs - fixedStepAccumulatorMs_;
  if (elapsedMs < untilNextTick) {
    fixedStepAccumulatorMs_ += elapsedMs;
    return;
  }

  elapsedMs -= untilNextTick;
  fixedStepAccumulatorMs_ = 0;
  fixedTick(hddActivity);
  while (elapsedMs >= Config::AuroraFixedStepMs) {
    elapsedMs -= Config::AuroraFixedStepMs;
    fixedTick(hddActivity);
  }
  fixedStepAccumulatorMs_ = elapsedMs;
}

Rgb8 Field::pixel(uint8_t index) const {
  const Rgb8 background = unpackRgb(Config::AuroraBackgroundRgb);
  if (index >= LedCount) return background;

  const Rgb8 color1 = unpackRgb(Config::AuroraColor1Rgb);
  const Rgb8 color2 = unpackRgb(Config::AuroraColor2Rgb);
  const Rgb8 flare =
      lerpRgb(color1, color2, colorProgress_[currentBank_][index]);
  const uint16_t outputBrightness =
      maxQ8_8(brightness_[currentBank_][index],
              backgroundBrightness_[index]);
  const uint8_t brightness = static_cast<uint8_t>(outputBrightness >> 8);
  return lerpRgb(background, flare, brightness);
}

FieldCellDiagnostics Field::diagnostics(uint8_t index) const {
  if (index >= LedCount) return {};
  const uint8_t progress = colorProgress_[currentBank_][index];
  return {
      brightness_[currentBank_][index],
      backgroundBrightness_[index],
      progress,
      lerpRgb(unpackRgb(Config::AuroraColor1Rgb),
              unpackRgb(Config::AuroraColor2Rgb), progress),
  };
}

uint16_t Field::brightnessQ8_8(uint8_t index) const {
  return index < LedCount ? brightness_[currentBank_][index] : 0;
}

uint16_t Field::backgroundBrightnessQ8_8(uint8_t index) const {
  return index < LedCount ? backgroundBrightness_[index] : 0;
}

uint8_t Field::colorProgress(uint8_t index) const {
  return index < LedCount ? colorProgress_[currentBank_][index] : 0;
}

#ifdef PIO_UNIT_TESTING
void Field::setCellForTest(uint8_t index, uint16_t brightnessQ8_8,
                           uint8_t colorProgress) {
  if (index >= LedCount) return;
  brightness_[currentBank_][index] =
      brightnessQ8_8 > kQ8_8Max ? kQ8_8Max : brightnessQ8_8;
  colorProgress_[currentBank_][index] = colorProgress;
}
#endif

void Field::fixedTick(uint8_t hddActivity) {
  diffuseTick();

  --ticksUntilFade_;
  const bool applyFade = ticksUntilFade_ == 0;
  if (applyFade) ticksUntilFade_ = Config::AuroraTicksPerFade;
  applyFadeAndColorTick(applyFade);

  currentBank_ ^= 1;
  advanceSpawnCountdown(hddActivity);
}

void Field::diffuseTick() {
  const uint8_t nextBank = currentBank_ ^ 1;

  for (uint8_t destination = 0; destination < LedCount;
       ++destination) {
    uint32_t weightedBrightness = 0;
    uint16_t weightedProgress = 0;
    uint8_t progressWeight = 0;

    if (destination > 0) {
      const uint8_t source = destination - 1;
      const uint32_t contribution =
          static_cast<uint32_t>(brightness_[currentBank_][source]) *
          Config::AuroraDiffusionSideWeight;
      weightedBrightness += contribution;
      weightedProgress += static_cast<uint16_t>(
          colorProgress_[currentBank_][source] *
          Config::AuroraDiffusionSideWeight);
      progressWeight += Config::AuroraDiffusionSideWeight;
    }

    {
      const uint32_t contribution =
          static_cast<uint32_t>(brightness_[currentBank_][destination]) *
          Config::AuroraDiffusionCenterWeight;
      weightedBrightness += contribution;
      weightedProgress += static_cast<uint16_t>(
          colorProgress_[currentBank_][destination] *
          Config::AuroraDiffusionCenterWeight);
      progressWeight += Config::AuroraDiffusionCenterWeight;
    }

    if (destination + 1 < LedCount) {
      const uint8_t source = destination + 1;
      const uint32_t contribution =
          static_cast<uint32_t>(brightness_[currentBank_][source]) *
          Config::AuroraDiffusionSideWeight;
      weightedBrightness += contribution;
      weightedProgress += static_cast<uint16_t>(
          colorProgress_[currentBank_][source] *
          Config::AuroraDiffusionSideWeight);
      progressWeight += Config::AuroraDiffusionSideWeight;
    }

    const uint16_t brightness = static_cast<uint16_t>(
        weightedBrightness / Config::AuroraDiffusionKernelSum);
    brightness_[nextBank][destination] =
        brightness > kQ8_8Max ? kQ8_8Max : brightness;
    colorProgress_[nextBank][destination] =
        static_cast<uint8_t>(weightedProgress / progressWeight);
  }
}

void Field::applyFadeAndColorTick(bool applyFade) {
  const uint8_t nextBank = currentBank_ ^ 1;

  const uint16_t fadeStepQ8_8 =
      static_cast<uint16_t>(Config::AuroraFadeStep) << 8;
  for (uint8_t index = 0; index < LedCount; ++index) {
    uint16_t brightness = brightness_[nextBank][index];
    if (brightness != 0 && applyFade) {
      brightness = brightness > fadeStepQ8_8 ? brightness - fadeStepQ8_8 : 0;
    }
    brightness_[nextBank][index] = brightness;

    if (brightness == 0 && backgroundBrightness_[index] == 0) continue;

    const uint16_t progress =
        static_cast<uint16_t>(colorProgress_[nextBank][index]) +
        Config::AuroraColorProgressStep;
    colorProgress_[nextBank][index] =
        progress > UINT8_MAX ? UINT8_MAX : static_cast<uint8_t>(progress);
  }
}

void Field::updateBackground(uint32_t elapsedMs, uint8_t hddActivity,
                             uint32_t nowMs) {
  updateBackgroundActivity(elapsedMs, hddActivity);

  uint32_t baseBrightnessQ8_8 = 0;
  if (Config::AuroraHddAffectsBackground) {
    const uint32_t maximumBrightnessQ8_8 =
        static_cast<uint32_t>(Config::AuroraHddBackgroundMaxBrightness) << 8;
    const uint32_t maximumActivityQ8_8 =
        static_cast<uint32_t>(Config::HddMax) << 8;
    baseBrightnessQ8_8 = static_cast<uint32_t>(
        (maximumBrightnessQ8_8 * backgroundActivityQ8_8_) /
        maximumActivityQ8_8);
  }

  const uint16_t time =
      static_cast<uint16_t>(nowMs / Config::AuroraFixedStepMs);
  for (uint8_t index = 0; index < LedCount; ++index) {
    const uint16_t cell =
        static_cast<uint16_t>((static_cast<uint16_t>(index) << 8) + time);
    const uint8_t a = hash8(static_cast<uint8_t>(cell >> 8));
    const uint8_t b =
        hash8(static_cast<uint8_t>((cell >> 8) + 1U));
    const uint8_t c = tri8(static_cast<uint8_t>(
        index * Config::AuroraHddBackgroundWaveLedScale +
        (time >> Config::AuroraHddBackgroundWaveTimeShift)));
    const uint8_t result = static_cast<uint8_t>(
        (static_cast<uint16_t>(a) + b + c) / 3U);
    const uint32_t pulsedBrightnessQ8_8 =
        (baseBrightnessQ8_8 * result) >> 8;
    backgroundBrightness_[index] = static_cast<uint16_t>(
        pulsedBrightnessQ8_8 < kQ8_8Max ? pulsedBrightnessQ8_8
                                        : kQ8_8Max);
  }
}

void Field::updateBackgroundActivity(uint32_t elapsedMs,
                                     uint8_t hddActivity) {
  if (!Config::AuroraHddAffectsBackground) {
    backgroundActivityQ8_8_ = 0;
    backgroundReleaseRemainder_ = 0;
    return;
  }

  const uint8_t boundedActivity =
      hddActivity < Config::HddMax ? hddActivity : Config::HddMax;
  const uint16_t targetQ8_8 =
      static_cast<uint16_t>(boundedActivity) << 8;
  if (targetQ8_8 >= backgroundActivityQ8_8_) {
    backgroundActivityQ8_8_ = targetQ8_8;
    backgroundReleaseRemainder_ = 0;
    return;
  }
  if (elapsedMs == 0) return;

  if (elapsedMs >= Config::AuroraHddBackgroundReleaseMs) {
    backgroundActivityQ8_8_ = targetQ8_8;
    backgroundReleaseRemainder_ = 0;
    return;
  }

  const uint32_t maximumActivityQ8_8 =
      static_cast<uint32_t>(Config::HddMax) << 8;
  const uint32_t numerator =
      elapsedMs * maximumActivityQ8_8 + backgroundReleaseRemainder_;
  const uint16_t releaseStepQ8_8 = static_cast<uint16_t>(
      numerator / Config::AuroraHddBackgroundReleaseMs);
  backgroundReleaseRemainder_ = static_cast<uint16_t>(
      numerator % Config::AuroraHddBackgroundReleaseMs);

  const uint16_t distanceQ8_8 = backgroundActivityQ8_8_ - targetQ8_8;
  if (releaseStepQ8_8 >= distanceQ8_8) {
    backgroundActivityQ8_8_ = targetQ8_8;
    backgroundReleaseRemainder_ = 0;
  } else {
    backgroundActivityQ8_8_ -= releaseStepQ8_8;
  }
}

void Field::advanceSpawnCountdown(uint8_t hddActivity) {
  uint16_t rateQ8_8 = kOneQ8_8;
  if (Config::AuroraHddAffectsSpawnRate) {
    const uint8_t boundedActivity =
        hddActivity < Config::HddMax ? hddActivity : Config::HddMax;
    rateQ8_8 += static_cast<uint16_t>(
        (static_cast<uint32_t>(boundedActivity) * kOneQ8_8) /
        Config::HddMax);
  }

  const uint16_t progressQ8_8 =
      rateQ8_8 + spawnRateRemainderQ0_8_;
  uint8_t spawnTicks = static_cast<uint8_t>(progressQ8_8 >> 8);
  spawnRateRemainderQ0_8_ = static_cast<uint8_t>(progressQ8_8);

  while (spawnTicks > 0) {
    --spawnTicks;
    --ticksUntilNextSpawn_;
    if (ticksUntilNextSpawn_ == 0) {
      spawnStars();
      scheduleNextSpawn();
    }
  }
}

void Field::spawnStars() {
  const uint8_t count =
      rangeInclusive(Config::AuroraSpawnMinCount, Config::AuroraSpawnMaxCount);
  for (uint8_t spawn = 0; spawn < count; ++spawn) {
    const uint8_t position = rangeInclusive(0, LedCount - 1);
    brightness_[currentBank_][position] = kQ8_8Max;
    colorProgress_[currentBank_][position] = 0;
  }
}

void Field::scheduleNextSpawn() {
  ticksUntilNextSpawn_ =
      rangeInclusive(Config::AuroraSpawnMinTicks, Config::AuroraSpawnMaxTicks);
}

uint32_t Field::nextU32() {
  prngState_ = xorshift32(prngState_);
  return prngState_;
}

uint8_t Field::rangeInclusive(uint8_t minimum, uint8_t maximum) {
  const uint8_t span = static_cast<uint8_t>(maximum - minimum + 1);
  return static_cast<uint8_t>(minimum + nextU32() % span);
}

}  // namespace Aurora
