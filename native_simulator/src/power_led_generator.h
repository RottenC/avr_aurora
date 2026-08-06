#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

enum class PowerLedSourceMode : uint8_t { Manual, Off, On, Blinking };

struct PowerLedTransition {
  uint32_t offsetMs = 0;
  bool active = false;
};

struct PowerLedUpdate {
  bool level = false;
  std::array<PowerLedTransition, 16> transitions{};
  std::size_t transitionCount = 0;
};

class PowerLedGenerator {
 public:
  void reset();
  PowerLedUpdate update(uint32_t deltaMs, bool manualLevel);
  void setMode(PowerLedSourceMode mode);
  void setHalfPeriodMs(uint32_t value);

  PowerLedSourceMode mode() const { return mode_; }
  uint32_t halfPeriodMs() const { return halfPeriodMs_; }
  bool level() const { return level_; }

 private:
  void append(PowerLedUpdate &result, uint32_t offset, bool active);

  PowerLedSourceMode mode_ = PowerLedSourceMode::Manual;
  uint32_t halfPeriodMs_ = 500;
  uint32_t phaseMs_ = 0;
  bool level_ = false;
  bool reconcilePending_ = false;
};
