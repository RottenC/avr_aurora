#include "aurora_field.h"

namespace {

constexpr uint16_t kQ8_8Max = static_cast<uint16_t>(UINT8_MAX) << 8;
constexpr uint16_t kFadeStepQ8_8 =
    static_cast<uint16_t>(Config::AuroraFadeStep) << 8;

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
      ticksUntilNextSpawn_(1),
      ticksUntilFade_(Config::AuroraTicksPerFade),
      currentBank_(0) {
  reset(1);
}

void Field::reset(uint32_t seed) {
  for (uint8_t bank = 0; bank < 2; ++bank) {
    for (uint8_t index = 0; index < Config::LedCount; ++index) {
      brightness_[bank][index] = 0;
      colorProgress_[bank][index] = 0;
    }
  }

  currentBank_ = 0;
  prngState_ = seed == 0 ? Config::AuroraZeroSeedFallback : seed;
  fixedStepAccumulatorMs_ = 0;
  ticksUntilFade_ = Config::AuroraTicksPerFade;
  scheduleNextSpawn();
}

void Field::advance(uint32_t elapsedMs) {
  const uint32_t untilNextTick =
      Config::AuroraFixedStepMs - fixedStepAccumulatorMs_;
  if (elapsedMs < untilNextTick) {
    fixedStepAccumulatorMs_ += elapsedMs;
    return;
  }

  elapsedMs -= untilNextTick;
  fixedStepAccumulatorMs_ = 0;
  fixedTick();
  while (elapsedMs >= Config::AuroraFixedStepMs) {
    elapsedMs -= Config::AuroraFixedStepMs;
    fixedTick();
  }
  fixedStepAccumulatorMs_ = elapsedMs;
}

Rgb8 Field::pixel(uint8_t index) const {
  const Rgb8 background = unpackRgb(Config::AuroraBackgroundRgb);
  if (index >= Config::LedCount) return background;

  const Rgb8 color1 = unpackRgb(Config::AuroraColor1Rgb);
  const Rgb8 color2 = unpackRgb(Config::AuroraColor2Rgb);
  const Rgb8 flare =
      lerpRgb(color1, color2, colorProgress_[currentBank_][index]);
  const uint8_t brightness =
      static_cast<uint8_t>(brightness_[currentBank_][index] >> 8);
  return lerpRgb(background, flare, brightness);
}

uint16_t Field::brightnessQ8_8(uint8_t index) const {
  return index < Config::LedCount ? brightness_[currentBank_][index] : 0;
}

uint8_t Field::colorProgress(uint8_t index) const {
  return index < Config::LedCount ? colorProgress_[currentBank_][index] : 0;
}

#ifdef PIO_UNIT_TESTING
void Field::setCellForTest(uint8_t index, uint16_t brightnessQ8_8,
                           uint8_t colorProgress) {
  if (index >= Config::LedCount) return;
  brightness_[currentBank_][index] =
      brightnessQ8_8 > kQ8_8Max ? kQ8_8Max : brightnessQ8_8;
  colorProgress_[currentBank_][index] = colorProgress;
}
#endif

void Field::fixedTick() {
  diffuseTick();

  --ticksUntilFade_;
  const bool applyFade = ticksUntilFade_ == 0;
  if (applyFade) ticksUntilFade_ = Config::AuroraTicksPerFade;
  applyFadeAndColorTick(applyFade);

  currentBank_ ^= 1;

  --ticksUntilNextSpawn_;
  if (ticksUntilNextSpawn_ == 0) {
    spawnStars();
    scheduleNextSpawn();
  }
}

void Field::diffuseTick() {
  const uint8_t nextBank = currentBank_ ^ 1;

  for (uint8_t destination = 0; destination < Config::LedCount;
       ++destination) {
    uint32_t weightedBrightness = 0;
    uint32_t weightedProgress = 0;

    if (destination > 0) {
      const uint8_t source = destination - 1;
      const uint32_t contribution =
          static_cast<uint32_t>(brightness_[currentBank_][source]) *
          Config::AuroraDiffusionSideWeight;
      weightedBrightness += contribution;
      weightedProgress +=
          contribution * colorProgress_[currentBank_][source];
    }

    {
      const uint32_t contribution =
          static_cast<uint32_t>(brightness_[currentBank_][destination]) *
          Config::AuroraDiffusionCenterWeight;
      weightedBrightness += contribution;
      weightedProgress +=
          contribution * colorProgress_[currentBank_][destination];
    }

    if (destination + 1 < Config::LedCount) {
      const uint8_t source = destination + 1;
      const uint32_t contribution =
          static_cast<uint32_t>(brightness_[currentBank_][source]) *
          Config::AuroraDiffusionSideWeight;
      weightedBrightness += contribution;
      weightedProgress +=
          contribution * colorProgress_[currentBank_][source];
    }

    const uint16_t brightness = static_cast<uint16_t>(
        weightedBrightness / Config::AuroraDiffusionKernelSum);
    brightness_[nextBank][destination] =
        brightness > kQ8_8Max ? kQ8_8Max : brightness;
    colorProgress_[nextBank][destination] =
        brightness > 0
            ? static_cast<uint8_t>(weightedProgress / weightedBrightness)
            : 0;
  }

  bool previousIsPeak = false;
  for (uint8_t destination = 1; destination + 1 < Config::LedCount;
       ++destination) {
    const bool currentIsPeak =
        colorProgress_[nextBank][destination] >
            colorProgress_[nextBank][destination - 1] &&
        colorProgress_[nextBank][destination] >
            colorProgress_[nextBank][destination + 1];

    if (previousIsPeak) {
      const uint8_t previous = destination - 1;
      if (colorProgress_[nextBank][previous] < UINT8_MAX) {
        ++colorProgress_[nextBank][previous];
      }
    }
    previousIsPeak = currentIsPeak;
  }

  if (previousIsPeak) {
    const uint8_t previous = Config::LedCount - 2;
    if (colorProgress_[nextBank][previous] < UINT8_MAX) {
      ++colorProgress_[nextBank][previous];
    }
  }
}

void Field::applyFadeAndColorTick(bool applyFade) {
  const uint8_t nextBank = currentBank_ ^ 1;

  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    uint16_t brightness = brightness_[nextBank][index];
    if (brightness == 0) {
      colorProgress_[nextBank][index] = 0;
      continue;
    }

    if (applyFade) {
      brightness =
          brightness > kFadeStepQ8_8 ? brightness - kFadeStepQ8_8 : 0;
    }
    brightness_[nextBank][index] = brightness;

    if (brightness == 0) {
      colorProgress_[nextBank][index] = 0;
      continue;
    }

    const uint16_t progress =
        static_cast<uint16_t>(colorProgress_[nextBank][index]) +
        Config::AuroraColorProgressStep;
    colorProgress_[nextBank][index] =
        progress > UINT8_MAX ? UINT8_MAX : static_cast<uint8_t>(progress);
  }
}

void Field::spawnStars() {
  const uint8_t count =
      rangeInclusive(Config::AuroraSpawnMinCount,
                     Config::AuroraSpawnMaxCount);
  for (uint8_t spawn = 0; spawn < count; ++spawn) {
    const uint8_t position = rangeInclusive(0, Config::LedCount - 1);
    brightness_[currentBank_][position] = kQ8_8Max;
    colorProgress_[currentBank_][position] = 0;
  }
}

void Field::scheduleNextSpawn() {
  ticksUntilNextSpawn_ =
      rangeInclusive(Config::AuroraSpawnMinTicks,
                     Config::AuroraSpawnMaxTicks);
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
