#include "avr_led_driver.h"

#include "avr_config.h"

void AvrLedDriver::begin() {
  FastLED.addLeds<WS2812B, AvrConfig::LedDataPin, AvrConfig::LedColorOrder>(
      leds_, Aurora::LedCount);
  FastLED.setMaxPowerInVoltsAndMilliamps(AvrConfig::LedVolts,
                                         AvrConfig::LedMaxMilliamps);
  fill_solid(leds_, Aurora::LedCount, CRGB::Black);
  enterSafeState();
}

void AvrLedDriver::output(const Aurora::Rgb8 *frame, uint8_t count,
                          bool stripPowerPresent, bool frameChanged) {
  if (!stripPowerPresent) {
    enterSafeState();
    stripPowerWasPresent_ = false;
    return;
  }

  digitalWrite(AvrConfig::LedDataPin, LOW);
  pinMode(AvrConfig::LedDataPin, OUTPUT);
  if (frameChanged || !stripPowerWasPresent_) {
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
  stripPowerWasPresent_ = true;
}

void AvrLedDriver::enterSafeState() {
  digitalWrite(AvrConfig::LedDataPin, LOW);
  pinMode(AvrConfig::LedDataPin, INPUT);
}
