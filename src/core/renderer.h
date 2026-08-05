#pragma once

#include <stdint.h>

#include "../effect_controller.h"
#include "../effects/aurora_field.h"
#include "../effects/effects.h"
#include "../pc_state.h"
#include "rgb8.h"

struct AuroraRendererConfig {
  uint32_t frameIntervalMs;
  SleepEffectConfig sleep;
  TransitionRenderConfig transition;
  bool hddAffectsSpeed;
  bool hddAffectsBrightness;
};

class AuroraRenderer {
 public:
  AuroraRenderer(const AuroraRendererConfig &config,
                 const EffectControllerConfig &effectConfig,
                 const PcStateConfig &pcConfig,
                 const Aurora::FieldConfig &fieldConfig);

  void reset(uint32_t seed, uint32_t nowMs = 0);
  void render(PcState state, TransitionEffect transition,
              uint32_t transitionStartedAtMs, uint8_t hddActivity,
              bool stripPowerPresent, uint32_t nowMs);

  const Aurora::Rgb8 *frame() const { return frame_; }
  uint8_t ledCount() const { return Aurora::LedCount; }
  uint32_t frameIntervalMs() const { return config_.frameIntervalMs; }

 private:
  void deactivateAurora();
  void renderAurora(uint8_t hddActivity, uint32_t nowMs);

  AuroraRendererConfig config_;
  EffectControllerConfig effectConfig_;
  PcStateConfig pcConfig_;
  Aurora::Field aurora_;
  Aurora::Rgb8 frame_[Aurora::LedCount];
  uint32_t seed_;
  uint32_t lastAuroraUpdateMs_;
  bool auroraActive_;
};
