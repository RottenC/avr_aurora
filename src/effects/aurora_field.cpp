#include "aurora_field.h"

#include "../config.h"
#include "../core/portable_math.h"

namespace {

constexpr uint16_t kQ8_8Max = static_cast<uint16_t>(UINT8_MAX) << 8;
constexpr uint16_t kOneQ8_8 = 1U << 8;
constexpr uint32_t kSmoothstepDenominator =
    static_cast<uint32_t>(UINT8_MAX) * UINT8_MAX;

uint16_t maxQ8_8(uint16_t left, uint16_t right) {
  return left > right ? left : right;
}

uint8_t calculateIgnitionEase(uint8_t elapsedTicks,
                              uint8_t durationTicks) {
  if (durationTicks == 0 || elapsedTicks >= durationTicks) return UINT8_MAX;

  const uint16_t normalized = static_cast<uint16_t>(
      (static_cast<uint16_t>(elapsedTicks) * UINT8_MAX +
       durationTicks / 2U) /
      durationTicks);
  const uint32_t normalizedSquared =
      static_cast<uint32_t>(normalized) * normalized;
  const uint16_t cubicFactor = static_cast<uint16_t>(
      3U * UINT8_MAX - 2U * normalized);
  const uint32_t numerator = normalizedSquared * cubicFactor;
  return static_cast<uint8_t>(
      (numerator + kSmoothstepDenominator / 2U) /
      kSmoothstepDenominator);
}

uint16_t calculateIgnitionTargetBrightness(uint8_t peakBrightness,
                                     uint8_t easedProgress) {
  const uint32_t peakQ8_8 =
      static_cast<uint32_t>(peakBrightness) << 8;
  return static_cast<uint16_t>(
      (peakQ8_8 * easedProgress + UINT8_MAX / 2U) / UINT8_MAX);
}

uint8_t calculateIgnitionSpatialWeight(uint8_t radius, uint8_t distance) {
  if (distance > radius) return 0;

  const uint16_t scale = static_cast<uint16_t>(radius) + 1U;
  const uint16_t distanceSquared =
      static_cast<uint16_t>(distance) * distance;
  const uint16_t scaleCubed = scale * scale * scale;
  const uint16_t numerator = static_cast<uint16_t>(
      scaleCubed + 2U * distanceSquared * distance -
      3U * distanceSquared * scale);
  return static_cast<uint8_t>(
      (static_cast<uint32_t>(numerator) * UINT8_MAX + scaleCubed / 2U) /
      scaleCubed);
}

uint8_t scaleU8(uint8_t value, uint8_t scale) {
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(value) * scale + UINT8_MAX / 2U) /
      UINT8_MAX);
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
      nextIgnitionSlot_(0),
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
  for (uint8_t slot = 0; slot < Config::AuroraIgnitionCapacity; ++slot) {
    ignitions_[slot] = {0, 0, 0, 0, 0};
  }

  currentBank_ = 0;
  nextIgnitionSlot_ = 0;
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

void Field::exciteCell(uint8_t index, uint8_t minimumBrightness,
                       uint8_t colorProgressCeiling) {
  if (index >= LedCount) return;

  const uint16_t target = static_cast<uint16_t>(minimumBrightness) << 8;
  if (brightness_[currentBank_][index] < target) {
    brightness_[currentBank_][index] = target;
  }
  if (colorProgress_[currentBank_][index] > colorProgressCeiling) {
    colorProgress_[currentBank_][index] = colorProgressCeiling;
  }
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

uint8_t Field::outputBrightness(uint8_t index) const {
  if (index >= LedCount) return 0;
  return static_cast<uint8_t>(
      maxQ8_8(brightness_[currentBank_][index],
              backgroundBrightness_[index]) >>
      8);
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

void Field::spawnIgnitionForTest(uint8_t position, uint8_t peakBrightness,
                                 uint8_t durationTicks, uint8_t radius) {
  spawnIgnition(position, peakBrightness, durationTicks, radius);
}

void Field::advanceIgnitionsForTest() { advanceIgnitions(); }

void Field::setSpawnCountdownForTest(uint8_t ticksUntilNextSpawn) {
  ticksUntilNextSpawn_ = ticksUntilNextSpawn;
}

uint8_t Field::activeIgnitionCountForTest() const {
  uint8_t count = 0;
  for (uint8_t slot = 0; slot < Config::AuroraIgnitionCapacity; ++slot) {
    if (ignitions_[slot].durationTicks != 0) ++count;
  }
  return count;
}

Field::IgnitionTestState Field::ignitionForTest(uint8_t slot) const {
  if (slot >= Config::AuroraIgnitionCapacity) return {};
  const Ignition &ignition = ignitions_[slot];
  return {
      ignition.durationTicks != 0,
      ignition.position,
      ignition.peakBrightness,
      ignition.durationTicks,
      ignition.elapsedTicks,
      ignition.radius,
  };
}

uint8_t Field::ignitionEaseForTest(uint8_t elapsedTicks,
                                   uint8_t durationTicks) {
  return calculateIgnitionEase(elapsedTicks, durationTicks);
}

uint8_t Field::ignitionSpatialWeightForTest(uint8_t radius,
                                            uint8_t distance) {
  return calculateIgnitionSpatialWeight(radius, distance);
}

uint16_t Field::ignitionTargetBrightnessForTest(
    uint8_t peakBrightness, uint8_t easedProgress) {
  return calculateIgnitionTargetBrightness(peakBrightness, easedProgress);
}
#endif

void Field::fixedTick(uint8_t hddActivity) {
  diffuseTick();

  --ticksUntilFade_;
  const bool applyFade = ticksUntilFade_ == 0;
  if (applyFade) ticksUntilFade_ = Config::AuroraTicksPerFade;
  applyFadeAndColorTick(applyFade);

  currentBank_ ^= 1;
  advanceIgnitions();
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

void Field::advanceIgnitions() {
  uint8_t easedProgress[Config::AuroraIgnitionCapacity];

  for (uint8_t slot = 0; slot < Config::AuroraIgnitionCapacity; ++slot) {
    Ignition &ignition = ignitions_[slot];
    if (ignition.durationTicks == 0) continue;

    if (ignition.elapsedTicks < ignition.durationTicks) {
      ++ignition.elapsedTicks;
    }
    easedProgress[slot] =
        calculateIgnitionEase(ignition.elapsedTicks, ignition.durationTicks);
    applyIgnitionColorCap(ignition, easedProgress[slot]);
  }

  for (uint8_t slot = 0; slot < Config::AuroraIgnitionCapacity; ++slot) {
    const Ignition &ignition = ignitions_[slot];
    if (ignition.durationTicks == 0) continue;

    bool positionHandled = false;
    for (uint8_t previous = 0; previous < slot; ++previous) {
      if (ignitions_[previous].durationTicks != 0 &&
          ignitions_[previous].position == ignition.position) {
        positionHandled = true;
        break;
      }
    }
    if (positionHandled) continue;

    uint32_t combinedTarget = 0;
    for (uint8_t source = slot; source < Config::AuroraIgnitionCapacity;
         ++source) {
      const Ignition &candidate = ignitions_[source];
      if (candidate.durationTicks == 0 ||
          candidate.position != ignition.position) {
        continue;
      }
      combinedTarget += calculateIgnitionTargetBrightness(
          candidate.peakBrightness, easedProgress[source]);
      if (combinedTarget >= kQ8_8Max) {
        combinedTarget = kQ8_8Max;
        break;
      }
    }

    uint16_t &cellBrightness =
        brightness_[currentBank_][ignition.position];
    if (cellBrightness < combinedTarget) {
      cellBrightness = static_cast<uint16_t>(combinedTarget);
    }
  }

  for (uint8_t slot = 0; slot < Config::AuroraIgnitionCapacity; ++slot) {
    Ignition &ignition = ignitions_[slot];
    if (ignition.durationTicks != 0 &&
        ignition.elapsedTicks >= ignition.durationTicks) {
      ignition.durationTicks = 0;
    }
  }
}

void Field::applyIgnitionColorCap(const Ignition &ignition,
                                  uint8_t easedProgress) {
  for (uint8_t distance = 0; distance <= ignition.radius; ++distance) {
    const uint8_t spatialWeight =
        calculateIgnitionSpatialWeight(ignition.radius, distance);
    const uint8_t suppression = scaleU8(spatialWeight, easedProgress);
    const uint8_t maximumProgress =
        static_cast<uint8_t>(UINT8_MAX - suppression);

    if (ignition.position >= distance) {
      const uint8_t left =
          static_cast<uint8_t>(ignition.position - distance);
      if (colorProgress_[currentBank_][left] > maximumProgress) {
        colorProgress_[currentBank_][left] = maximumProgress;
      }
    }

    if (distance == 0) continue;
    const uint16_t right =
        static_cast<uint16_t>(ignition.position) + distance;
    if (right < LedCount &&
        colorProgress_[currentBank_][right] > maximumProgress) {
      colorProgress_[currentBank_][right] = maximumProgress;
    }
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
    const uint8_t peakBrightness = rangeInclusive(
        Config::AuroraIgnitionMinPeakBrightness,
        Config::AuroraIgnitionMaxPeakBrightness);
    constexpr uint8_t minimumDurationTicks = static_cast<uint8_t>(
        Config::AuroraIgnitionMinDurationMs / Config::AuroraFixedStepMs);
    constexpr uint8_t maximumDurationTicks = static_cast<uint8_t>(
        Config::AuroraIgnitionMaxDurationMs / Config::AuroraFixedStepMs);
    const uint8_t durationTicks =
        rangeInclusive(minimumDurationTicks, maximumDurationTicks);
    const uint8_t radius = rangeInclusive(Config::AuroraIgnitionMinRadius,
                                          Config::AuroraIgnitionMaxRadius);
    spawnIgnition(position, peakBrightness, durationTicks, radius);
  }
}

void Field::spawnIgnition(uint8_t position, uint8_t peakBrightness,
                          uint8_t durationTicks, uint8_t radius) {
  if (position >= LedCount) return;
  if (durationTicks == 0) durationTicks = 1;
  if (radius > Config::AuroraIgnitionMaxRadius) {
    radius = Config::AuroraIgnitionMaxRadius;
  }

  const uint8_t slot = selectIgnitionSlot();
  ignitions_[slot] = {
      position,
      peakBrightness,
      durationTicks,
      0,
      radius,
  };
  nextIgnitionSlot_ = static_cast<uint8_t>(
      (slot + 1U) % Config::AuroraIgnitionCapacity);
}

uint8_t Field::selectIgnitionSlot() const {
  for (uint8_t offset = 0; offset < Config::AuroraIgnitionCapacity;
       ++offset) {
    const uint8_t slot = static_cast<uint8_t>(
        (nextIgnitionSlot_ + offset) % Config::AuroraIgnitionCapacity);
    if (ignitions_[slot].durationTicks == 0) return slot;
  }

  uint8_t oldestSlot = nextIgnitionSlot_;
  for (uint8_t offset = 1; offset < Config::AuroraIgnitionCapacity;
       ++offset) {
    const uint8_t slot = static_cast<uint8_t>(
        (nextIgnitionSlot_ + offset) % Config::AuroraIgnitionCapacity);
    if (ignitions_[slot].elapsedTicks >
        ignitions_[oldestSlot].elapsedTicks) {
      oldestSlot = slot;
    }
  }
  return oldestSlot;
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
