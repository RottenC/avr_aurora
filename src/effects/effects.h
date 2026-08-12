#pragma once

#include <stdint.h>

#include "../core/rgb8.h"

void renderBlack(Aurora::Rgb8 *leds, uint8_t count);
void renderSleep(Aurora::Rgb8 *leds, uint8_t count, uint32_t nowMs);
