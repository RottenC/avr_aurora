#pragma once

#include <stdint.h>

#include "core/aurora_runtime.h"

namespace Config {
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

constexpr bool AuroraHddAffectsSpeed = false;
constexpr bool AuroraHddAffectsBrightness = false;
constexpr uint16_t AuroraFixedStepMs = 20;
constexpr uint8_t AuroraSpawnMinTicks = 20;
constexpr uint8_t AuroraSpawnMaxTicks = 70;
constexpr uint8_t AuroraSpawnMinCount = 1;
constexpr uint8_t AuroraSpawnMaxCount = 3;
constexpr uint8_t AuroraTicksPerFade = 10;
constexpr uint8_t AuroraFadeStep = 1;
constexpr uint8_t AuroraColorProgressStep = 1;
constexpr uint8_t AuroraDiffusionSideWeight = 1;
constexpr uint8_t AuroraDiffusionCenterWeight = 63;
constexpr uint8_t AuroraDiffusionKernelSum =
    AuroraDiffusionSideWeight * 2 + AuroraDiffusionCenterWeight;
constexpr uint32_t AuroraBackgroundRgb = 0x091E37UL;
constexpr uint32_t AuroraColor1Rgb = 0x1ABA94UL;
constexpr uint32_t AuroraColor2Rgb = 0x6E347CUL;
constexpr uint32_t AuroraZeroSeedFallback = 0xA341316CUL;
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
constexpr uint8_t ShutdownOriginMin = 23;
constexpr uint8_t ShutdownOriginMax = 32;
constexpr Aurora::Rgb8 ShutdownColor = {255, 255, 255};
constexpr Aurora::Rgb8 ResetColor = {255, 0, 0};
constexpr uint8_t ForcedHue = 0;
constexpr uint8_t ForcedSaturation = UINT8_MAX;
constexpr uint8_t ForcedInitialBrightness = 10;
constexpr uint8_t ForcedFlashBrightness = 160;

constexpr PowerLedTrackerConfig powerLedTrackerConfig() {
  return {ShortPowerLedOffIgnoreMs, PowerLedBlinkMinHalfPeriodMs,
          PowerLedBlinkMaxHalfPeriodMs, PowerLedBlinkStaleMs,
          PowerLedBlinkEdgesRequired};
}

constexpr HddActivityConfig hddActivityConfig() {
  return {HddUpdateMs, HddActiveRise, HddInactiveDecay, HddMax};
}

constexpr PcStateConfig pcStateConfig() {
  return {PowerHoldForcedMs, StartingTimeoutMs, ShutdownWarningTimeoutMs};
}

constexpr EffectControllerConfig effectControllerConfig() {
  return {StartupDurationMs, ShutdownDurationMs, ResetDurationMs};
}

constexpr Aurora::FieldConfig auroraFieldConfig() {
  return {AuroraFixedStepMs,
          AuroraSpawnMinTicks,
          AuroraSpawnMaxTicks,
          AuroraSpawnMinCount,
          AuroraSpawnMaxCount,
          AuroraTicksPerFade,
          AuroraFadeStep,
          AuroraColorProgressStep,
          AuroraDiffusionSideWeight,
          AuroraDiffusionCenterWeight,
          AuroraDiffusionKernelSum,
          AuroraBackgroundRgb,
          AuroraColor1Rgb,
          AuroraColor2Rgb,
          AuroraZeroSeedFallback};
}

constexpr AuroraRendererConfig rendererConfig() {
  return {{SleepTravelIntervalMs,
           SleepSecondaryPointOffset,
           SleepBeatsPerMinute,
           SleepMinimumBrightness,
           SleepPointBrightness,
           SleepPrimaryHue,
           SleepPrimarySaturation,
           SleepSecondaryHue,
           SleepSecondarySaturation,
           SleepSecondaryBrightnessDivisor},
          {ForcedFlashAtMs,
           StartupHueBase,
           StartupSaturation,
           StartupBrightness,
           ShutdownOriginMin,
           ShutdownOriginMax,
           ShutdownColor,
           ResetColor,
           ForcedHue,
           ForcedSaturation,
           ForcedInitialBrightness,
           ForcedFlashBrightness},
          AuroraHddAffectsSpeed,
          AuroraHddAffectsBrightness};
}

constexpr AuroraRuntimeConfig runtimeConfig() {
  return {FrameIntervalMs,
          powerLedTrackerConfig(),
          hddActivityConfig(),
          pcStateConfig(),
          effectControllerConfig(),
          auroraFieldConfig(),
          rendererConfig()};
}

static_assert(LedCount == Aurora::LedCount,
              "Firmware and portable frame sizes must match");
static_assert(LedCount >= 3, "Aurora requires at least three LEDs");
static_assert(FrameIntervalMs > 0, "Frame interval must be positive");
static_assert(HddUpdateMs > 0, "HDD update interval must be positive");
static_assert(AuroraFixedStepMs > 0, "Aurora fixed step must be positive");
static_assert(AuroraSpawnMinTicks > 0 &&
                  AuroraSpawnMinTicks <= AuroraSpawnMaxTicks,
              "Invalid Aurora spawn tick range");
static_assert(AuroraSpawnMinCount > 0 &&
                  AuroraSpawnMinCount <= AuroraSpawnMaxCount,
              "Invalid Aurora spawn count range");
static_assert(AuroraSpawnMaxCount <= LedCount,
              "Aurora cannot spawn more points than LEDs");
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
