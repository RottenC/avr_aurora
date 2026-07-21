#include "pc_state.h"

PcStateMachine::PcStateMachine(const PcStateConfig &config) : config_(config) {}

void PcStateMachine::enterStarting(PcStateEvents &events, uint32_t nowMs, bool stripPowerPresent) {
  state_ = PcState::Starting;
  startingSinceMs_ = nowMs;
  trackingHold_ = false;
  forcedLatched_ = false;
  if (stripPowerPresent) events.startupRequested = true;
}

PcStateEvents PcStateMachine::update(const PcStateInputs &in, uint32_t nowMs) {
  PcStateEvents events = {false, false, false, false, false};

  if (state_ == PcState::Off) {
    if (in.powerMode == PowerLedMode::On && in.stripPowerPresent) {
      state_ = PcState::Running;
      forcedLatched_ = false;
    } else if (in.powerMode == PowerLedMode::Blinking) {
      state_ = PcState::Sleeping;
      forcedLatched_ = false;
    } else if (in.powerButtonPressed) {
      enterStarting(events, nowMs, in.stripPowerPresent);
    }
    return events;
  }

  if (state_ == PcState::Starting) {
    if (!in.stripPowerPresent) {
      events.cancelStartupRequested = true;
    } else if (in.startupAnimationFinished && in.powerMode == PowerLedMode::On) {
      state_ = PcState::Running;
    } else if (nowMs - startingSinceMs_ >= config_.startingTimeoutMs && in.powerMode != PowerLedMode::On) {
      state_ = PcState::Off;
      events.cancelStartupRequested = true;
    }
    return events;
  }

  if (state_ == PcState::Running) {
    if (in.resetButtonPressed) events.resetRequested = true;
    if (in.powerButtonPressed) { trackingHold_ = true; powerHoldStartMs_ = nowMs; }
    if (trackingHold_ && in.powerButton && nowMs - powerHoldStartMs_ >= config_.powerHoldForcedMs) {
      forcedLatched_ = true;
      events.forcedShutdownRequested = true;
    }
    if (in.powerButtonReleased && trackingHold_ && !forcedLatched_) {
      trackingHold_ = false;
      state_ = PcState::ShuttingDown;
      events.shutdownRequested = true;
    } else if (in.powerButtonReleased) {
      trackingHold_ = false;
    }
    if (in.powerMode == PowerLedMode::Blinking) state_ = PcState::Sleeping;
    if (in.powerMode == PowerLedMode::Off) state_ = PcState::Off;
    return events;
  }

  if (state_ == PcState::Sleeping) {
    if (in.powerMode == PowerLedMode::On && in.stripPowerPresent) state_ = PcState::Running;
    if (in.powerMode == PowerLedMode::Off) state_ = PcState::Off;
    return events;
  }

  if (state_ == PcState::ShuttingDown && in.powerMode == PowerLedMode::Off) state_ = PcState::Off;
  return events;
}
