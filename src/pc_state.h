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
  bool forcedPreview() const { return forcedPreview_; }
  bool forcedLatched() const { return forcedLatched_; }
  uint32_t powerHoldStartMs() const { return powerHoldStartMs_; }
  void clearRequests();
  void markStartupComplete();
  void markShutdownComplete();
private:
  PcState state_=PcState::Off;
  uint32_t powerHoldStartMs_=0;
  bool trackingHold_=false, forcedPreview_=false, forcedLatched_=false;
  bool startupRequested_=false, shutdownRequested_=false, resetRequested_=false;
};
const __FlashStringHelper *pcStateName(PcState state);
