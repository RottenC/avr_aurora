#include "config.h"
#include "led_output.h"
#include "effects/effects.h"

void LedOutput::begin(uint32_t auroraSeed){
  auroraSeed_ = auroraSeed;
  aurora_.reset(auroraSeed_);
  deactivateAurora();
  FastLED.addLeds<WS2812B, Config::LedDataPin, Config::LedColorOrder>(leds_, Config::LedCount);
  FastLED.setMaxPowerInVoltsAndMilliamps(Config::LedVolts, Config::LedMaxMilliamps);
  renderBlack(leds_, Config::LedCount);
  digitalWrite(Config::LedDataPin, LOW);
  pinMode(Config::LedDataPin, INPUT);
}

void LedOutput::render(PcState state, TransitionEffect transition, uint32_t transitionStartMs, uint8_t hddActivity, bool stripPowerPresent, uint32_t nowMs){
  (void)hddActivity;

  if (!stripPowerPresent) {
    deactivateAurora();
    renderBlack(leds_, Config::LedCount);
    digitalWrite(Config::LedDataPin, LOW);
    pinMode(Config::LedDataPin, INPUT);
    return;
  }

  digitalWrite(Config::LedDataPin, LOW);
  pinMode(Config::LedDataPin, OUTPUT);
  if (transition != TransitionEffect::None) {
    deactivateAurora();
    renderTransition(leds_, Config::LedCount, transition, transitionStartMs, nowMs);
  } else if (state == PcState::Running || state == PcState::Starting) {
    renderAuroraField(nowMs);
  } else if (state == PcState::Sleeping) {
    deactivateAurora();
    renderSleep(leds_, Config::LedCount, nowMs);
  } else {
    deactivateAurora();
    renderBlack(leds_, Config::LedCount);
  }
  FastLED.show();
}

void LedOutput::deactivateAurora() {
  auroraActive_ = false;
}

void LedOutput::renderAuroraField(uint32_t nowMs) {
  if (!auroraActive_) {
    aurora_.reset(auroraSeed_);
    lastAuroraUpdateMs_ = nowMs;
    auroraActive_ = true;
  } else {
    aurora_.advance(nowMs - lastAuroraUpdateMs_);
    lastAuroraUpdateMs_ = nowMs;
  }

  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    const Aurora::Rgb8 pixel = aurora_.pixel(index);
    leds_[index] = CRGB(pixel.r, pixel.g, pixel.b);
  }
}
