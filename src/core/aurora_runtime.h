#pragma once

#include <stdint.h>

#include "../effects/aurora_field.h"
#include "../hdd_activity.h"
#include "../pc_state.h"
#include "../power_led_tracker.h"
#include "geometry.h"
#include "rgb8.h"

struct AuroraInputFrame {
  SignalState powerLed = SignalState::Low;

  SignalState hddLed = SignalState::Low;
  // Signed normalized activity delta in Q8.8 units for the elapsed interval.
  int16_t hddContribution = 0;

  SignalState powerButton = SignalState::Low;
  SignalState resetButton = SignalState::Low;

  bool stripPowerPresent = false;
};

struct AuroraSnapshot {
  PcState pcState = PcState::Off;
  PcState previousPcState = PcState::Off;
  AnimationMode animation = AnimationMode::Off;
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

class AuroraRuntime {
 public:
  AuroraRuntime();

  void reset(uint32_t seed, uint32_t nowMs = 0);

  // Applies one normalized input snapshot at one explicit uint32_t timestamp.
  // The frame carries current signal levels and any transitions observed since
  // the previous call. Elapsed-time behavior stays inside the core; callers do
  // not need to use a fixed tick. Time arithmetic is unsigned and wraparound-
  // safe.
  void step(const AuroraInputFrame &inputs, uint32_t nowMs);

  // Platform adapters may request that the next step calculate a fresh frame,
  // for example after externally confirmed physical strip-power restoration.
  void requestFrameUpdate() { renderPending_ = true; }

  const AuroraSnapshot &snapshot() const { return snapshot_; }
  const Aurora::Rgb8 *ledFrame() const { return frame_; }
  uint8_t ledCount() const { return Aurora::LedCount; }
  Aurora::FieldCellDiagnostics auroraDiagnostics(uint8_t index) const {
    return field_.diagnostics(index);
  }

#ifdef PIO_UNIT_TESTING
  uint32_t auroraPrngStateForTest() const { return field_.prngState(); }
#endif

 private:
  static bool animationAdvancesField(AnimationMode mode);
  static TransitionEffect transitionFor(AnimationMode mode);
  static uint32_t transitionDuration(TransitionEffect transition);

  bool currentAnimationFinished(uint32_t nowMs) const;
  AnimationMode selectAnimation(const AuroraInputFrame &inputs,
                                bool currentAnimationFinished,
                                bool resetRequested, uint32_t nowMs) const;
  void enterAnimation(AnimationMode mode, uint32_t startedAtMs,
                      uint32_t nowMs, bool restart = false);
  void updateAnimation(const AuroraInputFrame &inputs, uint32_t nowMs);
  void updateStartupField(uint32_t elapsedMs, uint32_t nowMs);
  void updateResetField(uint32_t elapsedMs, uint32_t nowMs);
  void renderAnimation(uint32_t nowMs);
  void renderField();
  void renderStartup(uint32_t nowMs);
  void renderReset(uint32_t nowMs);
  void renderShutdown(uint32_t nowMs);
  void renderForcedShutdown(uint32_t nowMs);
  void updateSnapshot(PcState stateBeforeStep,
                      TransitionEffect transitionBeforeStep,
                      uint32_t nowMs);
  uint8_t animationProgress(uint32_t nowMs) const;

  PowerLedTracker powerLed_;
  HddActivity hdd_;
  PcStateMachine pc_;
  Aurora::Field field_;
  Aurora::Rgb8 frame_[Aurora::LedCount];
  AuroraSnapshot snapshot_;
  AnimationMode animationMode_;
  uint32_t animationStartedAtMs_;
  uint32_t lastHddUpdateMs_;
  uint32_t lastFrameMs_;
  uint32_t lastFieldUpdateMs_;
  uint8_t animationOrigin_;
  bool startupAnimationFinished_;
  bool shutdownAnimationFinished_;
  bool renderPending_;
  bool lastLogicalStripPowerPresent_;
};
