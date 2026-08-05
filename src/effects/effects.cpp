#include "effects.h"

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

uint32_t durationFor(TransitionEffect effect,
                     const EffectControllerConfig &effectConfig,
                     const PcStateConfig &pcConfig) {
  switch (effect) {
    case TransitionEffect::Startup:
      return effectConfig.startupDurationMs;
    case TransitionEffect::Shutdown:
      return effectConfig.shutdownDurationMs;
    case TransitionEffect::Reset:
      return effectConfig.resetDurationMs;
    case TransitionEffect::ForcedShutdown:
      return pcConfig.forcedHoldMs;
    case TransitionEffect::None:
      return 0;
  }
  return 0;
}

}  // namespace

void renderBlack(Aurora::Rgb8 *leds, uint8_t count) {
  Aurora::fillRgb(leds, count, {0, 0, 0});
}

void renderSleep(Aurora::Rgb8 *leds, uint8_t count,
                 const SleepEffectConfig &config, uint32_t nowMs) {
  renderBlack(leds, count);
  if (count == 0 || config.travelIntervalMs == 0 ||
      config.secondaryBrightnessDivisor == 0) {
    return;
  }
  const uint8_t primary = (nowMs / config.travelIntervalMs) % count;
  const uint8_t secondary =
      (primary + config.secondaryPointOffset) % count;
  const uint8_t brightness = Aurora::periodicBrightness(
      config.beatsPerMinute, config.minimumBrightness,
      config.pointBrightness, nowMs);
  leds[primary] = Aurora::hsvToRgb(config.primaryHue,
                                  config.primarySaturation, brightness);
  leds[secondary] = Aurora::hsvToRgb(
      config.secondaryHue, config.secondarySaturation,
      brightness / config.secondaryBrightnessDivisor);
}

void renderTransition(Aurora::Rgb8 *leds, uint8_t count,
                      TransitionEffect effect, uint32_t startedAt,
                      uint32_t nowMs,
                      const EffectControllerConfig &effectConfig,
                      const PcStateConfig &pcConfig,
                      const TransitionRenderConfig &renderConfig) {
  renderBlack(leds, count);
  const uint32_t age = nowMs - startedAt;
  if (effect == TransitionEffect::Startup) {
    const uint32_t duration =
        durationFor(effect, effectConfig, pcConfig);
    const uint8_t lit = duration == 0
                            ? count
                            : static_cast<uint8_t>(
                                  (static_cast<uint32_t>(count) * age) /
                                  duration);
    for (uint8_t index = 0; index < count && index <= lit; ++index) {
      leds[index] = Aurora::hsvToRgb(
          static_cast<uint8_t>(renderConfig.startupHueBase + index),
          renderConfig.startupSaturation, renderConfig.startupBrightness);
    }
  } else if (effect == TransitionEffect::Shutdown || effect == TransitionEffect::Reset) {
    const uint32_t duration = durationFor(effect, effectConfig, pcConfig);
    const uint8_t originSpan =
        static_cast<uint8_t>(renderConfig.shutdownOriginMax -
                             renderConfig.shutdownOriginMin + 1);
    const uint8_t origin =
        static_cast<uint8_t>(renderConfig.shutdownOriginMin +
                             ((startedAt >> 2) % originSpan));
    const uint8_t radius =
        duration == 0
            ? count
            : static_cast<uint8_t>(
                  (static_cast<uint32_t>(count) * age) / duration);
    const Aurora::Rgb8 color = effect == TransitionEffect::Reset
                                  ? renderConfig.resetColor
                                  : renderConfig.shutdownColor;
    for (uint8_t index = 0; index < count; ++index) {
      const uint8_t distance =
          index > origin ? index - origin : origin - index;
      if (distance == radius) leds[index] = color;
    }
  } else if (effect == TransitionEffect::ForcedShutdown) {
    const uint32_t boundedAge =
        age < pcConfig.forcedHoldMs ? age : pcConfig.forcedHoldMs;
    const uint8_t brightness =
        boundedAge < renderConfig.forcedFlashAtMs
            ? interpolatedU8(boundedAge, 0, renderConfig.forcedFlashAtMs,
                             renderConfig.forcedInitialBrightness,
                             renderConfig.forcedFlashBrightness)
            : interpolatedU8(boundedAge, renderConfig.forcedFlashAtMs,
                             pcConfig.forcedHoldMs,
                             renderConfig.forcedFlashBrightness, 0);
    Aurora::fillRgb(leds, count,
                    Aurora::hsvToRgb(renderConfig.forcedHue,
                                     renderConfig.forcedSaturation,
                                     brightness));
  }
}
