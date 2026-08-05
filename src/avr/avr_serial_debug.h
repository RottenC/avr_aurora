#pragma once

#include <Arduino.h>

#include "../core/aurora_runtime.h"

class AvrSerialDebug {
 public:
  void begin();
  void update(const AuroraInputFrame &inputs,
              const AuroraSnapshot &snapshot, uint32_t nowMs);

 private:
  uint32_t lastSnapshotMs_ = 0;
  uint32_t lastHddEventMs_ = 0;
  bool initialized_ = false;
  AuroraInputFrame lastInputs_{};
  PcState lastState_ = PcState::Off;
  TransitionEffect lastTransition_ = TransitionEffect::None;
  PowerLedMode lastPowerMode_ = PowerLedMode::Off;
  uint8_t lastHdd_ = 0;
};
