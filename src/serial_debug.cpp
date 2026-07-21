#include "serial_debug.h"
#include "config.h"

namespace {
const __FlashStringHelper *pcStateName(PcState state) {
  switch (state) {
    case PcState::Off: return F("Off");
    case PcState::Starting: return F("Starting");
    case PcState::Running: return F("Running");
    case PcState::Sleeping: return F("Sleeping");
    case PcState::ShuttingDown: return F("ShuttingDown");
  }
  return F("?");
}

const __FlashStringHelper *transitionName(TransitionEffect effect) {
  switch (effect) {
    case TransitionEffect::None: return F("None");
    case TransitionEffect::Startup: return F("Startup");
    case TransitionEffect::Shutdown: return F("Shutdown");
    case TransitionEffect::ForcedShutdown: return F("ForcedShutdown");
    case TransitionEffect::Reset: return F("Reset");
  }
  return F("?");
}

const __FlashStringHelper *powerModeName(PowerLedMode mode) {
  switch (mode) {
    case PowerLedMode::Off: return F("Off");
    case PowerLedMode::On: return F("On");
    case PowerLedMode::Blinking: return F("Blinking");
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

void logSnapshot(const NormalizedInputs &inputs,
                 PcState state,
                 TransitionEffect transition,
                 PowerLedMode powerMode,
                 uint8_t hdd) {
  Serial.print(F("snapshot pwrLed="));
  Serial.print(inputs.powerLed);
  Serial.print(F(" hddLed="));
  Serial.print(inputs.hddLed);
  Serial.print(F(" pwrBtn="));
  Serial.print(inputs.powerButton);
  Serial.print(F(" rstBtn="));
  Serial.print(inputs.resetButton);
  Serial.print(F(" strip="));
  Serial.print(inputs.stripPowerPresent);
  Serial.print(F(" powerMode="));
  Serial.print(powerModeName(powerMode));
  Serial.print(F(" state="));
  Serial.print(pcStateName(state));
  Serial.print(F(" transition="));
  Serial.print(transitionName(transition));
  Serial.print(F(" hdd="));
  Serial.println(hdd);
}
}

void SerialDebug::begin() {
  Serial.begin(Config::SerialBaud);
}

void SerialDebug::update(const NormalizedInputs &inputs,
                         PcState state,
                         TransitionEffect transition,
                         PowerLedMode powerMode,
                         uint8_t hdd,
                         uint32_t nowMs) {
  if (!initialized_) {
    initialized_ = true;
    lastInputs_ = inputs;
    lastState_ = state;
    lastTransition_ = transition;
    lastPowerMode_ = powerMode;
    lastHdd_ = hdd;
    lastSnapshotMs_ = nowMs;
    lastHddEventMs_ = nowMs;
    logSnapshot(inputs, state, transition, powerMode, hdd);
    return;
  }

  if (lastState_ != state) {
    Serial.print(F("state "));
    Serial.print(pcStateName(lastState_));
    Serial.print(F(" -> "));
    Serial.println(pcStateName(state));
    lastState_ = state;
  }
  if (lastTransition_ != transition) {
    Serial.print(F("transition "));
    Serial.print(transitionName(lastTransition_));
    Serial.print(F(" -> "));
    Serial.println(transitionName(transition));
    lastTransition_ = transition;
  }
  if (lastPowerMode_ != powerMode) {
    Serial.print(F("powerMode "));
    Serial.print(powerModeName(lastPowerMode_));
    Serial.print(F(" -> "));
    Serial.println(powerModeName(powerMode));
    lastPowerMode_ = powerMode;
  }

  if (lastInputs_.powerLed != inputs.powerLed) {
    logBoolChange(F("powerLed"), lastInputs_.powerLed, inputs.powerLed);
  }
  if (lastInputs_.powerButton != inputs.powerButton) {
    logBoolChange(F("powerButton"), lastInputs_.powerButton, inputs.powerButton);
  }
  if (lastInputs_.resetButton != inputs.resetButton) {
    logBoolChange(F("resetButton"), lastInputs_.resetButton, inputs.resetButton);
  }
  if (lastInputs_.stripPowerPresent != inputs.stripPowerPresent) {
    logBoolChange(F("stripPower"), lastInputs_.stripPowerPresent, inputs.stripPowerPresent);
  }
  if (lastInputs_.debugButton != inputs.debugButton) {
    logBoolChange(F("debugButton"), lastInputs_.debugButton, inputs.debugButton);
  }
  lastInputs_ = inputs;

  const uint8_t hddDelta = hdd > lastHdd_ ? hdd - lastHdd_ : lastHdd_ - hdd;
  const bool hddBecameIdleOrActive = (hdd == 0) != (lastHdd_ == 0);
  if ((hddDelta >= Config::DebugHddEventDelta || hddBecameIdleOrActive) &&
      nowMs - lastHddEventMs_ >= Config::DebugHddEventIntervalMs) {
    Serial.print(F("hdd "));
    Serial.print(lastHdd_);
    Serial.print(F(" -> "));
    Serial.println(hdd);
    lastHdd_ = hdd;
    lastHddEventMs_ = nowMs;
  }

  if (nowMs - lastSnapshotMs_ >= Config::DebugSnapshotIntervalMs) {
    logSnapshot(inputs, state, transition, powerMode, hdd);
    lastSnapshotMs_ = nowMs;
    lastHddEventMs_ = nowMs;
    lastHdd_ = hdd;
  }
}
