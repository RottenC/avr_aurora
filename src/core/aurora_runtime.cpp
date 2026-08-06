#include "aurora_runtime.h"

namespace {

Aurora::FieldConfig safeFieldConfig(const AuroraRuntimeConfig &runtimeConfig) {
  Aurora::FieldConfig config = runtimeConfig.auroraField;

  if (config.fixedStepMs == 0) config.fixedStepMs = 1;
  if (config.spawnMinTicks == 0 ||
      config.spawnMinTicks > config.spawnMaxTicks) {
    config.spawnMinTicks = 1;
    config.spawnMaxTicks = 1;
  }
  if (config.spawnMinCount == 0 ||
      config.spawnMinCount > config.spawnMaxCount ||
      config.spawnMaxCount > Aurora::LedCount) {
    config.spawnMinCount = 1;
    config.spawnMaxCount = 1;
  }
  if (config.ticksPerFade == 0) config.ticksPerFade = 1;
  if (config.hddActivityMaximum == 0) config.hddActivityMaximum = 1;

  const uint16_t calculatedKernelSum =
      static_cast<uint16_t>(config.diffusionSideWeight) * 2U +
      config.diffusionCenterWeight;
  if (config.diffusionKernelSum == 0 ||
      calculatedKernelSum != config.diffusionKernelSum) {
    config.diffusionSideWeight = 0;
    config.diffusionCenterWeight = 1;
    config.diffusionKernelSum = 1;
  }
  return config;
}

}  // namespace

AuroraConfigError validateAuroraRuntimeConfig(
    const AuroraRuntimeConfig &config) {
  if (config.frameIntervalMs == 0) {
    return AuroraConfigError::FrameIntervalZero;
  }
  if (config.hdd.updateMs == 0) {
    return AuroraConfigError::HddUpdateIntervalZero;
  }
  if (config.auroraField.fixedStepMs == 0) {
    return AuroraConfigError::AuroraFixedStepZero;
  }
  if (config.auroraField.spawnMinTicks == 0 ||
      config.auroraField.spawnMinTicks >
          config.auroraField.spawnMaxTicks) {
    return AuroraConfigError::AuroraSpawnTickRange;
  }
  if (config.auroraField.spawnMinCount == 0 ||
      config.auroraField.spawnMinCount >
          config.auroraField.spawnMaxCount) {
    return AuroraConfigError::AuroraSpawnCountRange;
  }
  if (config.auroraField.spawnMaxCount > Aurora::LedCount) {
    return AuroraConfigError::AuroraSpawnCountExceedsLedCount;
  }
  if (config.auroraField.ticksPerFade == 0) {
    return AuroraConfigError::AuroraFadePeriodZero;
  }
  if (config.auroraField.hddActivityMaximum == 0) {
    return AuroraConfigError::AuroraHddActivityMaximumZero;
  }
  if (config.auroraField.diffusionKernelSum == 0) {
    return AuroraConfigError::AuroraDiffusionKernelSumZero;
  }
  const uint16_t calculatedKernelSum =
      static_cast<uint16_t>(config.auroraField.diffusionSideWeight) * 2U +
      config.auroraField.diffusionCenterWeight;
  if (calculatedKernelSum != config.auroraField.diffusionKernelSum) {
    return AuroraConfigError::AuroraDiffusionKernelSumMismatch;
  }
  if (config.renderer.sleep.travelIntervalMs == 0) {
    return AuroraConfigError::SleepIntervalZero;
  }
  if (config.renderer.sleep.secondaryBrightnessDivisor == 0) {
    return AuroraConfigError::SleepBrightnessDivisorZero;
  }
  if (config.renderer.transition.shutdownOriginMin >
          config.renderer.transition.shutdownOriginMax ||
      config.renderer.transition.shutdownOriginMax >= Aurora::LedCount) {
    return AuroraConfigError::ShutdownOriginRange;
  }
  if (config.renderer.transition.forcedFlashAtMs >
      config.pcState.forcedHoldMs) {
    return AuroraConfigError::ForcedFlashAfterForcedHold;
  }
  return AuroraConfigError::None;
}

AuroraRuntime::AuroraRuntime(const AuroraRuntimeConfig &config)
    : powerLed_(config.powerLed),
      hdd_(config.hdd),
      pc_(config.pcState),
      effects_(config.effects),
      renderer_(config.renderer, safeFieldConfig(config)),
      configError_(validateAuroraRuntimeConfig(config)),
      frameIntervalMs_(config.frameIntervalMs == 0 ? 1
                                                   : config.frameIntervalMs),
      lastHddUpdateMs_(0),
      lastFrameMs_(0),
      renderPending_(true),
      lastLogicalStripPowerPresent_(false) {
  reset(config.auroraField.zeroSeedFallback, 0);
}

void AuroraRuntime::reset(uint32_t seed, uint32_t nowMs) {
  powerLed_.reset();
  hdd_.reset();
  pc_.reset();
  effects_.reset();
  renderer_.reset(seed, nowMs);

  snapshot_ = {};
  snapshot_.stateStartedAtMs = nowMs;
  lastHddUpdateMs_ = nowMs;
  lastFrameMs_ = nowMs;
  renderPending_ = true;
  lastLogicalStripPowerPresent_ = false;
}

void AuroraRuntime::step(const AuroraInputFrame &inputs, uint32_t nowMs) {
  snapshot_.stateChanged = false;
  snapshot_.transitionChanged = false;
  snapshot_.frameUpdated = false;
  if (!configValid()) return;

  if (inputs.stripPowerPresent != lastLogicalStripPowerPresent_) {
    lastLogicalStripPowerPresent_ = inputs.stripPowerPresent;
    renderPending_ = true;
  }

  powerLed_.update(inputs.powerLed, nowMs);
  const PowerLedMode powerMode = powerLed_.mode(nowMs);

  const uint32_t hddElapsedMs = nowMs - lastHddUpdateMs_;
  lastHddUpdateMs_ = nowMs;
  hdd_.update(inputs.hddLed, inputs.hddContribution, hddElapsedMs);

  const PcState stateBeforeStep = pc_.state();
  const TransitionEffect transitionBeforeStep = effects_.current();

  effects_.update(nowMs);
  const TransitionEffect finishedEffect = effects_.consumeFinished();
  const PcStateInputs pcInputs = {
      inputs.stripPowerPresent,
      signalIsHigh(inputs.powerButton),
      signalIsRising(inputs.powerButton),
      signalIsFalling(inputs.powerButton),
      signalIsRising(inputs.resetButton),
      powerMode,
      finishedEffect == TransitionEffect::Startup,
  };
  const PcStateEvents events = pc_.update(pcInputs, nowMs);

  if (events.cancelStartup) effects_.cancel(TransitionEffect::Startup);
  if (events.cancelForcedShutdown) {
    effects_.cancel(TransitionEffect::ForcedShutdown);
  }
  if (events.requestStartup) {
    effects_.restart(TransitionEffect::Startup, nowMs);
  }
  if (events.requestShutdown) {
    effects_.request(TransitionEffect::Shutdown, nowMs);
  }
  if (events.requestReset) effects_.restart(TransitionEffect::Reset, nowMs);
  if (events.requestForcedShutdown) {
    effects_.request(TransitionEffect::ForcedShutdown,
                     pc_.powerHoldStartMs());
  }
  effects_.reconcile(pc_.state());

  snapshot_.powerLedMode = powerMode;
  updateSnapshot(stateBeforeStep, transitionBeforeStep, nowMs);

  if (renderPending_ || nowMs - lastFrameMs_ >= frameIntervalMs_) {
    renderPending_ = false;
    lastFrameMs_ = nowMs;
    renderer_.render(renderContext(inputs, nowMs));
    snapshot_.frameUpdated = true;
  }
}

uint32_t AuroraRuntime::transitionDuration(
    TransitionEffect transition) const {
  switch (transition) {
    case TransitionEffect::Startup:
    case TransitionEffect::Shutdown:
    case TransitionEffect::Reset:
      return effects_.durationFor(transition);
    case TransitionEffect::ForcedShutdown:
      return pc_.forcedHoldMs();
    case TransitionEffect::None:
      return 0;
  }
  return 0;
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

  const TransitionEffect currentTransition = effects_.current();
  snapshot_.transitionChanged =
      currentTransition != transitionBeforeStep;
  if (snapshot_.transitionChanged) {
    snapshot_.previousTransition = transitionBeforeStep;
  }

  snapshot_.pcState = currentState;
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

  snapshot_.transitionStartedAtMs = effects_.startedAt();
  snapshot_.transitionElapsedMs = nowMs - effects_.startedAt();
  snapshot_.transitionDurationMs = transitionDuration(currentTransition);
  if (snapshot_.transitionDurationMs == 0 ||
      snapshot_.transitionElapsedMs >= snapshot_.transitionDurationMs) {
    snapshot_.transitionProgress = UINT8_MAX;
  } else {
    snapshot_.transitionProgress = static_cast<uint8_t>(
        (snapshot_.transitionElapsedMs * UINT8_MAX) /
        snapshot_.transitionDurationMs);
  }
}

AuroraRenderContext AuroraRuntime::renderContext(
    const AuroraInputFrame &inputs, uint32_t nowMs) const {
  return {
      snapshot_.pcState,
      snapshot_.transition,
      nowMs,
      snapshot_.transitionStartedAtMs,
      snapshot_.transitionElapsedMs,
      snapshot_.transitionDurationMs,
      snapshot_.transitionProgress,
      snapshot_.hddActivity,
      inputs.stripPowerPresent,
  };
}
