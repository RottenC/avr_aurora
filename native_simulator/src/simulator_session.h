#pragma once

#include <cstdint>

#include <aurora/aurora_runtime.h>

enum class SimulationSpeed : uint8_t {
  X0_1,
  X0_25,
  X0_5,
  X1,
  X2,
  X4,
  X8,
  X16,
};

class SimulatorSession {
 public:
  SimulatorSession();

  void update(uint32_t wallDeltaMs);
  void stepFrame();
  void reset();
  void setPaused(bool paused) { paused_ = paused; }
  void setSpeed(SimulationSpeed speed) { speed_ = speed; }

  uint32_t simulationTimeMs() const { return simulationTimeMs_; }
  bool paused() const { return paused_; }
  SimulationSpeed speed() const { return speed_; }
  const AuroraRuntime &runtime() const { return runtime_; }
  const AuroraSnapshot &snapshot() const { return runtime_.snapshot(); }

 private:
  static uint16_t speedUnitsPerWallMs(SimulationSpeed speed);
  static SignalState withoutTransition(SignalState state);
  void clearTransientInputs();
  void advanceOnce(uint32_t simulationDeltaMs);

  AuroraRuntime runtime_;
  AuroraInputFrame inputs_{};
  uint32_t simulationTimeMs_ = 0;
  uint32_t seed_ = 1;
  bool paused_ = false;
  SimulationSpeed speed_ = SimulationSpeed::X1;
  uint8_t fractionalSimulationUnits_ = 0;
};
