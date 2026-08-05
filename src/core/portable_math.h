#pragma once

#include <stdint.h>

#include "rgb8.h"

namespace Aurora {

uint8_t scale8(uint8_t value, uint8_t scale);
uint8_t lerp8(uint8_t from, uint8_t to, uint8_t amount);
Rgb8 hsvToRgb(uint8_t hue, uint8_t saturation, uint8_t value);
uint8_t periodicBrightness(uint8_t beatsPerMinute, uint8_t minimum,
                           uint8_t maximum, uint32_t nowMs);
void fillRgb(Rgb8 *pixels, uint8_t count, const Rgb8 &color);

}  // namespace Aurora
