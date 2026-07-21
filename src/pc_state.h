#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "state_types.h"

struct PcStateInputs {
  bool stripPowerPresent;
  bool powerButton;
  bool powerButtonPressed;
  bool powerButtonReleased;
  bool resetButtonPressed;
  PowerLedMode powerMode;
  bool startupAnimationFinished;
};

struct PcStateConfig {
  uint32_t powerHoldForcedMs;
  uint32_t startingTimeoutMs;
};

struct PcStateEvents {
  bool startupRequested;
  bool shutdownRequested;
  bool resetRequested;
  bool forcedShutdownRequested;
  bool cancelStartupRequested;
};

class PcStateMachine {
public:
  explicit PcStateMachine(const PcStateConfig &config);
  PcStateEvents update(const PcStateInputs &in, uint32_t nowMs);
  PcState state() const { return state_; }
  bool forcedLatched() const { return forcedLatched_; }
  uint32_t powerHoldStartMs() const { return powerHoldStartMs_; }
private:
  void enterStarting(PcStateEvents &events, uint32_t nowMs, bool stripPowerPresent);
  PcStateConfig config_;
  PcState state_ = PcState::Off;
  uint32_t powerHoldStartMs_ = 0;
  uint32_t startingSinceMs_ = 0;
  bool trackingHold_ = false;
  bool forcedLatched_ = false;
};
