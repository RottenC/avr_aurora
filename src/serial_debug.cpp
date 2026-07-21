#include "serial_debug.h"
#include "config.h"

namespace {
const __FlashStringHelper *pcStateName(PcState state) {
  switch(state){case PcState::Off:return F("Off");case PcState::Starting:return F("Starting");case PcState::Running:return F("Running");case PcState::Sleeping:return F("Sleeping");case PcState::ShuttingDown:return F("ShuttingDown");}
  return F("?");
}
const __FlashStringHelper *transitionName(TransitionEffect effect){
  switch(effect){case TransitionEffect::None:return F("None");case TransitionEffect::Startup:return F("Startup");case TransitionEffect::Shutdown:return F("Shutdown");case TransitionEffect::ForcedShutdown:return F("ForcedShutdown");case TransitionEffect::Reset:return F("Reset");}
  return F("?");
}
const __FlashStringHelper *powerModeName(PowerLedMode mode){
  switch(mode){case PowerLedMode::Off:return F("Off");case PowerLedMode::On:return F("On");case PowerLedMode::Blinking:return F("Blinking");}
  return F("?");
}
void logBoolChange(const __FlashStringHelper *name, bool from, bool to) { Serial.print(name); Serial.print(F(" ")); Serial.print(from); Serial.print(F(" -> ")); Serial.println(to); }
void logSnapshot(const NormalizedInputs &in, PcState state, TransitionEffect transition, PowerLedMode powerMode, uint8_t hdd) {
  Serial.print(F("snapshot pwrLed=")); Serial.print(in.powerLed); Serial.print(F(" hddLed=")); Serial.print(in.hddLed);
  Serial.print(F(" pwrBtn=")); Serial.print(in.powerButton); Serial.print(F(" rstBtn=")); Serial.print(in.resetButton);
  Serial.print(F(" strip=")); Serial.print(in.stripPowerPresent); Serial.print(F(" powerMode=")); Serial.print(powerModeName(powerMode));
  Serial.print(F(" state=")); Serial.print(pcStateName(state)); Serial.print(F(" transition=")); Serial.print(transitionName(transition));
  Serial.print(F(" hdd=")); Serial.println(hdd);
}
}

void SerialDebug::begin(){ Serial.begin(Config::SerialBaud); }
void SerialDebug::update(const NormalizedInputs &in, PcState state, TransitionEffect transition, PowerLedMode powerMode, uint8_t hdd, uint32_t nowMs){
  if (!initialized_) { initialized_ = true; lastInputs_ = in; lastState_ = state; lastTransition_ = transition; lastPowerMode_ = powerMode; lastHdd_ = hdd; logSnapshot(in, state, transition, powerMode, hdd); return; }
  if (lastState_ != state) { Serial.print(F("state ")); Serial.print(pcStateName(lastState_)); Serial.print(F(" -> ")); Serial.println(pcStateName(state)); lastState_ = state; }
  if (lastTransition_ != transition) { Serial.print(F("transition ")); Serial.print(transitionName(lastTransition_)); Serial.print(F(" -> ")); Serial.println(transitionName(transition)); lastTransition_ = transition; }
  if (lastPowerMode_ != powerMode) { Serial.print(F("powerMode ")); Serial.print(powerModeName(lastPowerMode_)); Serial.print(F(" -> ")); Serial.println(powerModeName(powerMode)); lastPowerMode_ = powerMode; }
  if (lastInputs_.stripPowerPresent != in.stripPowerPresent) { logBoolChange(F("stripPower"), lastInputs_.stripPowerPresent, in.stripPowerPresent); lastInputs_.stripPowerPresent = in.stripPowerPresent; }
  if (lastInputs_.powerButton != in.powerButton) { logBoolChange(F("powerButton"), lastInputs_.powerButton, in.powerButton); lastInputs_.powerButton = in.powerButton; }
  if (lastInputs_.resetButton != in.resetButton) { logBoolChange(F("resetButton"), lastInputs_.resetButton, in.resetButton); lastInputs_.resetButton = in.resetButton; }
  if ((hdd > lastHdd_ ? hdd - lastHdd_ : lastHdd_ - hdd) >= 8) { Serial.print(F("hdd ")); Serial.print(lastHdd_); Serial.print(F(" -> ")); Serial.println(hdd); lastHdd_ = hdd; }
  if (nowMs - lastSnapshotMs_ >= Config::DebugSnapshotIntervalMs) { lastSnapshotMs_ = nowMs; logSnapshot(in, state, transition, powerMode, hdd); }
}
