#include "aurora_runtime.h"

AuroraRuntime::AuroraRuntime(const AuroraRuntimeConfig &config)
    : powerLed_(config.powerLed),
      hdd_(config.hdd),
      pc_(config.pcState),
      effects_(config.effects),
      renderer_(config.renderer, config.effects, config.pcState,
                config.auroraField),
      lastHddUpdateMs_(0),
      lastFrameMs_(0),
      pendingHddEdges_(0),
      renderPending_(true) {
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
  pendingHddEdges_ = 0;
  renderPending_ = true;
}

void AuroraRuntime::step(const AuroraInputFrame &inputs, uint32_t nowMs) {
  const uint16_t edgeTotal =
      static_cast<uint16_t>(pendingHddEdges_) + inputs.hddActiveEdges;
  pendingHddEdges_ =
      static_cast<uint8_t>(edgeTotal > UINT8_MAX ? UINT8_MAX : edgeTotal);

  powerLed_.update(inputs.powerLed, nowMs);
  const PowerLedMode powerMode = powerLed_.mode(nowMs);

  if (nowMs - lastHddUpdateMs_ >= hdd_.updateIntervalMs()) {
    const uint32_t elapsedMs = nowMs - lastHddUpdateMs_;
    lastHddUpdateMs_ = nowMs;
    hdd_.update(inputs.hddLed, pendingHddEdges_, elapsedMs);
    pendingHddEdges_ = 0;
  }

  effects_.update(nowMs);
  const TransitionEffect finishedEffect = effects_.consumeFinished();
  const PcState stateBeforeStep = pc_.state();
  const PcStateInputs pcInputs = {
      inputs.stripPowerPresent,
      inputs.powerButton,
      inputs.powerButtonPressed,
      inputs.powerButtonReleased,
      inputs.resetButtonPressed,
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

  snapshot_.frameChanged = false;
  if (renderPending_ ||
      nowMs - lastFrameMs_ >= renderer_.frameIntervalMs()) {
    renderPending_ = false;
    lastFrameMs_ = nowMs;
    renderer_.render(pc_.state(), effects_.current(), effects_.startedAt(),
                     hdd_.value(), inputs.stripPowerPresent, nowMs);
    snapshot_.frameChanged = true;
  }

  snapshot_.powerLedMode = powerMode;
  updateSnapshot(stateBeforeStep, nowMs);
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

void AuroraRuntime::updateSnapshot(PcState stateBeforeStep, uint32_t nowMs) {
  const PcState currentState = pc_.state();
  snapshot_.stateChanged = currentState != stateBeforeStep;
  if (snapshot_.stateChanged) {
    snapshot_.previousPcState = stateBeforeStep;
    snapshot_.stateStartedAtMs = nowMs;
  }

  snapshot_.pcState = currentState;
  snapshot_.transition = effects_.current();
  snapshot_.hddActivity = hdd_.value();
  snapshot_.forcedShutdownLatched = pc_.forcedLatched();
  snapshot_.powerHoldElapsedMs = pc_.powerHoldElapsed(nowMs);

  if (snapshot_.transition == TransitionEffect::None) {
    snapshot_.transitionStartedAtMs = 0;
    snapshot_.transitionElapsedMs = 0;
    snapshot_.transitionProgress = 0;
    return;
  }

  snapshot_.transitionStartedAtMs = effects_.startedAt();
  snapshot_.transitionElapsedMs = nowMs - effects_.startedAt();
  const uint32_t duration = transitionDuration(snapshot_.transition);
  if (duration == 0 || snapshot_.transitionElapsedMs >= duration) {
    snapshot_.transitionProgress = UINT8_MAX;
  } else {
    snapshot_.transitionProgress = static_cast<uint8_t>(
        (snapshot_.transitionElapsedMs * UINT8_MAX) / duration);
  }
}
