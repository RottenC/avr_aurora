#include <cstdint>

#include "config.h"
#include "simulator_session.h"
#include "virtual_inputs.h"
#include "power_led_generator.h"
#include "hdd_generator.h"

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

bool testVirtualInputs() {
  VirtualInputs inputs;
  inputs.pressPowerButton();
  inputs.pressPowerButton();
  inputs.releasePowerButton();
  AuroraInputFrame frame = inputs.buildFrame(false, false, 0);
  if (frame.powerButton != SignalState::Rising) return false;
  frame = inputs.buildFrame(false, false, 0);
  if (frame.powerButton != SignalState::Falling) return false;
  frame = inputs.buildFrame(false, false, 0);
  if (frame.powerButton != SignalState::Low) return false;
  inputs.pressResetButton();
  frame = inputs.buildFrame(false, false, 0);
  if (frame.resetButton != SignalState::Rising) return false;
  inputs.addHddActiveEdges(250);
  inputs.addHddActiveEdges(20);
  if (inputs.pendingHddActiveEdges() != UINT8_MAX) return false;
  frame = inputs.buildFrame(false, false, 0);
  if (frame.hddContribution != INT16_MAX || inputs.pendingHddActiveEdges() != 0)
    return false;
  inputs.resetTransientState();
  return !inputs.powerButtonHeld() && !inputs.resetButtonHeld();
}

bool testPowerGenerator() {
  PowerLedGenerator generator;
  if (!generator.update(20, true).level) return false;
  generator.setMode(PowerLedSourceMode::Off);
  if (generator.update(20, true).level) return false;
  generator.setMode(PowerLedSourceMode::On);
  if (!generator.update(20, false).level) return false;
  generator.setMode(PowerLedSourceMode::Blinking);
  generator.setHalfPeriodMs(100);
  const PowerLedUpdate update = generator.update(350, false);
  if (!update.level || update.transitionCount != 4 ||
      update.transitions[0].offsetMs != 0 ||
      update.transitions[1].offsetMs != 100 ||
      update.transitions[3].offsetMs != 300) return false;
  generator.reset();
  return !generator.update(99, false).level && generator.update(1, false).level;
}

bool testHddGenerator() {
  HddGenerator first;
  HddGenerator second;
  first.setMode(HddSourceMode::RandomQuake);
  second.setMode(HddSourceMode::RandomQuake);
  bool sawEdge = false;
  for (int index = 0; index < 100; ++index) {
    const HddUpdate a = first.update(100, false);
    const HddUpdate b = second.update(100, false);
    if (a.level != b.level || a.activeEdges != b.activeEdges) return false;
    sawEdge |= a.activeEdges != 0;
  }
  first.reset(); second.reset();
  const HddUpdate repeatedA = first.update(500, false);
  const HddUpdate repeatedB = second.update(500, false);
  return sawEdge && repeatedA.level == repeatedB.level &&
         repeatedA.activeEdges == repeatedB.activeEdges;
}

bool testSessionInputsAndPause() {
  SimulatorSession session;
  session.setPaused(true);
  session.inputs().addHddActiveEdges(8);
  session.update(40);
  if (session.inputs().pendingHddActiveEdges() != 8) return false;
  session.inputs().setStripPowerPresent(false);
  session.stepFrame();
  if (session.snapshot().hddActivity == 0) return false;
  for (uint8_t index = 0; index < session.runtime().ledCount(); ++index) {
    const Aurora::Rgb8 pixel = session.runtime().ledFrame()[index];
    if (pixel.r != 0 || pixel.g != 0 || pixel.b != 0) return false;
  }
  return true;
}

}  // namespace

int main() {
  if (!testNormalPauseStepAndReset()) return 1;
  if (!testEverySpeedAndWallClamp()) return 2;
  if (!testFractionalMillisecondsAccumulate()) return 3;
  if (!testVirtualInputs()) return 4;
  if (!testPowerGenerator()) return 5;
  if (!testHddGenerator()) return 6;
  if (!testSessionInputsAndPause()) return 7;
  return 0;
}
