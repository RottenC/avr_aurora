#pragma once

#include <stdint.h>
#include "state_types.h"

struct PcStateInputs {
  bool powerButton;
  bool powerButtonPressed;
  bool powerButtonReleased;
  bool resetButtonPressed;
  PowerLedMode powerMode;
  bool startupAnimationFinished;
};

struct PcStateEvents {
  bool requestReset = false;
};

class PcStateMachine {
public:
  void reset();
  PcStateEvents update(const PcStateInputs &inputs, uint32_t nowMs);
  PcState state() const { return state_; }
  bool forcedLatched() const { return forcedLatched_; }
  bool trackingPowerHold() const { return trackingHold_; }
  uint32_t powerHoldStartMs() const { return powerHoldStartMs_; }
  uint32_t powerHoldElapsed(uint32_t nowMs) const {
    return trackingHold_ ? nowMs - powerHoldStartMs_ : 0;
  }

private:
  void enterOff();
  void enterRunning();
  void enterStarting(uint32_t nowMs);
  void enterAwaitShutdown(uint32_t nowMs);

  PcState state_ = PcState::Off;
  uint32_t powerHoldStartMs_ = 0;
  uint32_t startingSinceMs_ = 0;
  uint32_t awaitingShutdownSinceMs_ = 0;
  bool trackingHold_ = false;
  bool forcedLatched_ = false;
};
