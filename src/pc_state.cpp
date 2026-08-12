#include "pc_state.h"

#include "config.h"

void PcStateMachine::reset() {
  state_ = PcState::Off;
  powerHoldStartMs_ = 0;
  startingSinceMs_ = 0;
  awaitingShutdownSinceMs_ = 0;
  trackingHold_ = false;
  forcedLatched_ = false;
}

void PcStateMachine::enterOff() {
  state_ = PcState::Off;
  trackingHold_ = false;
  forcedLatched_ = false;
}

void PcStateMachine::enterRunning() {
  state_ = PcState::Running;
  trackingHold_ = false;
  forcedLatched_ = false;
}

void PcStateMachine::enterStarting(uint32_t nowMs) {
  state_ = PcState::Starting;
  startingSinceMs_ = nowMs;
  trackingHold_ = false;
  forcedLatched_ = false;
}

void PcStateMachine::enterAwaitShutdown(uint32_t nowMs) {
  state_ = PcState::AwaitShutdown;
  awaitingShutdownSinceMs_ = nowMs;
}

PcStateEvents PcStateMachine::update(const PcStateInputs &inputs,
                                     uint32_t nowMs) {
  PcStateEvents events{};

  switch (state_) {
    case PcState::Off:
      if (inputs.powerButtonPressed) {
        enterStarting(nowMs);
      } else if (inputs.powerMode == PowerLedMode::Blinking) {
        state_ = PcState::Sleeping;
      } else if (inputs.powerMode == PowerLedMode::On) {
        state_ = PcState::Running;
      }
      break;

    case PcState::Starting:
      if (inputs.powerMode == PowerLedMode::Blinking) {
        state_ = PcState::Sleeping;
        break;
      }

      if (inputs.powerMode == PowerLedMode::Off &&
          nowMs - startingSinceMs_ >= Config::StartingTimeoutMs) {
        enterOff();
        break;
      }

      if (inputs.startupAnimationFinished &&
          inputs.powerMode == PowerLedMode::On) {
        state_ = PcState::Running;
      }
      break;

    case PcState::Running:
      if (inputs.resetButtonPressed) events.requestReset = true;

      if (inputs.powerButtonPressed) {
        trackingHold_ = true;
        forcedLatched_ = false;
        powerHoldStartMs_ = nowMs;
      }

      if (trackingHold_ && inputs.powerButton && !forcedLatched_ &&
          nowMs - powerHoldStartMs_ >= Config::PowerHoldForcedMs) {
        forcedLatched_ = true;
      }

      if (trackingHold_ && inputs.powerButtonReleased) {
        const bool heldLongEnough =
            forcedLatched_ ||
            nowMs - powerHoldStartMs_ >= Config::PowerHoldForcedMs;
        trackingHold_ = false;
        forcedLatched_ = heldLongEnough;
        enterAwaitShutdown(nowMs);
        break;
      }

      if (!trackingHold_) {
        if (inputs.powerMode == PowerLedMode::Blinking) {
          state_ = PcState::Sleeping;
        } else if (inputs.powerMode == PowerLedMode::Off) {
          enterOff();
        }
      }
      break;

    case PcState::Sleeping:
      if (inputs.powerMode == PowerLedMode::Off) {
        enterOff();
      } else if (inputs.powerMode == PowerLedMode::On) {
        state_ = PcState::Running;
      }
      break;

    case PcState::AwaitShutdown:
      if (inputs.powerMode == PowerLedMode::Off) {
        enterOff();
      } else if (nowMs - awaitingShutdownSinceMs_ >=
                 Config::AwaitShutdownTimeoutMs) {
        enterRunning();
      }
      break;
  }

  return events;
}
