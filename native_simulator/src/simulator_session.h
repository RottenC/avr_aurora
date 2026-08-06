#pragma once

#include <cstdint>

#include <aurora/aurora_runtime.h>

class SimulatorSession {
 public:
  SimulatorSession();

  void reset(uint32_t seed);
  void advance(uint32_t deltaMs);

  uint32_t simulationTimeMs() const { return simulationTimeMs_; }
  const AuroraRuntime &runtime() const { return runtime_; }
  const AuroraSnapshot &snapshot() const { return runtime_.snapshot(); }

 private:
  AuroraRuntime runtime_;
  AuroraInputFrame inputs_{};
  uint32_t simulationTimeMs_ = 0;
  uint32_t seed_ = 1;
};
