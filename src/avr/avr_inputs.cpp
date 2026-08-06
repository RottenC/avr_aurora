#include "avr_inputs.h"

#include "avr_config.h"

namespace {

bool normalizedRead(uint8_t pin, bool activeHigh) {
  const bool high = digitalRead(pin) == HIGH;
  return activeHigh ? high : !high;
}

SignalState steadyState(bool high) {
  return high ? SignalState::High : SignalState::Low;
}

}  // namespace

void AvrInputs::begin() {
  pinMode(AvrConfig::PowerLedPin, INPUT);
  pinMode(AvrConfig::HddLedPin, INPUT);
  pinMode(AvrConfig::StripPowerPresentPin, INPUT);
  // PWR_SW and RESET are observed through external high-impedance circuits.
  pinMode(AvrConfig::PowerButtonPin, INPUT);
  pinMode(AvrConfig::ResetButtonPin, INPUT);

  const uint32_t nowMs = millis();
  powerLed_.stable = powerLed_.candidate =
      normalizedRead(AvrConfig::PowerLedPin, AvrConfig::PowerLedActiveHigh);
  powerButton_.stable = powerButton_.candidate = normalizedRead(
      AvrConfig::PowerButtonPin, AvrConfig::PowerButtonActiveHigh);
  resetButton_.stable = resetButton_.candidate =
      normalizedRead(AvrConfig::ResetButtonPin, AvrConfig::ResetButtonActiveHigh);
  rawStripPowerPresent_ = normalizedRead(
      AvrConfig::StripPowerPresentPin,
      AvrConfig::StripPowerPresentActiveHigh);
  stripPower_.stable = stripPower_.candidate = rawStripPowerPresent_;
  powerLed_.changedAt = powerButton_.changedAt = resetButton_.changedAt =
      stripPower_.changedAt = nowMs;

  hddHigh_ = normalizedRead(AvrConfig::HddLedPin,
                            AvrConfig::HddLedActiveHigh);
  frame_.powerLed = steadyState(powerLed_.stable);
  frame_.hddLed = steadyState(hddHigh_);
  frame_.powerButton = steadyState(powerButton_.stable);
  frame_.resetButton = steadyState(resetButton_.stable);
  frame_.stripPowerPresent = stripPower_.stable;
}

SignalState AvrInputs::updateOne(Debounced &input, bool raw,
                                 uint32_t nowMs) {
  if (raw != input.candidate) {
    input.candidate = raw;
    input.changedAt = nowMs;
  }
  if (input.stable != input.candidate &&
      nowMs - input.changedAt >= AvrConfig::DebounceMs) {
    const bool previous = input.stable;
    input.stable = input.candidate;
    return !previous && input.stable ? SignalState::Rising
                                     : SignalState::Falling;
  }
  return steadyState(input.stable);
}

void AvrInputs::update(uint32_t nowMs) {
  frame_.powerLed = steadyState(powerLed_.stable);
  frame_.powerButton = steadyState(powerButton_.stable);
  frame_.resetButton = steadyState(resetButton_.stable);

  // TODO: Reintroduce the HDD edge interrupt only if measurements on real
  // hardware show that ordinary sampling loses visually significant activity.
  const bool hddHigh = normalizedRead(AvrConfig::HddLedPin,
                                      AvrConfig::HddLedActiveHigh);
  if (hddHigh != hddHigh_) {
    frame_.hddLed = hddHigh ? SignalState::Rising : SignalState::Falling;
    hddHigh_ = hddHigh;
  } else {
    frame_.hddLed = steadyState(hddHigh_);
  }
  frame_.hddContribution = 0;

  rawStripPowerPresent_ = normalizedRead(
      AvrConfig::StripPowerPresentPin,
      AvrConfig::StripPowerPresentActiveHigh);

  if (nowMs - lastPollMs_ < AvrConfig::InputPollMs) return;
  lastPollMs_ = nowMs;

  frame_.powerLed = updateOne(
      powerLed_, normalizedRead(AvrConfig::PowerLedPin,
                                AvrConfig::PowerLedActiveHigh),
      nowMs);
  updateOne(stripPower_, rawStripPowerPresent_, nowMs);
  frame_.powerButton = updateOne(
      powerButton_, normalizedRead(AvrConfig::PowerButtonPin,
                                   AvrConfig::PowerButtonActiveHigh),
      nowMs);
  frame_.resetButton = updateOne(
      resetButton_, normalizedRead(AvrConfig::ResetButtonPin,
                                   AvrConfig::ResetButtonActiveHigh),
      nowMs);

  frame_.stripPowerPresent = stripPower_.stable;
}
