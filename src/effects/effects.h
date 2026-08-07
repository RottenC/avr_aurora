#pragma once

#include <stdint.h>

#include "../core/rgb8.h"
#include "../state_types.h"

void renderBlack(Aurora::Rgb8 *leds, uint8_t count);
void renderSleep(Aurora::Rgb8 *leds, uint8_t count, uint32_t nowMs);
void renderTransition(Aurora::Rgb8 *leds, uint8_t count,
                      TransitionEffect effect, uint32_t startedAt,
                      uint32_t transitionElapsedMs,
                      uint32_t transitionDurationMs);
