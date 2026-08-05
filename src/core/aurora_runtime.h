#pragma once

#include <stdint.h>

#include "../effect_controller.h"
#include "../hdd_activity.h"
#include "../pc_state.h"
#include "../power_led_tracker.h"
#include "renderer.h"
#include "rgb8.h"

struct AuroraInputFrame {
  bool powerLed = false;
  bool hddLed = false;
  uint8_t hddActiveEdges = 0;

  bool powerButton = false;
  bool powerButtonPressed = false;
  bool powerButtonReleased = false;

  bool resetButton = false;
  bool resetButtonPressed = false;
  bool resetButtonReleased = false;

  bool debugButton = false;
  bool debugButtonPressed = false;

  bool stripPowerPresent = false;
};

struct AuroraSnapshot {
  PcState pcState = PcState::Off;
  PcState previousPcState = PcState::Off;
  TransitionEffect transition = TransitionEffect::None;
  TransitionEffect previousTransition = TransitionEffect::None;
  PowerLedMode powerLedMode = PowerLedMode::Off;

  uint8_t hddActivity = 0;
  uint8_t transitionProgress = 0;

  bool stateChanged = false;
  bool transitionChanged = false;
  bool frameUpdated = false;
  bool forcedShutdownLatched = false;

  uint32_t stateStartedAtMs = 0;
  uint32_t transitionStartedAtMs = 0;
  uint32_t transitionElapsedMs = 0;
  uint32_t transitionDurationMs = 0;
  uint32_t powerHoldElapsedMs = 0;
};

struct AuroraRuntimeConfig {
  uint32_t frameIntervalMs;
  PowerLedTrackerConfig powerLed;
  HddActivityConfig hdd;
  PcStateConfig pcState;
  EffectControllerConfig effects;
  Aurora::FieldConfig auroraField;
  AuroraRendererConfig renderer;
};

enum class AuroraConfigError : uint8_t {
  None,
  FrameIntervalZero,
  HddUpdateIntervalZero,
  AuroraFixedStepZero,
  AuroraSpawnTickRange,
  AuroraSpawnCountRange,
  AuroraSpawnCountExceedsLedCount,
  AuroraFadePeriodZero,
  AuroraDiffusionKernelSumZero,
  AuroraDiffusionKernelSumMismatch,
  SleepIntervalZero,
  SleepBrightnessDivisorZero,
  ShutdownOriginRange,
  ForcedFlashAfterForcedHold,
};

AuroraConfigError validateAuroraRuntimeConfig(
    const AuroraRuntimeConfig &config);

class AuroraRuntime {
 public:
  explicit AuroraRuntime(const AuroraRuntimeConfig &config);

  void reset(uint32_t seed, uint32_t nowMs = 0);

  // Applies one normalized input snapshot at one explicit uint32_t timestamp.
  // Callers must supply input edges in regular logical substeps; a large time
  // jump cannot reconstruct edges that were never observed. Elapsed-time
  // arithmetic is intentionally unsigned and wraparound-safe.
  void step(const AuroraInputFrame &inputs, uint32_t nowMs);

  // Platform adapters may request that the next step calculate a fresh frame,
  // for example after externally confirmed physical strip-power restoration.
  void requestFrameUpdate() { renderPending_ = true; }

  const AuroraSnapshot &snapshot() const { return snapshot_; }
  const Aurora::Rgb8 *ledFrame() const { return renderer_.frame(); }
  uint8_t ledCount() const { return renderer_.ledCount(); }
  AuroraConfigError configError() const { return configError_; }
  bool configValid() const {
    return configError_ == AuroraConfigError::None;
  }

 private:
  uint32_t transitionDuration(TransitionEffect transition) const;
  void updateSnapshot(PcState stateBeforeStep,
                      TransitionEffect transitionBeforeStep,
                      uint32_t nowMs);
  AuroraRenderContext renderContext(const AuroraInputFrame &inputs,
                                    uint32_t nowMs) const;

  PowerLedTracker powerLed_;
  HddActivity hdd_;
  PcStateMachine pc_;
  EffectController effects_;
  AuroraRenderer renderer_;
  AuroraSnapshot snapshot_;
  AuroraConfigError configError_;
  uint32_t frameIntervalMs_;
  uint32_t lastHddUpdateMs_;
  uint32_t lastFrameMs_;
  uint8_t pendingHddEdges_;
  bool renderPending_;
  bool lastLogicalStripPowerPresent_;
};
