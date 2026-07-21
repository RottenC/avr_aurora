#include "inputs.h"
#include "config.h"

namespace {
volatile bool gHddEdgePending = false;

bool normalizedRead(uint8_t pin, bool activeHigh) {
  const bool high = digitalRead(pin) == HIGH;
  return activeHigh ? high : !high;
}

void onHddActiveEdge() {
  gHddEdgePending = true;
}
}

void Inputs::begin() {
  pinMode(Config::PowerLedPin, INPUT);
  pinMode(Config::HddLedPin, INPUT);
  pinMode(Config::StripPowerPresentPin, INPUT);
  // PWR_SW and RESET are observed through external high-impedance sense circuits.
  pinMode(Config::PowerButtonPin, INPUT);
  pinMode(Config::ResetButtonPin, INPUT);
  pinMode(Config::DebugButtonPin, Config::DebugButtonActiveHigh ? INPUT : INPUT_PULLUP);

  const uint32_t now = millis();
  powerLed_.stable = powerLed_.candidate = normalizedRead(Config::PowerLedPin, Config::PowerLedActiveHigh);
  powerButton_.stable = powerButton_.candidate = normalizedRead(Config::PowerButtonPin, Config::PowerButtonActiveHigh);
  resetButton_.stable = resetButton_.candidate = normalizedRead(Config::ResetButtonPin, Config::ResetButtonActiveHigh);
  stripPower_.stable = stripPower_.candidate = normalizedRead(Config::StripPowerPresentPin, Config::StripPowerPresentActiveHigh);
  debugButton_.stable = debugButton_.candidate = normalizedRead(Config::DebugButtonPin, Config::DebugButtonActiveHigh);
  powerLed_.changedAt = powerButton_.changedAt = resetButton_.changedAt = stripPower_.changedAt = debugButton_.changedAt = now;

  stable_.hddLed = normalizedRead(Config::HddLedPin, Config::HddLedActiveHigh);
  const int interruptNumber = digitalPinToInterrupt(Config::HddLedPin);
  if (interruptNumber != NOT_AN_INTERRUPT) {
    attachInterrupt(interruptNumber, onHddActiveEdge, Config::HddLedActiveHigh ? RISING : FALLING);
  }
}

void Inputs::updateOne(Debounced &d, bool raw, uint32_t nowMs, bool &pressed, bool &released) {
  if (raw != d.candidate) { d.candidate = raw; d.changedAt = nowMs; }
  if (d.stable != d.candidate && nowMs - d.changedAt >= Config::DebounceMs) {
    const bool old = d.stable;
    d.stable = d.candidate;
    pressed = !old && d.stable;
    released = old && !d.stable;
  }
}

void Inputs::update(uint32_t nowMs) {
  powerButtonPressed_ = powerButtonReleased_ = resetButtonPressed_ = debugButtonPressed_ = false;
  stable_.hddLed = normalizedRead(Config::HddLedPin, Config::HddLedActiveHigh);
  if (nowMs - lastPollMs_ < Config::InputPollMs) return;
  lastPollMs_ = nowMs;

  bool dummyPressed = false, dummyReleased = false;
  updateOne(powerLed_, normalizedRead(Config::PowerLedPin, Config::PowerLedActiveHigh), nowMs, dummyPressed, dummyReleased);
  updateOne(stripPower_, normalizedRead(Config::StripPowerPresentPin, Config::StripPowerPresentActiveHigh), nowMs, dummyPressed, dummyReleased);
  updateOne(powerButton_, normalizedRead(Config::PowerButtonPin, Config::PowerButtonActiveHigh), nowMs, powerButtonPressed_, powerButtonReleased_);
  updateOne(resetButton_, normalizedRead(Config::ResetButtonPin, Config::ResetButtonActiveHigh), nowMs, resetButtonPressed_, dummyReleased);
  updateOne(debugButton_, normalizedRead(Config::DebugButtonPin, Config::DebugButtonActiveHigh), nowMs, debugButtonPressed_, dummyReleased);
  stable_.powerLed = powerLed_.stable;
  stable_.powerButton = powerButton_.stable;
  stable_.resetButton = resetButton_.stable;
  stable_.stripPowerPresent = stripPower_.stable;
  stable_.debugButton = debugButton_.stable;
}

bool Inputs::consumeHddEdge() {
  noInterrupts();
  const bool result = gHddEdgePending;
  gHddEdgePending = false;
  interrupts();
  return result;
}