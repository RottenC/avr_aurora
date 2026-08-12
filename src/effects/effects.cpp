#include "effects.h"

#include "../config.h"
#include "../core/portable_math.h"

void renderBlack(Aurora::Rgb8 *leds, uint8_t count) {
  Aurora::fillRgb(leds, count, {0, 0, 0});
}

void renderSleep(Aurora::Rgb8 *leds, uint8_t count, uint32_t nowMs) {
  renderBlack(leds, count);
  if (count == 0) return;
  const uint8_t primary = (nowMs / Config::SleepTravelIntervalMs) % count;
  const uint8_t secondary =
      (primary + Config::SleepSecondaryPointOffset) % count;
  const uint8_t brightness = Aurora::periodicBrightness(
      Config::SleepBeatsPerMinute, Config::SleepMinimumBrightness,
      Config::SleepPointBrightness, nowMs);
  leds[primary] = Aurora::hsvToRgb(
      Config::SleepPrimaryHue, Config::SleepPrimarySaturation, brightness);
  leds[secondary] = Aurora::hsvToRgb(
      Config::SleepSecondaryHue, Config::SleepSecondarySaturation,
      brightness / Config::SleepSecondaryBrightnessDivisor);
}
