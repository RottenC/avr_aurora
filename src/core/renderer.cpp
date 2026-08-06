#include "renderer.h"

#include "../effects/effects.h"
#include "portable_math.h"

AuroraRenderer::AuroraRenderer(const AuroraRendererConfig &config,
                               const Aurora::FieldConfig &fieldConfig)
    : config_(config),
      aurora_(fieldConfig),
      seed_(fieldConfig.zeroSeedFallback),
      lastAuroraUpdateMs_(0),
      auroraActive_(false) {
  Aurora::fillRgb(frame_, Aurora::LedCount, {0, 0, 0});
}

void AuroraRenderer::reset(uint32_t seed, uint32_t nowMs) {
  seed_ = seed;
  aurora_.reset(seed_);
  lastAuroraUpdateMs_ = nowMs;
  auroraActive_ = false;
  Aurora::fillRgb(frame_, Aurora::LedCount, {0, 0, 0});
}

void AuroraRenderer::render(const AuroraRenderContext &context) {
  if (!context.logicalStripPowerPresent) {
    deactivateAurora();
    renderBlack(frame_, Aurora::LedCount);
    return;
  }

  if (context.transition != TransitionEffect::None) {
    deactivateAurora();
    renderTransition(frame_, Aurora::LedCount, context.transition,
                     context.transitionStartedAtMs,
                     context.transitionElapsedMs,
                     context.transitionDurationMs,
                     config_.transition);
  } else if (context.pcState == PcState::Running ||
             context.pcState == PcState::Starting) {
    renderAurora(context.hddActivity, context.nowMs);
  } else if (context.pcState == PcState::Sleeping) {
    deactivateAurora();
    renderSleep(frame_, Aurora::LedCount, config_.sleep, context.nowMs);
  } else {
    deactivateAurora();
    renderBlack(frame_, Aurora::LedCount);
  }
}

void AuroraRenderer::deactivateAurora() { auroraActive_ = false; }

void AuroraRenderer::renderAurora(uint8_t hddActivity, uint32_t nowMs) {
  uint32_t elapsedMs = 0;
  if (!auroraActive_) {
    aurora_.reset(seed_);
    auroraActive_ = true;
  } else {
    elapsedMs = nowMs - lastAuroraUpdateMs_;
  }
  lastAuroraUpdateMs_ = nowMs;
  aurora_.advance(elapsedMs, hddActivity);

  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    frame_[index] = aurora_.pixel(index);
  }
}
