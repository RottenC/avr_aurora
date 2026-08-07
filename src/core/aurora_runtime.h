#pragma once

#include <stdint.h>

#include "../effect_controller.h"
#include "../hdd_activity.h"
#include "../pc_state.h"
#include "../power_led_tracker.h"
#include "renderer.h"
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
  const Aurora::Rgb8 *ledFrame() const { return renderer_.frame(); }
  uint8_t ledCount() const { return renderer_.ledCount(); }
  Aurora::FieldCellDiagnostics auroraDiagnostics(uint8_t index) const {
    return renderer_.auroraDiagnostics(index);
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
  uint32_t lastHddUpdateMs_;
  uint32_t lastFrameMs_;
  bool renderPending_;
  bool lastLogicalStripPowerPresent_;
};
