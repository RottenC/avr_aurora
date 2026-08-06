#pragma once

#include <Arduino.h>

#include "../fastled_compat.h"

namespace AvrConfig {

constexpr uint8_t LedDataPin = 9;
constexpr uint8_t LedVolts = 5;
constexpr uint16_t LedMaxMilliamps = 2000;
constexpr EOrder LedColorOrder = GRB;

// Keep this analog pin unconnected so startup ADC samples provide seed noise.
constexpr uint8_t AuroraEntropyPin = A0;

constexpr uint8_t PowerLedPin = 3;
constexpr uint8_t HddLedPin = 2;
constexpr uint8_t PowerButtonPin = 4;
constexpr uint8_t ResetButtonPin = 5;
constexpr uint8_t StripPowerPresentPin = 7;

// Optocoupler outputs pull the Arduino inputs low when the motherboard LEDs are
// on. Buttons are observed through high-impedance, active-low sense circuits.
constexpr bool PowerLedActiveHigh = false;
constexpr bool HddLedActiveHigh = false;
constexpr bool PowerButtonActiveHigh = false;
constexpr bool ResetButtonActiveHigh = false;
constexpr bool StripPowerPresentActiveHigh = true;

constexpr uint32_t InputPollMs = 5;
constexpr uint16_t DebounceMs = 25;
constexpr uint32_t SerialBaud = 115200;

constexpr uint32_t DebugSnapshotIntervalMs = 1000;
constexpr uint16_t DebugHddEventIntervalMs = 250;
constexpr uint8_t DebugHddEventDelta = 8;

}  // namespace AvrConfig
