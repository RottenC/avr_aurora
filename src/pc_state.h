#pragma once
#include <Arduino.h>
#include "inputs.h"
#include "power_led_tracker.h"

enum class PcState : uint8_t { Off, Starting, Running, Sleeping, ShuttingDown };

class PcStateMachine {
public:
  void update(const NormalizedInputs &in, bool powerPressed, bool powerReleased, bool resetPressed, PowerLedMode powerMode, uint32_t nowMs);
  PcState state() const { return state_; }
  bool startupRequested() const { return startupRequested_; }
  bool shutdownRequested() const { return shutdownRequested_; }
  bool resetRequested() const { return resetRequested_; }
  bool holdPreviewRequested() const { return holdPreviewRequested_; }
  bool cancelHoldPreviewRequested() const { return cancelHoldPreviewRequested_; }
  bool forcedLatched() const { return forcedLatched_; }
  uint32_t powerHoldStartMs() const { return powerHoldStartMs_; }
  void markStartupComplete();
private:
  void clearRequests();
  PcState state_=PcState::Off;
  uint32_t bootAtMs_=0;
  uint32_t powerHoldStartMs_=0;
  bool initialized_=false;
  bool trackingHold_=false;
  bool forcedLatched_=false;
  bool startupRequested_=false;
  bool shutdownRequested_=false;
  bool resetRequested_=false;
  bool holdPreviewRequested_=false;
  bool cancelHoldPreviewRequested_=false;
};
const __FlashStringHelper *pcStateName(PcState state);