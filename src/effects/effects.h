#pragma once
#include <Arduino.h>
#include "../fastled_compat.h"
#include "../effect_controller.h"
#include "../pc_state.h"

void renderBlack(CRGB *leds, uint8_t count);
void renderAurora(CRGB *leds, uint8_t count, uint32_t nowMs, uint8_t hddActivity);
void renderSleep(CRGB *leds, uint8_t count, uint32_t nowMs);
void renderTransition(CRGB *leds, uint8_t count, TransitionEffect effect, uint32_t startedAt, uint32_t nowMs);
