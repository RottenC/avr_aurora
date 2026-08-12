#pragma once

#include <cstdint>

#include <aurora/aurora_runtime.h>

#include "hdd_generator.h"
#include "power_led_generator.h"
#include "virtual_inputs.h"

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
  VirtualInputs &inputs() { return inputs_; }
  const VirtualInputs &inputs() const { return inputs_; }
  PowerLedGenerator &powerLedGenerator() { return powerLedGenerator_; }
  const PowerLedGenerator &powerLedGenerator() const { return powerLedGenerator_; }
  HddGenerator &hddGenerator() { return hddGenerator_; }
  const HddGenerator &hddGenerator() const { return hddGenerator_; }
  uint8_t lastGeneratedHddEdges() const { return lastGeneratedHddEdges_; }

 private:
  static uint16_t speedUnitsPerWallMs(SimulationSpeed speed);
  void advanceOnce(uint32_t simulationDeltaMs);

  AuroraRuntime runtime_;
  VirtualInputs inputs_{};
  PowerLedGenerator powerLedGenerator_{};
  HddGenerator hddGenerator_{};
  uint32_t simulationTimeMs_ = 0;
  uint32_t seed_ = 1;
  bool paused_ = false;
  SimulationSpeed speed_ = SimulationSpeed::X1;
  uint8_t fractionalSimulationUnits_ = 0;
  uint8_t lastGeneratedHddEdges_ = 0;
};
