#include "pc_state.h"
#include "config.h"

void PcStateMachine::clearRequests(){ startupRequested_=shutdownRequested_=resetRequested_=false; }
void PcStateMachine::markStartupComplete(){ if(state_==PcState::Starting) state_=PcState::Running; }
void PcStateMachine::markShutdownComplete(){ if(state_==PcState::ShuttingDown) state_=PcState::Off; }
void PcStateMachine::update(const NormalizedInputs &in, bool powerPressed, bool powerReleased, bool resetPressed, PowerLedMode powerMode, uint32_t nowMs) {
  clearRequests();
  if (state_ == PcState::Off && powerPressed) { state_=PcState::Starting; startupRequested_=true; forcedLatched_=false; }
  if (state_ == PcState::Running) {
    if (resetPressed) resetRequested_=true;
    if (powerPressed) { trackingHold_=true; powerHoldStartMs_=nowMs; }
    if (trackingHold_ && in.powerButton && nowMs - powerHoldStartMs_ >= Config::PowerHoldForcedMs) { forcedPreview_=true; forcedLatched_=true; }
    if (powerReleased && trackingHold_) { trackingHold_=false; forcedPreview_=false; state_=PcState::ShuttingDown; shutdownRequested_=true; }
    if (powerMode == PowerLedMode::Blinking) state_=PcState::Sleeping;
    if (powerMode == PowerLedMode::Off) state_=PcState::Off;
  } else if (state_ == PcState::Sleeping) {
    if (powerMode == PowerLedMode::On) state_=PcState::Running;
    if (powerMode == PowerLedMode::Off) state_=PcState::Off;
  } else if (state_ == PcState::ShuttingDown && powerMode == PowerLedMode::Off) state_=PcState::Off;
  if (!in.stripPowerPresent && powerMode == PowerLedMode::Off) state_=PcState::Off;
}
const __FlashStringHelper *pcStateName(PcState state) {
  switch(state){case PcState::Off:return F("Off");case PcState::Starting:return F("Starting");case PcState::Running:return F("Running");case PcState::Sleeping:return F("Sleeping");case PcState::ShuttingDown:return F("ShuttingDown");}
  return F("?");
}
