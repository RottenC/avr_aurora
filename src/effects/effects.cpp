#include "effects.h"

#include "../config.h"
#include "../core/portable_math.h"

namespace {

uint8_t interpolatedU8(uint32_t value, uint32_t fromLow, uint32_t fromHigh,
                       uint8_t toLow, uint8_t toHigh) {
  if (fromHigh <= fromLow || value <= fromLow) return toLow;
  if (value >= fromHigh) return toHigh;
  const int32_t outputDelta =
      static_cast<int16_t>(toHigh) - static_cast<int16_t>(toLow);
  const int32_t scaledDelta =
      static_cast<int32_t>(value - fromLow) * outputDelta /
      static_cast<int32_t>(fromHigh - fromLow);
  return static_cast<uint8_t>(static_cast<int16_t>(toLow) + scaledDelta);
}

}  // namespace

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

void renderTransition(Aurora::Rgb8 *leds, uint8_t count,
                      TransitionEffect effect, uint32_t startedAt,
                      uint32_t transitionElapsedMs,
                      uint32_t transitionDurationMs) {
  renderBlack(leds, count);
  const uint32_t age = transitionElapsedMs;
  if (effect == TransitionEffect::Startup) {
    const uint8_t lit = transitionDurationMs == 0
                            ? count
                            : static_cast<uint8_t>(
                                  (static_cast<uint32_t>(count) * age) /
                                  transitionDurationMs);
    for (uint8_t index = 0; index < count && index <= lit; ++index) {
      leds[index] = Aurora::hsvToRgb(
          static_cast<uint8_t>(Config::StartupHueBase + index),
          Config::StartupSaturation, Config::StartupBrightness);
    }
  } else if (effect == TransitionEffect::Shutdown || effect == TransitionEffect::Reset) {
    const uint8_t originSpan =
        static_cast<uint8_t>(Config::ShutdownOriginMax -
                             Config::ShutdownOriginMin + 1);
    const uint8_t origin =
        static_cast<uint8_t>(Config::ShutdownOriginMin +
                             ((startedAt >> 2) % originSpan));
    const uint8_t radius =
        transitionDurationMs == 0
            ? count
            : static_cast<uint8_t>(
                  (static_cast<uint32_t>(count) * age) /
                  transitionDurationMs);
    const Aurora::Rgb8 color = effect == TransitionEffect::Reset
                                  ? Config::ResetColor
                                  : Config::ShutdownColor;
    for (uint8_t index = 0; index < count; ++index) {
      const uint8_t distance =
          index > origin ? index - origin : origin - index;
      if (distance == radius) leds[index] = color;
    }
  } else if (effect == TransitionEffect::ForcedShutdown) {
    const uint32_t boundedAge =
        age < transitionDurationMs ? age : transitionDurationMs;
    const uint8_t brightness =
        boundedAge < Config::ForcedFlashAtMs
            ? interpolatedU8(boundedAge, 0, Config::ForcedFlashAtMs,
                             Config::ForcedInitialBrightness,
                             Config::ForcedFlashBrightness)
            : interpolatedU8(boundedAge, Config::ForcedFlashAtMs,
                             transitionDurationMs,
                             Config::ForcedFlashBrightness, 0);
    Aurora::fillRgb(leds, count,
                    Aurora::hsvToRgb(Config::ForcedHue,
                                     Config::ForcedSaturation,
                                     brightness));
  }
}
