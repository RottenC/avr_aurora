#include "simulator_session.h"

#include <algorithm>

#include "config.h"

namespace {

constexpr uint32_t MaxAcceptedWallDeltaMs = 40;
constexpr uint8_t SimulationUnitsPerMs = 20;

}  // namespace

SimulatorSession::SimulatorSession() : runtime_(Config::runtimeConfig()) {
  inputs_.powerLed = SignalState::High;
  inputs_.stripPowerPresent = true;
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

SignalState SimulatorSession::withoutTransition(SignalState state) {
  if (state == SignalState::Rising) return SignalState::High;
  if (state == SignalState::Falling) return SignalState::Low;
  return state;
}

void SimulatorSession::clearTransientInputs() {
  inputs_.powerLed = withoutTransition(inputs_.powerLed);
  inputs_.hddLed = withoutTransition(inputs_.hddLed);
  inputs_.powerButton = withoutTransition(inputs_.powerButton);
  inputs_.resetButton = withoutTransition(inputs_.resetButton);
  inputs_.hddContribution = 0;
}

void SimulatorSession::advanceOnce(uint32_t simulationDeltaMs) {
  simulationTimeMs_ += simulationDeltaMs;
  runtime_.step(inputs_, simulationTimeMs_);
  clearTransientInputs();
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
  clearTransientInputs();
  simulationTimeMs_ = 0;
  fractionalSimulationUnits_ = 0;
  runtime_.reset(seed_, simulationTimeMs_);
  runtime_.requestFrameUpdate();
  runtime_.step(inputs_, simulationTimeMs_);
  clearTransientInputs();
}
