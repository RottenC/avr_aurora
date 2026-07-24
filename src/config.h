#pragma once

#include <stdint.h>

#ifdef ARDUINO
#include <Arduino.h>
#include "fastled_compat.h"
#endif

namespace Config {
constexpr uint8_t LedDataPin = 6;
constexpr uint8_t LedCount = 56;
constexpr uint8_t LedVolts = 5;
constexpr uint16_t LedMaxMilliamps = 2000;
#ifdef ARDUINO
constexpr EOrder LedColorOrder = GRB;
// Keep this analog pin unconnected so startup ADC samples provide seed noise.
constexpr uint8_t AuroraEntropyPin = A0;
#endif

constexpr uint8_t PowerLedPin = 2;
constexpr uint8_t HddLedPin = 3;
constexpr uint8_t PowerButtonPin = 4;
constexpr uint8_t ResetButtonPin = 5;
constexpr uint8_t StripPowerPresentPin = 7;
constexpr uint8_t DebugButtonPin = 8;

// Optocoupler outputs pull the Arduino inputs low when the motherboard LEDs are on.
constexpr bool PowerLedActiveHigh = false;
constexpr bool HddLedActiveHigh = false;
constexpr bool PowerButtonActiveHigh = false;
constexpr bool ResetButtonActiveHigh = false;
constexpr bool StripPowerPresentActiveHigh = true;
constexpr bool DebugButtonActiveHigh = false;

constexpr uint32_t InputPollMs = 5;
constexpr uint16_t DebounceMs = 25;
constexpr uint32_t FrameIntervalMs = 20;
constexpr uint32_t HddUpdateMs = 10;
constexpr uint32_t SerialBaud = 115200;
constexpr uint32_t DebugIntervalMs = 1000;
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

constexpr uint8_t HddEdgeBoost = 20;
constexpr uint8_t HddActiveRise = 3;
constexpr uint8_t HddInactiveDecay = 2;
constexpr uint8_t HddMax = 128;

constexpr uint32_t DebugSnapshotIntervalMs = 1000;
constexpr uint16_t DebugHddEventIntervalMs = 250;
constexpr uint8_t DebugHddEventDelta = 8;

constexpr bool AuroraHddAffectsSpeed = false;
constexpr bool AuroraHddAffectsBrightness = false;
constexpr uint16_t AuroraFixedStepMs = 16;
constexpr uint8_t AuroraSpawnMinTicks = 20;
constexpr uint8_t AuroraSpawnMaxTicks = 50;
constexpr uint8_t AuroraSpawnMinCount = 2;
constexpr uint8_t AuroraSpawnMaxCount = 7;
constexpr uint8_t AuroraTicksPerFade = 2;
constexpr uint8_t AuroraFadeStep = 1;
constexpr uint8_t AuroraColorProgressStep = 2;
constexpr uint8_t AuroraDiffusionSideWeight = 3;
constexpr uint8_t AuroraDiffusionCenterWeight = 59;
constexpr uint8_t AuroraDiffusionKernelSum =
    AuroraDiffusionSideWeight * 2 + AuroraDiffusionCenterWeight;
constexpr uint32_t AuroraBackgroundRgb = 0x050508UL;//;
constexpr uint32_t AuroraColor1Rgb = 0x1ABA94UL;
constexpr uint32_t AuroraColor2Rgb = 0x6E347CUL;
constexpr uint32_t AuroraZeroSeedFallback = 0xA341316CUL;
constexpr uint8_t SleepPointBrightness = 32;
constexpr uint8_t TransitionBrightness = 96;
constexpr uint8_t ShutdownOriginMin = 23;
constexpr uint8_t ShutdownOriginMax = 32;

static_assert(LedCount >= 3, "Aurora requires at least three LEDs");
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
static_assert(AuroraDiffusionKernelSum == 65,
              "Aurora diffusion weights must remain normalized to 65");
}
