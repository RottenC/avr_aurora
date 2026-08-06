#include "avr_serial_debug.h"

#include "avr_config.h"

namespace {

const __FlashStringHelper *pcStateName(PcState state) {
  switch (state) {
    case PcState::Off:
      return F("Off");
    case PcState::Starting:
      return F("Starting");
    case PcState::Running:
      return F("Running");
    case PcState::Sleeping:
      return F("Sleeping");
    case PcState::AwaitShutdown:
      return F("AwaitShutdown");
    case PcState::Warn:
      return F("Warn");
  }
  return F("?");
}

const __FlashStringHelper *transitionName(TransitionEffect effect) {
  switch (effect) {
    case TransitionEffect::None:
      return F("None");
    case TransitionEffect::Startup:
      return F("Startup");
    case TransitionEffect::Shutdown:
      return F("Shutdown");
    case TransitionEffect::ForcedShutdown:
      return F("ForcedShutdown");
    case TransitionEffect::Reset:
      return F("Reset");
  }
  return F("?");
}

const __FlashStringHelper *powerModeName(PowerLedMode mode) {
  switch (mode) {
    case PowerLedMode::Off:
      return F("Off");
    case PowerLedMode::On:
      return F("On");
    case PowerLedMode::Blinking:
      return F("Blinking");
  }
  return F("?");
}

const __FlashStringHelper *signalStateName(SignalState state) {
  switch (state) {
    case SignalState::Low:
      return F("Low");
    case SignalState::Rising:
      return F("Rising");
    case SignalState::High:
      return F("High");
    case SignalState::Falling:
      return F("Falling");
    case SignalState::Blinking:
      return F("Blinking");
  }
  return F("?");
}

void logBoolChange(const __FlashStringHelper *name, bool before, bool after) {
  Serial.print(name);
  Serial.print(F(" "));
  Serial.print(before);
  Serial.print(F(" -> "));
  Serial.println(after);
}

bool signalObservationChanged(SignalState before, SignalState after) {
  if (before == after) return false;
  if (signalIsHigh(before) && signalIsHigh(after)) return false;
  const bool beforeLow = before == SignalState::Low ||
                         before == SignalState::Falling;
  const bool afterLow = after == SignalState::Low ||
                        after == SignalState::Falling;
  return !beforeLow || !afterLow;
}

void logSignalChange(const __FlashStringHelper *name, SignalState before,
                     SignalState after) {
  Serial.print(name);
  Serial.print(F(" "));
  Serial.print(signalStateName(before));
  Serial.print(F(" -> "));
  Serial.println(signalStateName(after));
}

void logSnapshot(const AuroraInputFrame &inputs,
                 const AuroraSnapshot &snapshot) {
  Serial.print(F("snapshot pwrLed="));
  Serial.print(signalStateName(inputs.powerLed));
  Serial.print(F(" hddLed="));
  Serial.print(signalStateName(inputs.hddLed));
  Serial.print(F(" hddContribution="));
  Serial.print(inputs.hddContribution);
  Serial.print(F(" pwrBtn="));
  Serial.print(signalStateName(inputs.powerButton));
  Serial.print(F(" rstBtn="));
  Serial.print(signalStateName(inputs.resetButton));
  Serial.print(F(" strip="));
  Serial.print(inputs.stripPowerPresent);
  Serial.print(F(" powerMode="));
  Serial.print(powerModeName(snapshot.powerLedMode));
  Serial.print(F(" state="));
  Serial.print(pcStateName(snapshot.pcState));
  Serial.print(F(" transition="));
  Serial.print(transitionName(snapshot.transition));
  Serial.print(F(" hdd="));
  Serial.println(snapshot.hddActivity);
}

}  // namespace

void AvrSerialDebug::begin() { Serial.begin(AvrConfig::SerialBaud); }

void AvrSerialDebug::update(const AuroraInputFrame &inputs,
                            const AuroraSnapshot &snapshot, uint32_t nowMs) {
  if (!initialized_) {
    initialized_ = true;
    lastInputs_ = inputs;
    lastState_ = snapshot.pcState;
    lastTransition_ = snapshot.transition;
    lastPowerMode_ = snapshot.powerLedMode;
    lastHdd_ = snapshot.hddActivity;
    lastSnapshotMs_ = nowMs;
    lastHddEventMs_ = nowMs;
    logSnapshot(inputs, snapshot);
    return;
  }

  if (lastState_ != snapshot.pcState) {
    Serial.print(F("state "));
    Serial.print(pcStateName(lastState_));
    Serial.print(F(" -> "));
    Serial.println(pcStateName(snapshot.pcState));
    lastState_ = snapshot.pcState;
  }
  if (lastTransition_ != snapshot.transition) {
    Serial.print(F("transition "));
    Serial.print(transitionName(lastTransition_));
    Serial.print(F(" -> "));
    Serial.println(transitionName(snapshot.transition));
    lastTransition_ = snapshot.transition;
  }
  if (lastPowerMode_ != snapshot.powerLedMode) {
    Serial.print(F("powerMode "));
    Serial.print(powerModeName(lastPowerMode_));
    Serial.print(F(" -> "));
    Serial.println(powerModeName(snapshot.powerLedMode));
    lastPowerMode_ = snapshot.powerLedMode;
  }

  if (signalObservationChanged(lastInputs_.powerLed, inputs.powerLed)) {
    logSignalChange(F("powerLed"), lastInputs_.powerLed, inputs.powerLed);
  }
  if (signalObservationChanged(lastInputs_.hddLed, inputs.hddLed)) {
    logSignalChange(F("hddLed"), lastInputs_.hddLed, inputs.hddLed);
  }
  if (signalObservationChanged(lastInputs_.powerButton,
                               inputs.powerButton)) {
    logSignalChange(F("powerButton"), lastInputs_.powerButton,
                    inputs.powerButton);
  }
  if (signalObservationChanged(lastInputs_.resetButton,
                               inputs.resetButton)) {
    logSignalChange(F("resetButton"), lastInputs_.resetButton,
                    inputs.resetButton);
  }
  if (lastInputs_.stripPowerPresent != inputs.stripPowerPresent) {
    logBoolChange(F("stripPower"), lastInputs_.stripPowerPresent,
                  inputs.stripPowerPresent);
  }
  lastInputs_ = inputs;

  const uint8_t hdd = snapshot.hddActivity;
  const uint8_t hddDelta = hdd > lastHdd_ ? hdd - lastHdd_ : lastHdd_ - hdd;
  const bool hddBecameIdleOrActive = (hdd == 0) != (lastHdd_ == 0);
  if ((hddDelta >= AvrConfig::DebugHddEventDelta ||
       hddBecameIdleOrActive) &&
      nowMs - lastHddEventMs_ >= AvrConfig::DebugHddEventIntervalMs) {
    Serial.print(F("hdd "));
    Serial.print(lastHdd_);
    Serial.print(F(" -> "));
    Serial.println(hdd);
    lastHdd_ = hdd;
    lastHddEventMs_ = nowMs;
  }

  if (nowMs - lastSnapshotMs_ >= AvrConfig::DebugSnapshotIntervalMs) {
    logSnapshot(inputs, snapshot);
    lastSnapshotMs_ = nowMs;
    lastHddEventMs_ = nowMs;
    lastHdd_ = hdd;
  }
}
