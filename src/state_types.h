#pragma once

#include <stdint.h>

enum class SignalState : uint8_t { Low, Rising, High, Falling, Blinking };

constexpr bool signalIsHigh(SignalState state) {
  return state == SignalState::Rising || state == SignalState::High;
}

constexpr bool signalIsRising(SignalState state) {
  return state == SignalState::Rising;
}

constexpr bool signalIsFalling(SignalState state) {
  return state == SignalState::Falling;
}

enum class PcState : uint8_t {
  Off,
  Starting,
  Running,
  Sleeping,
  AwaitShutdown,
  Warn
};
enum class TransitionEffect : uint8_t {
  None,
  Startup,
  Shutdown,
  ForcedShutdown,
  Reset
};
enum class PowerLedMode : uint8_t { Off, On, Blinking };
