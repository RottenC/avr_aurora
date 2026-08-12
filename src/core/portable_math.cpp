#include "portable_math.h"

namespace {

uint8_t scale8Video(uint8_t value, uint8_t scale) {
  return static_cast<uint8_t>(
      ((static_cast<uint16_t>(value) * scale) >> 8) +
      ((value != 0 && scale != 0) ? 1U : 0U));
}

}  // namespace

namespace Aurora {

uint8_t sine8(uint8_t phase) {
  static constexpr uint8_t kBaseSlope[] = {0, 49, 49, 41, 90, 27, 117, 10};

  uint8_t offset = phase;
  if ((phase & 0x40U) != 0) offset = UINT8_MAX - offset;
  offset &= 0x3FU;

  uint8_t sectionOffset = offset & 0x0FU;
  if ((phase & 0x40U) != 0) ++sectionOffset;
  const uint8_t section = offset >> 4;
  const uint8_t base = kBaseSlope[section * 2];
  const uint8_t slope = kBaseSlope[section * 2 + 1];
  const uint8_t delta =
      static_cast<uint8_t>((static_cast<uint16_t>(slope) * sectionOffset) >>
                           4);
  int16_t result = static_cast<int16_t>(base) + delta;
  if ((phase & 0x80U) != 0) result = -result;
  return static_cast<uint8_t>(result + 128);
}

uint8_t tri8(uint8_t phase) {
  if ((phase & 0x80U) != 0) phase = UINT8_MAX - phase;
  return static_cast<uint8_t>(phase << 1);
}

uint8_t hash8(uint8_t value) {
  value ^= 0xA3U;
  value ^= value >> 4;
  value = static_cast<uint8_t>(value * 0x27U);
  value ^= value >> 3;
  return value;
}

uint8_t scale8(uint8_t value, uint8_t scale) {
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(value) * (static_cast<uint16_t>(scale) + 1U)) >>
      8);
}

uint8_t lerp8(uint8_t from, uint8_t to, uint8_t amount) {
  const uint32_t numerator =
      static_cast<uint32_t>(from) * (UINT8_MAX - amount) +
      static_cast<uint32_t>(to) * amount;
  return static_cast<uint8_t>(numerator / UINT8_MAX);
}

Rgb8 hsvToRgb(uint8_t hue, uint8_t saturation, uint8_t value) {
  const uint8_t offset = hue & 0x1FU;
  const uint8_t offset8 = static_cast<uint8_t>(offset << 3);
  const uint8_t third = scale8(offset8, 256U / 3U);
  Rgb8 rgb;

  switch (hue >> 5) {
    case 0:
      rgb = {static_cast<uint8_t>(UINT8_MAX - third), third, 0};
      break;
    case 1:
      rgb = {171, static_cast<uint8_t>(85 + third), 0};
      break;
    case 2: {
      const uint8_t twoThirds = scale8(offset8, (256U * 2U) / 3U);
      rgb = {static_cast<uint8_t>(171 - twoThirds),
             static_cast<uint8_t>(170 + third), 0};
      break;
    }
    case 3:
      rgb = {0, static_cast<uint8_t>(UINT8_MAX - third), third};
      break;
    case 4: {
      const uint8_t twoThirds = scale8(offset8, (256U * 2U) / 3U);
      rgb = {0, static_cast<uint8_t>(171 - twoThirds),
             static_cast<uint8_t>(85 + twoThirds)};
      break;
    }
    case 5:
      rgb = {third, 0, static_cast<uint8_t>(UINT8_MAX - third)};
      break;
    case 6:
      rgb = {static_cast<uint8_t>(85 + third), 0,
             static_cast<uint8_t>(171 - third)};
      break;
    default:
      rgb = {static_cast<uint8_t>(170 + third), 0,
             static_cast<uint8_t>(85 - third)};
      break;
  }

  if (saturation != UINT8_MAX) {
    if (saturation == 0) {
      rgb = {UINT8_MAX, UINT8_MAX, UINT8_MAX};
    } else {
      uint8_t desaturation = UINT8_MAX - saturation;
      desaturation = scale8Video(desaturation, desaturation);
      const uint8_t saturationScale = UINT8_MAX - desaturation;
      rgb.r = static_cast<uint8_t>(scale8(rgb.r, saturationScale) +
                                   desaturation);
      rgb.g = static_cast<uint8_t>(scale8(rgb.g, saturationScale) +
                                   desaturation);
      rgb.b = static_cast<uint8_t>(scale8(rgb.b, saturationScale) +
                                   desaturation);
    }
  }

  if (value != UINT8_MAX) {
    value = scale8Video(value, value);
    if (value == 0) {
      return {0, 0, 0};
    }
    rgb.r = scale8(rgb.r, value);
    rgb.g = scale8(rgb.g, value);
    rgb.b = scale8(rgb.b, value);
  }
  return rgb;
}

uint8_t periodicBrightness(uint8_t beatsPerMinute, uint8_t minimum,
                           uint8_t maximum, uint32_t nowMs) {
  const uint16_t beatsPerMinuteQ8_8 =
      static_cast<uint16_t>(beatsPerMinute) << 8;
  const uint32_t beatProduct =
      static_cast<uint32_t>(nowMs * beatsPerMinuteQ8_8 * 280UL);
  const uint16_t beat = static_cast<uint16_t>(beatProduct >> 16);
  const uint8_t phase = static_cast<uint8_t>(beat >> 8);
  const uint8_t range = static_cast<uint8_t>(maximum - minimum);
  return static_cast<uint8_t>(minimum + scale8(sine8(phase), range));
}

void fillRgb(Rgb8 *pixels, uint8_t count, const Rgb8 &color) {
  for (uint8_t index = 0; index < count; ++index) pixels[index] = color;
}

}  // namespace Aurora
