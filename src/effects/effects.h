#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "../state_types.h"

void renderBlack(CRGB *leds, uint8_t count);
void renderAurora(CRGB *leds, uint8_t count, uint32_t nowMs, uint8_t hddActivity);
void renderSleep(CRGB *leds, uint8_t count, uint32_t nowMs);
void renderTransition(CRGB *leds, uint8_t count, TransitionEffect effect, uint32_t startedAt, uint32_t nowMs);
