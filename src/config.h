#pragma once

#include <stdint.h>

#include "core/rgb8.h"

namespace Config {
namespace VisualTuning {
constexpr bool AuroraHddAffectsSpawnRate = true;
constexpr bool AuroraHddAffectsBackground = true;
constexpr uint8_t AuroraHddBackgroundMaxBrightness = 240;
constexpr uint16_t AuroraHddBackgroundReleaseMs = 2000;
constexpr uint16_t AuroraFixedStepMs = 20;
constexpr uint8_t AuroraHddBackgroundWaveLedScale = 3;
constexpr uint8_t AuroraHddBackgroundWaveTimeShift = 1;
constexpr uint8_t AuroraSpawnMinTicks = 25;
constexpr uint8_t AuroraSpawnMaxTicks = 50;
constexpr uint8_t AuroraTicksPerFade = 5;
constexpr uint8_t AuroraFadeStep = 2;
constexpr uint8_t AuroraColorProgressStep = 2;
constexpr uint8_t AuroraDiffusionSideWeight = 1;
constexpr uint8_t AuroraDiffusionCenterWeight = 63;
constexpr uint8_t AuroraDiffusionKernelSum =
    AuroraDiffusionSideWeight * 2 + AuroraDiffusionCenterWeight;
constexpr uint32_t AuroraBackgroundRgb = 0x000000UL;
constexpr uint32_t AuroraColor1Rgb = 0x1ABA94UL;
constexpr uint32_t AuroraColor2Rgb = 0x6E347CUL;
constexpr uint16_t SleepTravelIntervalMs = 1700;
constexpr uint8_t SleepSecondaryPointOffset = 29;
constexpr uint8_t SleepBeatsPerMinute = 4;
constexpr uint8_t SleepMinimumBrightness = 4;
constexpr uint8_t SleepPointBrightness = 32;
constexpr uint8_t SleepPrimaryHue = 160;
constexpr uint8_t SleepPrimarySaturation = 180;
constexpr uint8_t SleepSecondaryHue = 176;
constexpr uint8_t SleepSecondarySaturation = 160;
constexpr uint8_t SleepSecondaryBrightnessDivisor = 2;
constexpr uint8_t StartupHueBase = 120;
constexpr uint8_t StartupSaturation = 220;
constexpr uint8_t StartupBrightness = 96;
}  // namespace VisualTuning

constexpr uint8_t LedCount = 56;
constexpr uint32_t FrameIntervalMs = 20;
constexpr uint32_t HddUpdateMs = 10;
constexpr uint8_t HddEdgeBoost = 20;
constexpr uint32_t ShortPowerLedOffIgnoreMs = 3500;
constexpr uint32_t PowerLedBlinkMinHalfPeriodMs = 150;
constexpr uint32_t PowerLedBlinkMaxHalfPeriodMs = 3000;
constexpr uint32_t PowerLedBlinkStaleMs = 3500;
constexpr uint8_t PowerLedBlinkEdgesRequired = 4;
constexpr uint32_t InitialStateObserveMs = 1500;
constexpr uint32_t PowerHoldForcedMs = 4000;
constexpr uint32_t ForcedFlashAtMs = 2000;
constexpr uint32_t StartupDurationMs = 2200;
constexpr uint32_t ShutdownDurationMs = 1800;
constexpr uint32_t ResetDurationMs = 900;
constexpr uint32_t StartingTimeoutMs = 30000;
constexpr uint32_t ShutdownWarningTimeoutMs = 120000;
constexpr uint8_t HddActiveRise = 3;
constexpr uint8_t HddInactiveDecay = 2;
constexpr uint8_t HddMax = 128;
constexpr uint8_t AuroraSpawnMinCount = 1;
constexpr uint8_t AuroraSpawnMaxCount = 3;
constexpr uint8_t AuroraIgnitionCapacity = 10;
constexpr uint8_t AuroraIgnitionMinPeakBrightness = 168;
constexpr uint8_t AuroraIgnitionMaxPeakBrightness = 220;
constexpr uint16_t AuroraIgnitionMinDurationMs = 600;
constexpr uint16_t AuroraIgnitionMaxDurationMs = 1400;
constexpr uint8_t AuroraIgnitionMinRadius = 1;
constexpr uint8_t AuroraIgnitionMaxRadius = 3;
constexpr uint32_t AuroraZeroSeedFallback = 0xA341316CUL;
constexpr uint8_t ShutdownOriginMin = 23;
constexpr uint8_t ShutdownOriginMax = 32;
constexpr Aurora::Rgb8 ShutdownColor = {255, 255, 255};
constexpr Aurora::Rgb8 ResetColor = {255, 0, 0};
constexpr uint8_t ForcedHue = 0;
constexpr uint8_t ForcedSaturation = UINT8_MAX;
constexpr uint8_t ForcedInitialBrightness = 10;
constexpr uint8_t ForcedFlashBrightness = 160;

constexpr bool AuroraHddAffectsSpawnRate = VisualTuning::AuroraHddAffectsSpawnRate;
constexpr bool AuroraHddAffectsBackground = VisualTuning::AuroraHddAffectsBackground;
constexpr uint8_t AuroraHddBackgroundMaxBrightness = VisualTuning::AuroraHddBackgroundMaxBrightness;
constexpr uint16_t AuroraHddBackgroundReleaseMs = VisualTuning::AuroraHddBackgroundReleaseMs;
constexpr uint16_t AuroraFixedStepMs = VisualTuning::AuroraFixedStepMs;
constexpr uint8_t AuroraHddBackgroundWaveLedScale = VisualTuning::AuroraHddBackgroundWaveLedScale;
constexpr uint8_t AuroraHddBackgroundWaveTimeShift = VisualTuning::AuroraHddBackgroundWaveTimeShift;
constexpr uint8_t AuroraSpawnMinTicks = VisualTuning::AuroraSpawnMinTicks;
constexpr uint8_t AuroraSpawnMaxTicks = VisualTuning::AuroraSpawnMaxTicks;
constexpr uint8_t AuroraTicksPerFade = VisualTuning::AuroraTicksPerFade;
constexpr uint8_t AuroraFadeStep = VisualTuning::AuroraFadeStep;
constexpr uint8_t AuroraColorProgressStep = VisualTuning::AuroraColorProgressStep;
constexpr uint8_t AuroraDiffusionSideWeight = VisualTuning::AuroraDiffusionSideWeight;
constexpr uint8_t AuroraDiffusionCenterWeight = VisualTuning::AuroraDiffusionCenterWeight;
constexpr uint8_t AuroraDiffusionKernelSum = VisualTuning::AuroraDiffusionKernelSum;
constexpr uint32_t AuroraBackgroundRgb = VisualTuning::AuroraBackgroundRgb;
constexpr uint32_t AuroraColor1Rgb = VisualTuning::AuroraColor1Rgb;
constexpr uint32_t AuroraColor2Rgb = VisualTuning::AuroraColor2Rgb;
constexpr uint16_t SleepTravelIntervalMs = VisualTuning::SleepTravelIntervalMs;
constexpr uint8_t SleepSecondaryPointOffset = VisualTuning::SleepSecondaryPointOffset;
constexpr uint8_t SleepBeatsPerMinute = VisualTuning::SleepBeatsPerMinute;
constexpr uint8_t SleepMinimumBrightness = VisualTuning::SleepMinimumBrightness;
constexpr uint8_t SleepPointBrightness = VisualTuning::SleepPointBrightness;
constexpr uint8_t SleepPrimaryHue = VisualTuning::SleepPrimaryHue;
constexpr uint8_t SleepPrimarySaturation = VisualTuning::SleepPrimarySaturation;
constexpr uint8_t SleepSecondaryHue = VisualTuning::SleepSecondaryHue;
constexpr uint8_t SleepSecondarySaturation = VisualTuning::SleepSecondarySaturation;
constexpr uint8_t SleepSecondaryBrightnessDivisor = VisualTuning::SleepSecondaryBrightnessDivisor;
constexpr uint8_t StartupHueBase = VisualTuning::StartupHueBase;
constexpr uint8_t StartupSaturation = VisualTuning::StartupSaturation;
constexpr uint8_t StartupBrightness = VisualTuning::StartupBrightness;

static_assert(LedCount >= 3, "Aurora requires at least three LEDs");
static_assert(FrameIntervalMs > 0, "Frame interval must be positive");
static_assert(HddUpdateMs > 0, "HDD update interval must be positive");
static_assert(AuroraFixedStepMs > 0, "Aurora fixed step must be positive");
static_assert(HddMax > 0, "HDD activity maximum must be positive");
static_assert(AuroraHddBackgroundReleaseMs > 0,
              "HDD background release must be positive");
static_assert(AuroraSpawnMinTicks > 0 &&
                  AuroraSpawnMinTicks <= AuroraSpawnMaxTicks,
              "Invalid Aurora spawn tick range");
static_assert(AuroraSpawnMinCount > 0 &&
                  AuroraSpawnMinCount <= AuroraSpawnMaxCount,
              "Invalid Aurora spawn count range");
static_assert(AuroraSpawnMaxCount <= LedCount,
              "Aurora cannot spawn more points than LEDs");
static_assert(AuroraIgnitionCapacity > 0,
              "Aurora ignition capacity must be positive");
static_assert(AuroraSpawnMaxCount <= AuroraIgnitionCapacity,
              "One spawn batch must fit in the ignition pool");
static_assert(AuroraIgnitionMinPeakBrightness > 0 &&
                  AuroraIgnitionMinPeakBrightness <=
                      AuroraIgnitionMaxPeakBrightness,
              "Invalid Aurora ignition brightness range");
static_assert(AuroraIgnitionMinDurationMs > 0 &&
                  AuroraIgnitionMinDurationMs <=
                      AuroraIgnitionMaxDurationMs,
              "Invalid Aurora ignition duration range");
static_assert(AuroraIgnitionMinDurationMs % AuroraFixedStepMs == 0 &&
                  AuroraIgnitionMaxDurationMs % AuroraFixedStepMs == 0,
              "Aurora ignition durations must align to the fixed step");
static_assert(AuroraIgnitionMaxDurationMs / AuroraFixedStepMs <= UINT8_MAX,
              "Aurora ignition duration must fit in one byte of ticks");

static_assert(AuroraTicksPerFade > 0,
              "Aurora fade period must be positive");
static_assert(SleepTravelIntervalMs > 0,
              "Sleep travel interval must be positive");
static_assert(SleepSecondaryBrightnessDivisor > 0,
              "Sleep brightness divisor must be positive");
static_assert(ShutdownOriginMin <= ShutdownOriginMax &&
                  ShutdownOriginMax < LedCount,
              "Shutdown origin must be inside the LED frame");
static_assert(ForcedFlashAtMs <= PowerHoldForcedMs,
              "Forced flash must occur during the hold interval");
static_assert(AuroraDiffusionKernelSum > 0,
              "Aurora diffusion kernel sum must be positive");
static_assert(AuroraDiffusionKernelSum ==
                  AuroraDiffusionSideWeight * 2 +
                      AuroraDiffusionCenterWeight,
              "Aurora diffusion kernel sum must match its weights");
}


