#pragma once

#include <cstdint>

#include "simulator_session.h"

class SimulatorApp {
 public:
  SimulatorApp() = default;

  void update(uint32_t wallDeltaMs);
  void draw();

 private:
  SimulatorSession session_;
  float ledSquareBrightnessScale_ = 2.0F;
};
