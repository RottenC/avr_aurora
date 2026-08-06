#include <cstdint>

#include "config.h"
#include "simulator_session.h"

namespace {

bool testNormalPauseStepAndReset() {
  SimulatorSession session;
  if (session.simulationTimeMs() != 0 || !session.snapshot().frameUpdated) {
    return false;
  }

  session.update(17);
  if (session.simulationTimeMs() != 17) return false;

  session.setPaused(true);
  session.update(40);
  if (session.simulationTimeMs() != 17) return false;

  session.stepFrame();
  if (session.simulationTimeMs() != 17 + Config::FrameIntervalMs) {
    return false;
  }

  session.setSpeed(SimulationSpeed::X8);
  session.reset();
  return session.simulationTimeMs() == 0 && session.paused() &&
         session.speed() == SimulationSpeed::X8 &&
         session.snapshot().frameUpdated;
}

bool testEverySpeedAndWallClamp() {
  constexpr SimulationSpeed Speeds[] = {
      SimulationSpeed::X0_1, SimulationSpeed::X0_25,
      SimulationSpeed::X0_5, SimulationSpeed::X1,
      SimulationSpeed::X2,   SimulationSpeed::X4,
      SimulationSpeed::X8,   SimulationSpeed::X16,
  };
  constexpr uint32_t ExpectedMs[] = {4, 10, 20, 40, 80, 160, 320, 640};

  for (uint8_t index = 0; index < 8; ++index) {
    SimulatorSession session;
    session.setSpeed(Speeds[index]);
    session.update(UINT32_MAX);
    if (session.simulationTimeMs() != ExpectedMs[index]) return false;
  }
  return true;
}

bool testFractionalMillisecondsAccumulate() {
  SimulatorSession session;
  session.setSpeed(SimulationSpeed::X0_1);
  for (uint8_t update = 0; update < 9; ++update) session.update(1);
  if (session.simulationTimeMs() != 0) return false;
  session.update(1);
  return session.simulationTimeMs() == 1;
}

}  // namespace

int main() {
  if (!testNormalPauseStepAndReset()) return 1;
  if (!testEverySpeedAndWallClamp()) return 2;
  if (!testFractionalMillisecondsAccumulate()) return 3;
  return 0;
}
