#include "pc_state.h"
#include "config.h"

void PcStateMachine::clearRequests() {
  startupRequested_=shutdownRequested_=resetRequested_=false;
  holdPreviewRequested_=cancelHoldPreviewRequested_=false;
}

void PcStateMachine::markStartupComplete() {
  if (state_ == PcState::Starting) state_ = PcState::Running;
}

void PcStateMachine::update(const NormalizedInputs &in, bool powerPressed, bool powerReleased, bool resetPressed, PowerLedMode powerMode, uint32_t nowMs) {
  clearRequests();

  if (!initialized_) {
    if (bootAtMs_ == 0) bootAtMs_ = nowMs;
    if (nowMs - bootAtMs_ < Config::InitialStateObserveMs) return;
    if (powerMode == PowerLedMode::Blinking) state_ = PcState::Sleeping;
    else if (powerMode == PowerLedMode::On && in.stripPowerPresent) state_ = PcState::Running;
    else state_ = PcState::Off;
    initialized_ = true;
  }

  if (state_ == PcState::Off && powerPressed) {
    state_ = PcState::Starting;
    startupRequested_ = true;
    forcedLatched_ = false;
    return;
  }

  if (state_ == PcState::Running) {
    if (resetPressed) resetRequested_ = true;

    if (powerPressed) {
      trackingHold_ = true;
      forcedLatched_ = false;
      powerHoldStartMs_ = nowMs;
      holdPreviewRequested_ = true;
    }

    if (trackingHold_ && in.powerButton && !forcedLatched_ &&
        nowMs - powerHoldStartMs_ >= Config::PowerHoldForcedMs) {
      forcedLatched_ = true;
    }

    if (powerReleased && trackingHold_) {
      trackingHold_ = false;
      if (forcedLatched_) {
        state_ = PcState::ShuttingDown;
      } else {
        cancelHoldPreviewRequested_ = true;
        state_ = PcState::ShuttingDown;
        shutdownRequested_ = true;
      }
    }

    if (!trackingHold_) {
      if (powerMode == PowerLedMode::Blinking) state_ = PcState::Sleeping;
      else if (powerMode == PowerLedMode::Off) state_ = PcState::Off;
    }
  } else if (state_ == PcState::Sleeping) {
    if (powerMode == PowerLedMode::On) state_ = PcState::Running;
    else if (powerMode == PowerLedMode::Off) state_ = PcState::Off;
  } else if (state_ == PcState::ShuttingDown) {
    if (powerMode == PowerLedMode::Off) state_ = PcState::Off;
  }

  if (!in.stripPowerPresent && powerMode == PowerLedMode::Off) state_ = PcState::Off;
}

const __FlashStringHelper *pcStateName(PcState state) {
  switch(state){case PcState::Off:return F("Off");case PcState::Starting:return F("Starting");case PcState::Running:return F("Running");case PcState::Sleeping:return F("Sleeping");case PcState::ShuttingDown:return F("ShuttingDown");}
  return F("?");
}