#include "avr_led_driver.h"

#include "avr_config.h"

AvrLedDriver::AvrLedDriver()
    : stripPowerSafety_(AvrConfig::DebounceMs) {}

void AvrLedDriver::begin() {
  FastLED.addLeds<WS2812B, AvrConfig::LedDataPin, AvrConfig::LedColorOrder>(
      leds_, Aurora::LedCount);
  FastLED.setMaxPowerInVoltsAndMilliamps(AvrConfig::LedVolts,
                                         AvrConfig::LedMaxMilliamps);
  fill_solid(leds_, Aurora::LedCount, CRGB::Black);
  stripPowerSafety_.reset(false, false, 0);
  enterSafeState();
}

bool AvrLedDriver::updatePowerState(bool rawStripPowerPresent,
                                    bool logicalStripPowerPresent,
                                    uint32_t nowMs) {
  const StripPowerSafetyResult result = stripPowerSafety_.update(
      rawStripPowerPresent, logicalStripPowerPresent, nowMs);
  outputAllowed_ = result.outputAllowed;
  if (result.safeStateRequired) enterSafeState();
  if (result.requestFrameUpdate) waitingForFreshFrame_ = true;
  return result.requestFrameUpdate;
}

void AvrLedDriver::output(const Aurora::Rgb8 *frame, uint8_t count,
                          bool frameUpdated) {
  if (!outputAllowed_ || (waitingForFreshFrame_ && !frameUpdated)) {
    enterSafeState();
    return;
  }

  digitalWrite(AvrConfig::LedDataPin, LOW);
  pinMode(AvrConfig::LedDataPin, OUTPUT);
  if (frameUpdated || !dataOutputWasEnabled_) {
    const uint8_t copyCount =
        count < Aurora::LedCount ? count : Aurora::LedCount;
    for (uint8_t index = 0; index < copyCount; ++index) {
      leds_[index] = CRGB(frame[index].r, frame[index].g, frame[index].b);
    }
    for (uint8_t index = copyCount; index < Aurora::LedCount; ++index) {
      leds_[index] = CRGB::Black;
    }
    FastLED.show();
  }
  waitingForFreshFrame_ = false;
  dataOutputWasEnabled_ = true;
}

void AvrLedDriver::enterSafeState() {
  digitalWrite(AvrConfig::LedDataPin, LOW);
  pinMode(AvrConfig::LedDataPin, INPUT);
  dataOutputWasEnabled_ = false;
}
