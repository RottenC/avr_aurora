#include "config.h"
#include "led_output.h"
#include "effects/effects.h"

void LedOutput::begin(){
  FastLED.addLeds<WS2812B, Config::LedDataPin, Config::LedColorOrder>(leds_, Config::LedCount);
  FastLED.setMaxPowerInVoltsAndMilliamps(Config::LedVolts, Config::LedMaxMilliamps);
  renderBlack(leds_, Config::LedCount);
  digitalWrite(Config::LedDataPin, LOW);
  pinMode(Config::LedDataPin, INPUT);
}

void LedOutput::render(PcState state, TransitionEffect transition, uint32_t transitionStartMs, uint8_t hddActivity, bool stripPowerPresent, uint32_t nowMs){
  if (!stripPowerPresent) {
    renderBlack(leds_, Config::LedCount);
    digitalWrite(Config::LedDataPin, LOW);
    pinMode(Config::LedDataPin, INPUT);
    return;
  }

  digitalWrite(Config::LedDataPin, LOW);
  pinMode(Config::LedDataPin, OUTPUT);
  if (transition != TransitionEffect::None) renderTransition(leds_, Config::LedCount, transition, transitionStartMs, nowMs);
  else if (state == PcState::Running || state == PcState::Starting) renderAurora(leds_, Config::LedCount, nowMs, hddActivity);
  else if (state == PcState::Sleeping) renderSleep(leds_, Config::LedCount, nowMs);
  else renderBlack(leds_, Config::LedCount);
  FastLED.show();
}