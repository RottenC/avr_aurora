#include "pc_state.h"

void PcStateMachine::enterOff() {
  state_ = PcState::Off;
  trackingHold_ = false;
  forcedLatched_ = false;
  startupTransitionRequested_ = false;
  startupTransitionFinished_ = false;
  waitingForStripPower_ = false;
}

void PcStateMachine::enterStarting(PcStateEvents &events, bool stripPowerPresent, uint32_t nowMs) {
  state_ = PcState::Starting;
  startingSinceMs_ = nowMs;
  trackingHold_ = false;
  forcedLatched_ = false;
  startupTransitionFinished_ = false;
  waitingForStripPower_ = !stripPowerPresent;
  startupTransitionRequested_ = stripPowerPresent;
  events.requestStartup = stripPowerPresent;
}

void PcStateMachine::leaveStarting(PcStateEvents &events, PcState nextState) {
  if (startupTransitionRequested_) events.cancelStartup = true;
  state_ = nextState;
  startupTransitionRequested_ = false;
  startupTransitionFinished_ = false;
  waitingForStripPower_ = false;
}

void PcStateMachine::enterAwaitShutdown(uint32_t nowMs) {
  state_ = PcState::AwaitShutdown;
  awaitingShutdownSinceMs_ = nowMs;
}

PcStateEvents PcStateMachine::update(const PcStateInputs &inputs, uint32_t nowMs) {
  PcStateEvents events;

  switch (state_) {
    case PcState::Off:
      if (inputs.powerButtonPressed) {
        enterStarting(events, inputs.stripPowerPresent, nowMs);
      } else if (inputs.powerMode == PowerLedMode::Blinking) {
        state_ = PcState::Sleeping;
      } else if (inputs.powerMode == PowerLedMode::On) {
        state_ = PcState::Running;
      }
      break;

    case PcState::Starting:
      if (inputs.powerMode == PowerLedMode::Blinking) {
        leaveStarting(events, PcState::Sleeping);
        break;
      }

      if (inputs.powerMode == PowerLedMode::Off &&
          nowMs - startingSinceMs_ >= config_.startingTimeoutMs) {
        leaveStarting(events, PcState::Off);
        break;
      }

      if (!inputs.stripPowerPresent) {
        if (startupTransitionRequested_) events.cancelStartup = true;
        startupTransitionRequested_ = false;
        startupTransitionFinished_ = false;
        waitingForStripPower_ = true;
        break;
      }

      if (waitingForStripPower_ || !startupTransitionRequested_) {
        waitingForStripPower_ = false;
        startupTransitionRequested_ = true;
        startupTransitionFinished_ = false;
        events.requestStartup = true;
      } else if (inputs.startupTransitionFinished) {
        startupTransitionFinished_ = true;
      }

      if (startupTransitionFinished_ && inputs.powerMode == PowerLedMode::On) {
        leaveStarting(events, PcState::Running);
      }
      break;

    case PcState::Running:
      if (inputs.resetButtonPressed) events.requestReset = true;

      if (inputs.powerButtonPressed) {
        trackingHold_ = true;
        forcedLatched_ = false;
        powerHoldStartMs_ = nowMs;
        events.requestForcedShutdown = true;
      }

      if (trackingHold_ && inputs.powerButton && !forcedLatched_ &&
          nowMs - powerHoldStartMs_ >= config_.forcedHoldMs) {
        forcedLatched_ = true;
      }

      if (trackingHold_ && inputs.powerButtonReleased) {
        const bool heldLongEnough =
            forcedLatched_ || nowMs - powerHoldStartMs_ >= config_.forcedHoldMs;
        trackingHold_ = false;
        forcedLatched_ = heldLongEnough;
        enterAwaitShutdown(nowMs);
        if (!heldLongEnough) {
          events.cancelForcedShutdown = true;
          events.requestShutdown = true;
        }
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
      } else if (inputs.powerMode == PowerLedMode::On &&
                 nowMs - awaitingShutdownSinceMs_ >= config_.shutdownWarningTimeoutMs) {
        state_ = PcState::Warn;
      }
      break;

    case PcState::Warn:
      if (inputs.powerMode == PowerLedMode::Off) enterOff();
      break;
  }

  return events;
}
