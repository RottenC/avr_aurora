#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "config_values.h"

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

constexpr bool PowerLedActiveHigh = true;
constexpr bool HddLedActiveHigh = true;
constexpr bool PowerButtonActiveHigh = false;
constexpr bool ResetButtonActiveHigh = false;
constexpr bool StripPowerPresentActiveHigh = true;
constexpr bool DebugButtonActiveHigh = false;

constexpr uint32_t InputPollMs = 5;
constexpr uint16_t DebounceMs = 25;
constexpr uint32_t FrameIntervalMs = 20;
constexpr uint32_t SerialBaud = 115200;
constexpr uint32_t DebugSnapshotIntervalMs = 5000;
constexpr uint32_t ShortPowerLedOffIgnoreMs = FirmwareConfig::ShortPowerLedOffIgnoreMs;
constexpr uint32_t PowerLedBlinkMinHalfPeriodMs = FirmwareConfig::PowerLedBlinkMinHalfPeriodMs;
constexpr uint32_t PowerLedBlinkMaxHalfPeriodMs = FirmwareConfig::PowerLedBlinkMaxHalfPeriodMs;
constexpr uint8_t PowerLedBlinkEdgesRequired = FirmwareConfig::PowerLedBlinkEdgesRequired;
constexpr uint32_t PowerHoldForcedMs = FirmwareConfig::PowerHoldForcedMs;
constexpr uint32_t ForcedFlashAtMs = FirmwareConfig::ForcedFlashAtMs;
constexpr uint32_t StartupDurationMs = FirmwareConfig::StartupDurationMs;
constexpr uint32_t ShutdownDurationMs = FirmwareConfig::ShutdownDurationMs;
constexpr uint32_t ResetDurationMs = FirmwareConfig::ResetDurationMs;
constexpr uint32_t StartingTimeoutMs = FirmwareConfig::StartingTimeoutMs;

constexpr uint8_t HddEdgeBoost = FirmwareConfig::HddEdgeBoost;
constexpr uint8_t HddActiveRise = FirmwareConfig::HddActiveRise;
constexpr uint8_t HddInactiveDecay = FirmwareConfig::HddInactiveDecay;
constexpr uint8_t HddMax = FirmwareConfig::HddMax;

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
