#pragma once

#include <Arduino.h>
#include <FastLED.h>

namespace Config {
constexpr uint8_t LedDataPin = 6;
constexpr uint8_t LedCount = 56;
constexpr uint8_t LedVolts = 5;
constexpr uint16_t LedMaxMilliamps = 2000;
constexpr EOrder LedColorOrder = GRB;

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

constexpr uint8_t HddEdgeBoost = 20;
constexpr uint8_t HddActiveRise = 3;
constexpr uint8_t HddInactiveDecay = 2;
constexpr uint8_t HddMax = 128;

constexpr uint8_t AuroraBaseBrightness = 48;
constexpr uint8_t AuroraHddBrightnessBoost = 44;
constexpr uint8_t AuroraBaseSpeed = 18;
constexpr bool AuroraHddAffectsSpeed = true;
constexpr bool AuroraHddAffectsBrightness = true;
constexpr uint8_t SleepPointBrightness = 32;
constexpr uint8_t TransitionBrightness = 96;
constexpr uint8_t ShutdownOriginMin = 23;
constexpr uint8_t ShutdownOriginMax = 32;
}