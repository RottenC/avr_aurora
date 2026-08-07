#include "simulator_session.h"

#include <algorithm>

#include "config.h"

namespace {

constexpr uint32_t MaxAcceptedWallDeltaMs = 40;
constexpr uint8_t SimulationUnitsPerMs = 20;

}  // namespace

SimulatorSession::SimulatorSession() {
  reset();
}

uint16_t SimulatorSession::speedUnitsPerWallMs(SimulationSpeed speed) {
  switch (speed) {
    case SimulationSpeed::X0_1:
      return 2;
    case SimulationSpeed::X0_25:
      return 5;
    case SimulationSpeed::X0_5:
      return 10;
    case SimulationSpeed::X1:
      return 20;
    case SimulationSpeed::X2:
      return 40;
    case SimulationSpeed::X4:
      return 80;
    case SimulationSpeed::X8:
      return 160;
    case SimulationSpeed::X16:
      return 320;
  }
  return 20;
}

void SimulatorSession::advanceOnce(uint32_t simulationDeltaMs) {
  const uint32_t intervalStart = simulationTimeMs_;
  const bool initialPowerLevel = powerLedGenerator_.level();
  const bool initialHddLevel = hddGenerator_.level();
  const PowerLedUpdate power = powerLedGenerator_.update(
      simulationDeltaMs, inputs_.manualPowerLed());
  const HddUpdate hdd = hddGenerator_.update(
      simulationDeltaMs, inputs_.manualHddLed());
  lastGeneratedHddEdges_ = hdd.activeEdges;

  bool powerLevel = initialPowerLevel;
  for (std::size_t index = 0; index < power.transitionCount; ++index) {
    const PowerLedTransition &transition = power.transitions[index];
    powerLevel = transition.active;
    runtime_.step(inputs_.buildFrame(powerLevel, initialHddLevel, 0),
                  intervalStart + transition.offsetMs);
  }
  simulationTimeMs_ += simulationDeltaMs;
  // The final call carries the generated HDD level and accumulated active
  // edges, including when a Power LED transition also occurred at the end.
  runtime_.step(inputs_.buildFrame(power.level, hdd.level, hdd.activeEdges),
                simulationTimeMs_);
}

void SimulatorSession::update(uint32_t wallDeltaMs) {
  if (paused_) return;

  const uint32_t acceptedWallDeltaMs =
      std::min(wallDeltaMs, MaxAcceptedWallDeltaMs);
  const uint32_t scaledSimulationUnits =
      acceptedWallDeltaMs * speedUnitsPerWallMs(speed_) +
      fractionalSimulationUnits_;
  const uint32_t simulationDeltaMs =
      scaledSimulationUnits / SimulationUnitsPerMs;
  fractionalSimulationUnits_ = static_cast<uint8_t>(
      scaledSimulationUnits % SimulationUnitsPerMs);
  if (simulationDeltaMs != 0) advanceOnce(simulationDeltaMs);
}

void SimulatorSession::stepFrame() {
  if (!paused_) return;
  advanceOnce(Config::FrameIntervalMs);
}

void SimulatorSession::reset() {
  simulationTimeMs_ = 0;
  fractionalSimulationUnits_ = 0;
  lastGeneratedHddEdges_ = 0;
  inputs_.resetTransientState();
  powerLedGenerator_.reset();
  hddGenerator_.reset();
  runtime_.reset(seed_, simulationTimeMs_);
  runtime_.requestFrameUpdate();
  const PowerLedUpdate power = powerLedGenerator_.update(
      0, inputs_.manualPowerLed());
  const HddUpdate hdd = hddGenerator_.update(0, inputs_.manualHddLed());
  runtime_.step(inputs_.buildFrame(power.level, hdd.level, 0),
                simulationTimeMs_);
}
