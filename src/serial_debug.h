#pragma once

#include <Arduino.h>
#include "inputs.h"
#include "state_types.h"

class SerialDebug {
public:
  void begin();
  void update(const NormalizedInputs &inputs,
              PcState state,
              TransitionEffect transition,
              PowerLedMode powerMode,
              uint8_t hdd,
              uint32_t nowMs);

private:
  uint32_t lastSnapshotMs_ = 0;
  uint32_t lastHddEventMs_ = 0;
  bool initialized_ = false;
  NormalizedInputs lastInputs_{};
  PcState lastState_ = PcState::Off;
  TransitionEffect lastTransition_ = TransitionEffect::None;
  PowerLedMode lastPowerMode_ = PowerLedMode::Off;
  uint8_t lastHdd_ = 0;
};
