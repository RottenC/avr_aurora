#include "simulator_session.h"

#include "config.h"

SimulatorSession::SimulatorSession() : runtime_(Config::runtimeConfig()) {
  inputs_.powerLed = true;
  inputs_.stripPowerPresent = true;
  reset(seed_);
}

void SimulatorSession::reset(uint32_t seed) {
  seed_ = seed;
  simulationTimeMs_ = 0;
  runtime_.reset(seed_, simulationTimeMs_);
  runtime_.requestFrameUpdate();
  runtime_.step(inputs_, simulationTimeMs_);
}

void SimulatorSession::advance(uint32_t deltaMs) {
  simulationTimeMs_ += deltaMs;
  runtime_.step(inputs_, simulationTimeMs_);
}
