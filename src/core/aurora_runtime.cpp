#include "aurora_runtime.h"

#include "../config.h"
#include "../effects/effects.h"
#include "portable_math.h"

namespace {

uint8_t distanceBetween(uint8_t left, uint8_t right) {
  return left > right ? static_cast<uint8_t>(left - right)
                      : static_cast<uint8_t>(right - left);
}

uint8_t maximumDistanceFrom(uint8_t origin) {
  const uint8_t toLeft = origin;
  const uint8_t toRight =
      static_cast<uint8_t>(Aurora::LedCount - 1U - origin);
  return toLeft > toRight ? toLeft : toRight;
}

uint8_t phaseProgress(uint8_t value, uint8_t begin, uint8_t end) {
  if (value <= begin) return 0;
  if (value >= end || end <= begin) return UINT8_MAX;
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(value - begin) * UINT8_MAX) / (end - begin));
}

uint8_t radiusForProgress(uint8_t progress, uint8_t progressAtFullRadius,
                          uint8_t maximumRadius) {
  if (progress >= progressAtFullRadius) return maximumRadius;
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(maximumRadius) * progress) /
      progressAtFullRadius);
}

uint8_t progressForDistance(uint8_t distance, uint8_t maximumDistance,
                            uint8_t progressAtMaximumDistance) {
  if (distance == 0 || maximumDistance == 0) return 0;
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(distance) * progressAtMaximumDistance +
       maximumDistance - 1U) /
      maximumDistance);
}

Aurora::Rgb8 blendRgb(const Aurora::Rgb8 &from, const Aurora::Rgb8 &to,
                      uint8_t amount) {
  return {Aurora::lerp8(from.r, to.r, amount),
          Aurora::lerp8(from.g, to.g, amount),
          Aurora::lerp8(from.b, to.b, amount)};
}

Aurora::Rgb8 rgbFromPacked(uint32_t packed) {
  return {static_cast<uint8_t>(packed >> 16),
          static_cast<uint8_t>(packed >> 8),
          static_cast<uint8_t>(packed)};
}

Aurora::Rgb8 scaleRgb(const Aurora::Rgb8 &color, uint8_t brightness) {
  return {Aurora::scale8(color.r, brightness),
          Aurora::scale8(color.g, brightness),
          Aurora::scale8(color.b, brightness)};
}

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

AuroraRuntime::AuroraRuntime()
    : animationMode_(AnimationMode::Off),
      animationStartedAtMs_(0),
      lastHddUpdateMs_(0),
      lastFrameMs_(0),
      lastFieldUpdateMs_(0),
      animationOrigin_(Config::ShutdownOriginMin),
      startupAnimationFinished_(false),
      shutdownAnimationFinished_(false),
      renderPending_(true),
      lastLogicalStripPowerPresent_(false) {
  reset(Config::AuroraZeroSeedFallback, 0);
}

void AuroraRuntime::reset(uint32_t seed, uint32_t nowMs) {
  powerLed_.reset();
  hdd_.reset();
  pc_.reset();
  field_.reset(seed);
  Aurora::fillRgb(frame_, Aurora::LedCount, {0, 0, 0});

  snapshot_ = {};
  snapshot_.stateStartedAtMs = nowMs;
  animationMode_ = AnimationMode::Off;
  animationStartedAtMs_ = nowMs;
  lastHddUpdateMs_ = nowMs;
  lastFrameMs_ = nowMs;
  lastFieldUpdateMs_ = nowMs;
  animationOrigin_ = Config::ShutdownOriginMin;
  startupAnimationFinished_ = false;
  shutdownAnimationFinished_ = false;
  renderPending_ = true;
  lastLogicalStripPowerPresent_ = false;
}

void AuroraRuntime::step(const AuroraInputFrame &inputs, uint32_t nowMs) {
  snapshot_.stateChanged = false;
  snapshot_.transitionChanged = false;
  snapshot_.frameUpdated = false;
  if (inputs.stripPowerPresent != lastLogicalStripPowerPresent_) {
    lastLogicalStripPowerPresent_ = inputs.stripPowerPresent;
    lastFieldUpdateMs_ = nowMs;
    renderPending_ = true;
  }

  powerLed_.update(inputs.powerLed, nowMs);
  const PowerLedMode powerMode = powerLed_.mode(nowMs);

  const uint32_t hddElapsedMs = nowMs - lastHddUpdateMs_;
  lastHddUpdateMs_ = nowMs;
  hdd_.update(inputs.hddLed, inputs.hddContribution, hddElapsedMs);

  const PcState stateBeforeStep = pc_.state();
  const TransitionEffect transitionBeforeStep = transitionFor(animationMode_);
  const bool animationFinished = currentAnimationFinished(nowMs);

  if (stateBeforeStep != PcState::Starting) {
    startupAnimationFinished_ = false;
  }
  if (stateBeforeStep != PcState::AwaitShutdown) {
    shutdownAnimationFinished_ = false;
  }
  if (animationFinished) {
    if (animationMode_ == AnimationMode::Startup) {
      startupAnimationFinished_ = true;
    } else if (animationMode_ == AnimationMode::Shutdown) {
      shutdownAnimationFinished_ = true;
    }
  }
  if (stateBeforeStep == PcState::Starting &&
      !inputs.stripPowerPresent) {
    startupAnimationFinished_ = false;
  }

  const PcStateInputs pcInputs = {
      signalIsHigh(inputs.powerButton),
      signalIsRising(inputs.powerButton),
      signalIsFalling(inputs.powerButton),
      signalIsRising(inputs.resetButton),
      powerMode,
      startupAnimationFinished_,
  };
  const PcStateEvents events = pc_.update(pcInputs, nowMs);

  if (stateBeforeStep != PcState::AwaitShutdown &&
      pc_.state() == PcState::AwaitShutdown) {
    shutdownAnimationFinished_ = false;
  }

  const AnimationMode desired =
      selectAnimation(inputs, animationFinished, events.requestReset, nowMs);
  const bool restart = desired == AnimationMode::Reset &&
                       events.requestReset &&
                       !pc_.trackingPowerHold();
  const uint32_t desiredStartedAt =
      desired == AnimationMode::ForcedShutdown
          ? pc_.powerHoldStartMs() + Config::ForcedShutdownDelayMs
          : nowMs;
  enterAnimation(desired, desiredStartedAt, nowMs, restart);

  if (pc_.state() != PcState::Starting) {
    startupAnimationFinished_ = false;
  }
  if (pc_.state() != PcState::AwaitShutdown) {
    shutdownAnimationFinished_ = false;
  }

  snapshot_.powerLedMode = powerMode;
  updateSnapshot(stateBeforeStep, transitionBeforeStep, nowMs);

  if (renderPending_ || nowMs - lastFrameMs_ >= Config::FrameIntervalMs) {
    renderPending_ = false;
    lastFrameMs_ = nowMs;
    updateAnimation(inputs, nowMs);
    snapshot_.frameUpdated = true;
  }
}

bool AuroraRuntime::animationAdvancesField(AnimationMode mode) {
  return mode == AnimationMode::Startup || mode == AnimationMode::Ambient ||
         mode == AnimationMode::Reset;
}

TransitionEffect AuroraRuntime::transitionFor(AnimationMode mode) {
  switch (mode) {
    case AnimationMode::Startup:
      return TransitionEffect::Startup;
    case AnimationMode::Reset:
      return TransitionEffect::Reset;
    case AnimationMode::Shutdown:
      return TransitionEffect::Shutdown;
    case AnimationMode::ForcedShutdown:
      return TransitionEffect::ForcedShutdown;
    case AnimationMode::Off:
    case AnimationMode::Ambient:
    case AnimationMode::Sleep:
      return TransitionEffect::None;
  }
  return TransitionEffect::None;
}

uint32_t AuroraRuntime::transitionDuration(TransitionEffect transition) {
  switch (transition) {
    case TransitionEffect::Startup:
      return Config::StartupDurationMs;
    case TransitionEffect::Shutdown:
      return Config::ShutdownDurationMs;
    case TransitionEffect::Reset:
      return Config::ResetDurationMs;
    case TransitionEffect::ForcedShutdown:
      return Config::PowerHoldForcedMs - Config::ForcedShutdownDelayMs;
    case TransitionEffect::None:
      return 0;
  }
  return 0;
}

bool AuroraRuntime::currentAnimationFinished(uint32_t nowMs) const {
  const TransitionEffect transition = transitionFor(animationMode_);
  if (transition == TransitionEffect::None ||
      transition == TransitionEffect::ForcedShutdown) {
    return false;
  }
  return nowMs - animationStartedAtMs_ >= transitionDuration(transition);
}

AnimationMode AuroraRuntime::selectAnimation(
    const AuroraInputFrame &inputs, bool currentAnimationFinished,
    bool resetRequested, uint32_t nowMs) const {
  switch (pc_.state()) {
    case PcState::Off:
      return AnimationMode::Off;

    case PcState::Starting:
      if (!inputs.stripPowerPresent) return AnimationMode::Off;
      return startupAnimationFinished_ ? AnimationMode::Ambient
                                       : AnimationMode::Startup;

    case PcState::Running:
      if (pc_.trackingPowerHold() &&
          pc_.powerHoldElapsed(nowMs) >= Config::ForcedShutdownDelayMs) {
        return AnimationMode::ForcedShutdown;
      }
      if (resetRequested) return AnimationMode::Reset;
      if (animationMode_ == AnimationMode::Reset &&
          !currentAnimationFinished) {
        return AnimationMode::Reset;
      }
      return AnimationMode::Ambient;

    case PcState::Sleeping:
      return AnimationMode::Sleep;

    case PcState::AwaitShutdown:
      if (pc_.forcedLatched()) return AnimationMode::ForcedShutdown;
      return shutdownAnimationFinished_ ? AnimationMode::Off
                                        : AnimationMode::Shutdown;
  }
  return AnimationMode::Off;
}

void AuroraRuntime::enterAnimation(AnimationMode mode, uint32_t startedAtMs,
                                   uint32_t nowMs, bool restart) {
  if (animationMode_ == mode && !restart) return;

  const bool previousAdvancedField = animationAdvancesField(animationMode_);
  animationMode_ = mode;
  animationStartedAtMs_ = startedAtMs;
  if (!previousAdvancedField && animationAdvancesField(mode)) {
    lastFieldUpdateMs_ = nowMs;
  }

  if (mode == AnimationMode::Startup || mode == AnimationMode::Reset ||
      mode == AnimationMode::Shutdown) {
    const uint8_t span = static_cast<uint8_t>(
        Config::ShutdownOriginMax - Config::ShutdownOriginMin + 1U);
    animationOrigin_ = static_cast<uint8_t>(
        Config::ShutdownOriginMin + ((startedAtMs >> 2) % span));
  }
  if (mode == AnimationMode::Startup) startupAnimationFinished_ = false;
  if (mode == AnimationMode::Shutdown) shutdownAnimationFinished_ = false;
  renderPending_ = true;
}

void AuroraRuntime::updateAnimation(const AuroraInputFrame &inputs,
                                    uint32_t nowMs) {
  if (!inputs.stripPowerPresent) {
    lastFieldUpdateMs_ = nowMs;
    renderBlack(frame_, Aurora::LedCount);
    return;
  }

  const uint32_t fieldElapsedMs = nowMs - lastFieldUpdateMs_;
  switch (animationMode_) {
    case AnimationMode::Startup:
      updateStartupField(fieldElapsedMs, nowMs);
      break;
    case AnimationMode::Ambient:
      field_.advance(fieldElapsedMs, hdd_.value(), nowMs);
      break;
    case AnimationMode::Reset:
      updateResetField(fieldElapsedMs, nowMs);
      break;
    case AnimationMode::Off:
    case AnimationMode::Shutdown:
    case AnimationMode::ForcedShutdown:
    case AnimationMode::Sleep:
      break;
  }
  lastFieldUpdateMs_ = nowMs;
  renderAnimation(nowMs);
}

void AuroraRuntime::updateStartupField(uint32_t elapsedMs, uint32_t nowMs) {
  field_.advance(elapsedMs, 0, nowMs);

  const uint8_t progress = animationProgress(nowMs);
  const uint8_t maximumRadius = maximumDistanceFrom(animationOrigin_);
  const uint8_t radius = radiusForProgress(
      progress, Config::StartupSpreadEndProgress, maximumRadius);

  uint8_t brightness = Config::StartupInitialBrightness;
  if (progress <= Config::StartupSpreadEndProgress) {
    brightness = Aurora::lerp8(
        Config::StartupInitialBrightness, Config::StartupSpreadBrightness,
        phaseProgress(progress, 0, Config::StartupSpreadEndProgress));
  } else if (progress <= Config::StartupFlashEndProgress) {
    brightness = Aurora::lerp8(
        Config::StartupSpreadBrightness, Config::StartupFlashBrightness,
        phaseProgress(progress, Config::StartupSpreadEndProgress,
                      Config::StartupFlashEndProgress));
  } else {
    brightness = Aurora::lerp8(
        Config::StartupFlashBrightness, Config::StartupSettleBrightness,
        phaseProgress(progress, Config::StartupFlashEndProgress,
                      UINT8_MAX));
  }

  const uint8_t colorProgressCeiling = Aurora::lerp8(
      Config::StartupInitialColorProgressCeiling, UINT8_MAX, progress);
  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    if (distanceBetween(index, animationOrigin_) <= radius) {
      field_.exciteCell(index, brightness, colorProgressCeiling);
    }
  }
}

void AuroraRuntime::updateResetField(uint32_t elapsedMs, uint32_t nowMs) {
  field_.advance(elapsedMs, 0, nowMs);

  const uint8_t radius = radiusForProgress(
      animationProgress(nowMs), UINT8_MAX,
      maximumDistanceFrom(animationOrigin_));
  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    if (distanceBetween(index, animationOrigin_) == radius) {
      field_.exciteCell(index, Config::ResetWaveBrightness,
                        Config::ResetWaveColorProgressCeiling);
    }
  }
}

void AuroraRuntime::renderAnimation(uint32_t nowMs) {
  switch (animationMode_) {
    case AnimationMode::Off:
      renderBlack(frame_, Aurora::LedCount);
      break;
    case AnimationMode::Startup:
      renderStartup(nowMs);
      break;
    case AnimationMode::Ambient:
      renderField();
      break;
    case AnimationMode::Reset:
      renderReset(nowMs);
      break;
    case AnimationMode::Shutdown:
      renderShutdown(nowMs);
      break;
    case AnimationMode::ForcedShutdown:
      renderForcedShutdown(nowMs);
      break;
    case AnimationMode::Sleep:
      renderSleep(frame_, Aurora::LedCount, nowMs);
      break;
  }
}

void AuroraRuntime::renderField() {
  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    frame_[index] = field_.pixel(index);
  }
}

void AuroraRuntime::renderStartup(uint32_t nowMs) {
  const uint8_t radius = radiusForProgress(
      animationProgress(nowMs), Config::StartupSpreadEndProgress,
      maximumDistanceFrom(animationOrigin_));
  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    frame_[index] = distanceBetween(index, animationOrigin_) <= radius
                        ? field_.pixel(index)
                        : Aurora::Rgb8{};
  }
}

void AuroraRuntime::renderReset(uint32_t nowMs) {
  const uint8_t progress = animationProgress(nowMs);
  const uint8_t radius = radiusForProgress(
      progress, UINT8_MAX, maximumDistanceFrom(animationOrigin_));
  const uint8_t tint =
      progress < Config::ResetTintFadeStartProgress
          ? UINT8_MAX
          : Aurora::lerp8(
                UINT8_MAX, 0,
                phaseProgress(progress, Config::ResetTintFadeStartProgress,
                              UINT8_MAX));

  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    const Aurora::Rgb8 fieldColor = field_.pixel(index);
    if (distanceBetween(index, animationOrigin_) > radius) {
      frame_[index] = fieldColor;
      continue;
    }
    const uint8_t brightness = field_.outputBrightness(index);
    const Aurora::Rgb8 resetColor = {
        Aurora::scale8(Config::ResetColor.r, brightness),
        Aurora::scale8(Config::ResetColor.g, brightness),
        Aurora::scale8(Config::ResetColor.b, brightness),
    };
    frame_[index] = blendRgb(fieldColor, resetColor, tint);
  }
}

void AuroraRuntime::renderShutdown(uint32_t nowMs) {
  const uint8_t progress = animationProgress(nowMs);
  const uint8_t maximumDistance = maximumDistanceFrom(animationOrigin_);
  const uint8_t radius = radiusForProgress(
      progress, Config::ShutdownWaveTravelEndProgress, maximumDistance);
  const uint8_t fadeProgressSpan = static_cast<uint8_t>(
      UINT8_MAX - Config::ShutdownWaveTravelEndProgress);

  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    const uint8_t distance = distanceBetween(index, animationOrigin_);
    if (distance > radius) {
      frame_[index] = field_.pixel(index);
      continue;
    }

    const uint8_t arrivalProgress = progressForDistance(
        distance, maximumDistance,
        Config::ShutdownWaveTravelEndProgress);
    const uint8_t fadeEndProgress = static_cast<uint8_t>(
        arrivalProgress + fadeProgressSpan);
    const uint8_t brightness = static_cast<uint8_t>(
        UINT8_MAX -
        phaseProgress(progress, arrivalProgress, fadeEndProgress));
    frame_[index] = {
        Aurora::scale8(Config::ShutdownColor.r, brightness),
        Aurora::scale8(Config::ShutdownColor.g, brightness),
        Aurora::scale8(Config::ShutdownColor.b, brightness),
    };
  }
}

void AuroraRuntime::renderForcedShutdown(uint32_t nowMs) {
  const uint32_t animationElapsedMs = nowMs - animationStartedAtMs_;
  const uint32_t holdElapsedMs =
      animationElapsedMs + Config::ForcedShutdownDelayMs;
  const uint32_t boundedElapsed = holdElapsedMs < Config::PowerHoldForcedMs
                                      ? holdElapsedMs
                                      : Config::PowerHoldForcedMs;
  const uint8_t brightness =
      boundedElapsed < Config::ForcedFlashAtMs
          ? interpolatedU8(boundedElapsed, Config::ForcedShutdownDelayMs,
                           Config::ForcedFlashAtMs,
                           Config::ForcedInitialBrightness,
                           Config::ForcedFlashBrightness)
          : interpolatedU8(boundedElapsed, Config::ForcedFlashAtMs,
                           Config::PowerHoldForcedMs,
                           Config::ForcedFlashBrightness, 0);
  Aurora::fillRgb(frame_, Aurora::LedCount,
                  scaleRgb(rgbFromPacked(Config::AuroraColor2Rgb),
                           brightness));
}

void AuroraRuntime::updateSnapshot(
    PcState stateBeforeStep, TransitionEffect transitionBeforeStep,
    uint32_t nowMs) {
  const PcState currentState = pc_.state();
  snapshot_.stateChanged = currentState != stateBeforeStep;
  if (snapshot_.stateChanged) {
    snapshot_.previousPcState = stateBeforeStep;
    snapshot_.stateStartedAtMs = nowMs;
  }

  const TransitionEffect currentTransition = transitionFor(animationMode_);
  snapshot_.transitionChanged = currentTransition != transitionBeforeStep;
  if (snapshot_.transitionChanged) {
    snapshot_.previousTransition = transitionBeforeStep;
  }

  snapshot_.pcState = currentState;
  snapshot_.animation = animationMode_;
  snapshot_.transition = currentTransition;
  snapshot_.hddActivity = hdd_.value();
  snapshot_.forcedShutdownLatched = pc_.forcedLatched();
  snapshot_.powerHoldElapsedMs = pc_.powerHoldElapsed(nowMs);

  if (currentTransition == TransitionEffect::None) {
    snapshot_.transitionStartedAtMs = 0;
    snapshot_.transitionElapsedMs = 0;
    snapshot_.transitionDurationMs = 0;
    snapshot_.transitionProgress = 0;
    return;
  }

  snapshot_.transitionStartedAtMs = animationStartedAtMs_;
  snapshot_.transitionElapsedMs = nowMs - animationStartedAtMs_;
  snapshot_.transitionDurationMs = transitionDuration(currentTransition);
  snapshot_.transitionProgress = animationProgress(nowMs);
}

uint8_t AuroraRuntime::animationProgress(uint32_t nowMs) const {
  const uint32_t duration = transitionDuration(transitionFor(animationMode_));
  const uint32_t elapsed = nowMs - animationStartedAtMs_;
  if (duration == 0 || elapsed >= duration) return UINT8_MAX;
  return static_cast<uint8_t>((elapsed * UINT8_MAX) / duration);
}
