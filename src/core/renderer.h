#pragma once

#include <stdint.h>

#include "../effects/aurora_field.h"
#include "../effects/effects.h"
#include "../state_types.h"
#include "geometry.h"
#include "rgb8.h"

struct AuroraRenderContext {
  PcState pcState = PcState::Off;
  TransitionEffect transition = TransitionEffect::None;

  uint32_t nowMs = 0;
  uint32_t transitionStartedAtMs = 0;
  uint32_t transitionElapsedMs = 0;
  uint32_t transitionDurationMs = 0;
  uint8_t transitionProgress = 0;

  uint8_t hddActivity = 0;
  bool logicalStripPowerPresent = false;
};

class AuroraRenderer {
 public:
  AuroraRenderer();

  void reset(uint32_t seed, uint32_t nowMs = 0);
  void render(const AuroraRenderContext &context);

  const Aurora::Rgb8 *frame() const { return frame_; }
  uint8_t ledCount() const { return Aurora::LedCount; }
  Aurora::FieldCellDiagnostics auroraDiagnostics(uint8_t index) const {
    return aurora_.diagnostics(index);
  }

 private:
  void deactivateAurora();
  void renderAurora(uint8_t hddActivity, uint32_t nowMs);

  Aurora::Field aurora_;
  Aurora::Rgb8 frame_[Aurora::LedCount];
  uint32_t seed_;
  uint32_t lastAuroraUpdateMs_;
  bool auroraActive_;
};
