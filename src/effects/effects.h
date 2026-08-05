#pragma once

#include <stdint.h>

#include "../core/rgb8.h"
#include "../effect_controller.h"
#include "../pc_state.h"

struct SleepEffectConfig {
  uint16_t travelIntervalMs;
  uint8_t secondaryPointOffset;
  uint8_t beatsPerMinute;
  uint8_t minimumBrightness;
  uint8_t pointBrightness;
  uint8_t primaryHue;
  uint8_t primarySaturation;
  uint8_t secondaryHue;
  uint8_t secondarySaturation;
  uint8_t secondaryBrightnessDivisor;
};

struct TransitionRenderConfig {
  uint32_t forcedFlashAtMs;
  uint8_t startupHueBase;
  uint8_t startupSaturation;
  uint8_t startupBrightness;
  uint8_t shutdownOriginMin;
  uint8_t shutdownOriginMax;
  Aurora::Rgb8 shutdownColor;
  Aurora::Rgb8 resetColor;
  uint8_t forcedHue;
  uint8_t forcedSaturation;
  uint8_t forcedInitialBrightness;
  uint8_t forcedFlashBrightness;
};

void renderBlack(Aurora::Rgb8 *leds, uint8_t count);
void renderSleep(Aurora::Rgb8 *leds, uint8_t count,
                 const SleepEffectConfig &config, uint32_t nowMs);
void renderTransition(Aurora::Rgb8 *leds, uint8_t count,
                      TransitionEffect effect, uint32_t startedAt,
                      uint32_t nowMs,
                      const EffectControllerConfig &effectConfig,
                      const PcStateConfig &pcConfig,
                      const TransitionRenderConfig &renderConfig);
