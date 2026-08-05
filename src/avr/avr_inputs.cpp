#include "avr_inputs.h"

#include "avr_config.h"

namespace {

volatile uint8_t gHddEdgeCount = 0;

bool normalizedRead(uint8_t pin, bool activeHigh) {
  const bool high = digitalRead(pin) == HIGH;
  return activeHigh ? high : !high;
}

void onHddActiveEdge() {
  if (gHddEdgeCount != UINT8_MAX) ++gHddEdgeCount;
}

}  // namespace

void AvrInputs::begin() {
  pinMode(AvrConfig::PowerLedPin, INPUT);
  pinMode(AvrConfig::HddLedPin, INPUT);
  pinMode(AvrConfig::StripPowerPresentPin, INPUT);
  // PWR_SW and RESET are observed through external high-impedance circuits.
  pinMode(AvrConfig::PowerButtonPin, INPUT);
  pinMode(AvrConfig::ResetButtonPin, INPUT);
  pinMode(AvrConfig::DebugButtonPin,
          AvrConfig::DebugButtonActiveHigh ? INPUT : INPUT_PULLUP);

  const uint32_t nowMs = millis();
  powerLed_.stable = powerLed_.candidate =
      normalizedRead(AvrConfig::PowerLedPin, AvrConfig::PowerLedActiveHigh);
  powerButton_.stable = powerButton_.candidate = normalizedRead(
      AvrConfig::PowerButtonPin, AvrConfig::PowerButtonActiveHigh);
  resetButton_.stable = resetButton_.candidate =
      normalizedRead(AvrConfig::ResetButtonPin, AvrConfig::ResetButtonActiveHigh);
  stripPower_.stable = stripPower_.candidate = normalizedRead(
      AvrConfig::StripPowerPresentPin,
      AvrConfig::StripPowerPresentActiveHigh);
  debugButton_.stable = debugButton_.candidate = normalizedRead(
      AvrConfig::DebugButtonPin, AvrConfig::DebugButtonActiveHigh);
  powerLed_.changedAt = powerButton_.changedAt = resetButton_.changedAt =
      stripPower_.changedAt = debugButton_.changedAt = nowMs;

  frame_.powerLed = powerLed_.stable;
  frame_.hddLed =
      normalizedRead(AvrConfig::HddLedPin, AvrConfig::HddLedActiveHigh);
  frame_.powerButton = powerButton_.stable;
  frame_.resetButton = resetButton_.stable;
  frame_.stripPowerPresent = stripPower_.stable;
  frame_.debugButton = debugButton_.stable;

  const int interruptNumber = digitalPinToInterrupt(AvrConfig::HddLedPin);
  if (interruptNumber != NOT_AN_INTERRUPT) {
    attachInterrupt(interruptNumber, onHddActiveEdge,
                    AvrConfig::HddLedActiveHigh ? RISING : FALLING);
  }
}

void AvrInputs::updateOne(Debounced &input, bool raw, uint32_t nowMs,
                          bool &pressed, bool &released) {
  if (raw != input.candidate) {
    input.candidate = raw;
    input.changedAt = nowMs;
  }
  if (input.stable != input.candidate &&
      nowMs - input.changedAt >= AvrConfig::DebounceMs) {
    const bool previous = input.stable;
    input.stable = input.candidate;
    pressed = !previous && input.stable;
    released = previous && !input.stable;
  }
}

void AvrInputs::update(uint32_t nowMs) {
  frame_.powerButtonPressed = false;
  frame_.powerButtonReleased = false;
  frame_.resetButtonPressed = false;
  frame_.resetButtonReleased = false;
  frame_.debugButtonPressed = false;
  frame_.hddLed =
      normalizedRead(AvrConfig::HddLedPin, AvrConfig::HddLedActiveHigh);
  frame_.hddActiveEdges = consumeHddEdges();

  if (nowMs - lastPollMs_ < AvrConfig::InputPollMs) return;
  lastPollMs_ = nowMs;

  bool unusedPressed = false;
  bool unusedReleased = false;
  updateOne(powerLed_,
            normalizedRead(AvrConfig::PowerLedPin,
                           AvrConfig::PowerLedActiveHigh),
            nowMs, unusedPressed, unusedReleased);
  updateOne(stripPower_,
            normalizedRead(AvrConfig::StripPowerPresentPin,
                           AvrConfig::StripPowerPresentActiveHigh),
            nowMs, unusedPressed, unusedReleased);
  updateOne(powerButton_,
            normalizedRead(AvrConfig::PowerButtonPin,
                           AvrConfig::PowerButtonActiveHigh),
            nowMs, frame_.powerButtonPressed, frame_.powerButtonReleased);
  updateOne(resetButton_,
            normalizedRead(AvrConfig::ResetButtonPin,
                           AvrConfig::ResetButtonActiveHigh),
            nowMs, frame_.resetButtonPressed, frame_.resetButtonReleased);
  updateOne(debugButton_,
            normalizedRead(AvrConfig::DebugButtonPin,
                           AvrConfig::DebugButtonActiveHigh),
            nowMs, frame_.debugButtonPressed, unusedReleased);

  frame_.powerLed = powerLed_.stable;
  frame_.powerButton = powerButton_.stable;
  frame_.resetButton = resetButton_.stable;
  frame_.stripPowerPresent = stripPower_.stable;
  frame_.debugButton = debugButton_.stable;
}

uint8_t AvrInputs::consumeHddEdges() {
  noInterrupts();
  const uint8_t result = gHddEdgeCount;
  gHddEdgeCount = 0;
  interrupts();
  return result;
}
