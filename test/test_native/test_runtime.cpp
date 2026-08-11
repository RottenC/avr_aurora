#include <unity.h>

#include "config.h"
#include "core/aurora_runtime.h"
#include "core/portable_math.h"

namespace {

SignalState signalState(bool high) {
  return high ? SignalState::High : SignalState::Low;
}

AuroraInputFrame inputFrame(bool powerLed = false,
                            bool stripPowerPresent = true) {
  AuroraInputFrame input;
  input.powerLed = signalState(powerLed);
  input.stripPowerPresent = stripPowerPresent;
  return input;
}

uint32_t frameHash(const AuroraRuntime &runtime) {
  uint32_t hash = 0x811C9DC5UL;
  for (uint8_t index = 0; index < runtime.ledCount(); ++index) {
    const Aurora::Rgb8 &pixel = runtime.ledFrame()[index];
    hash = (hash ^ pixel.r) * 0x01000193UL;
    hash = (hash ^ pixel.g) * 0x01000193UL;
    hash = (hash ^ pixel.b) * 0x01000193UL;
  }
  return hash;
}

void assertState(const AuroraRuntime &runtime, PcState expected) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                          static_cast<uint8_t>(runtime.snapshot().pcState));
}

void assertTransition(const AuroraRuntime &runtime,
                      TransitionEffect expected) {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(expected),
      static_cast<uint8_t>(runtime.snapshot().transition));
}

void assertAnimation(const AuroraRuntime &runtime, AnimationMode expected) {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(expected),
      static_cast<uint8_t>(runtime.snapshot().animation));
}

void reconcileRuntimeToRunning(AuroraRuntime &runtime, uint32_t nowMs = 0) {
  AuroraInputFrame input = inputFrame(true);
  runtime.step(input, nowMs);
  assertState(runtime, PcState::Running);
}

void startPowerHold(AuroraRuntime &runtime, uint32_t nowMs) {
  AuroraInputFrame input = inputFrame(true);
  input.powerButton = SignalState::Rising;
  runtime.step(input, nowMs);
}

void releasePowerButton(AuroraRuntime &runtime, uint32_t nowMs) {
  AuroraInputFrame input = inputFrame(true);
  input.powerButton = SignalState::Falling;
  runtime.step(input, nowMs);
}

void test_runtime_01_cold_start_is_off_with_complete_black_frame() {
  AuroraRuntime runtime;
  runtime.reset(123, 10);
  AuroraInputFrame input = inputFrame(false, false);
  runtime.step(input, 10);

  assertState(runtime, PcState::Off);
  assertTransition(runtime, TransitionEffect::None);
  TEST_ASSERT_EQUAL_UINT8(Aurora::LedCount, runtime.ledCount());
  TEST_ASSERT_EQUAL_UINT8(0, runtime.snapshot().hddActivity);
  for (uint8_t index = 0; index < runtime.ledCount(); ++index) {
    TEST_ASSERT_TRUE(runtime.ledFrame()[index] == Aurora::Rgb8{});
  }
}

void test_runtime_02_power_button_starts_and_waits_for_strip_power() {
  AuroraRuntime runtime;
  runtime.reset(1);
  AuroraInputFrame input = inputFrame(false, false);
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);

  assertState(runtime, PcState::Starting);
  assertTransition(runtime, TransitionEffect::None);
  TEST_ASSERT_TRUE(runtime.snapshot().stateChanged);
  TEST_ASSERT_EQUAL_UINT32(100, runtime.snapshot().stateStartedAtMs);
}

void test_runtime_03_strip_power_loss_cancels_and_restarts_startup() {
  AuroraRuntime runtime;
  runtime.reset(1);
  AuroraInputFrame input = inputFrame();
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);
  assertTransition(runtime, TransitionEffect::Startup);
  const uint8_t startupOrigin = static_cast<uint8_t>(
      Config::ShutdownOriginMin +
      (((100U >> 2) % (Config::ShutdownOriginMax -
                       Config::ShutdownOriginMin + 1U))));
  TEST_ASSERT_TRUE(runtime.ledFrame()[startupOrigin] != Aurora::Rgb8{});

  input.powerButton = SignalState::High;
  input.stripPowerPresent = false;
  runtime.step(input, 200);
  assertState(runtime, PcState::Starting);
  assertTransition(runtime, TransitionEffect::None);

  input.stripPowerPresent = true;
  runtime.step(input, 300);
  assertTransition(runtime, TransitionEffect::Startup);
  TEST_ASSERT_EQUAL_UINT32(300,
                           runtime.snapshot().transitionStartedAtMs);
}

void test_runtime_04_startup_needs_visual_completion_and_power_confirmation() {
  AuroraRuntime runtime;
  runtime.reset(5);
  AuroraInputFrame input = inputFrame();
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);

  input.powerButton = SignalState::High;
  input.powerLed = SignalState::High;
  runtime.step(input, 100 + Config::StartupDurationMs - 1);
  assertState(runtime, PcState::Starting);
  assertTransition(runtime, TransitionEffect::Startup);

  runtime.step(input, 100 + Config::StartupDurationMs);
  assertState(runtime, PcState::Running);
  assertTransition(runtime, TransitionEffect::None);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting),
                          static_cast<uint8_t>(
                              runtime.snapshot().previousPcState));
}

void test_runtime_05_completed_startup_waits_for_late_power_confirmation() {
  AuroraRuntime runtime;
  runtime.reset(6);
  AuroraInputFrame input = inputFrame();
  input.powerButton = SignalState::Rising;
  runtime.step(input, 50);
  input.powerButton = SignalState::High;
  runtime.step(input, 50 + Config::StartupDurationMs);
  assertState(runtime, PcState::Starting);
  assertTransition(runtime, TransitionEffect::None);

  input.powerLed = SignalState::High;
  runtime.step(input, 50 + Config::StartupDurationMs + 100);
  assertState(runtime, PcState::Running);
}

void test_runtime_06_running_sleeps_and_wakes_from_classified_power_led() {
  AuroraRuntime runtime;
  runtime.reset(7);
  AuroraInputFrame input = inputFrame(true);
  runtime.step(input, 0);

  input.powerLed = SignalState::Falling;
  runtime.step(input, 200);
  input.powerLed = SignalState::Rising;
  runtime.step(input, 400);
  input.powerLed = SignalState::Falling;
  runtime.step(input, 600);
  input.powerLed = SignalState::Rising;
  runtime.step(input, 800);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PowerLedMode::Blinking),
                          static_cast<uint8_t>(
                              runtime.snapshot().powerLedMode));
  assertState(runtime, PcState::Sleeping);

  runtime.step(input, 800 + Config::PowerLedBlinkStaleMs + 1);
  assertState(runtime, PcState::Running);
}

void test_runtime_07_short_power_press_requests_normal_shutdown() {
  AuroraRuntime runtime;
  runtime.reset(8);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);
  TEST_ASSERT_EQUAL_UINT32(0, runtime.snapshot().powerHoldElapsedMs);
  assertAnimation(runtime, AnimationMode::Ambient);
  assertTransition(runtime, TransitionEffect::None);

  releasePowerButton(runtime, 500);
  assertState(runtime, PcState::AwaitShutdown);
  assertTransition(runtime, TransitionEffect::Shutdown);
  TEST_ASSERT_FALSE(runtime.snapshot().forcedShutdownLatched);
}

void test_runtime_08_forced_shutdown_release_boundaries() {
  const uint32_t heldMs[] = {Config::PowerHoldForcedMs - 1,
                             Config::PowerHoldForcedMs,
                             Config::PowerHoldForcedMs + 1};
  const bool expectedForced[] = {false, true, true};

  for (uint8_t index = 0; index < 3; ++index) {
    AuroraRuntime runtime;
    runtime.reset(9);
    reconcileRuntimeToRunning(runtime);
    startPowerHold(runtime, 100);
    releasePowerButton(runtime, 100 + heldMs[index]);
    assertState(runtime, PcState::AwaitShutdown);
    TEST_ASSERT_EQUAL(expectedForced[index],
                      runtime.snapshot().forcedShutdownLatched);
    assertTransition(
        runtime, expectedForced[index] ? TransitionEffect::ForcedShutdown
                                       : TransitionEffect::Shutdown);
  }
}

void test_runtime_09_forced_shutdown_latches_before_release() {
  AuroraRuntime runtime;
  runtime.reset(10);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);

  AuroraInputFrame input = inputFrame(true);
  input.powerButton = SignalState::High;
  runtime.step(input, 100 + Config::PowerHoldForcedMs);
  TEST_ASSERT_TRUE(runtime.snapshot().forcedShutdownLatched);
  TEST_ASSERT_EQUAL_UINT32(Config::PowerHoldForcedMs,
                           runtime.snapshot().powerHoldElapsedMs);

  input.powerButton = SignalState::Falling;
  runtime.step(input, 200 + Config::PowerHoldForcedMs);
  assertState(runtime, PcState::AwaitShutdown);
  assertTransition(runtime, TransitionEffect::ForcedShutdown);
  TEST_ASSERT_TRUE(runtime.snapshot().forcedShutdownLatched);
}

void test_runtime_10_reset_transition_keeps_running_persistent_state() {
  AuroraRuntime runtime;
  runtime.reset(11);
  reconcileRuntimeToRunning(runtime);
  AuroraInputFrame input = inputFrame(true);
  input.resetButton = SignalState::Rising;
  runtime.step(input, 250);

  assertState(runtime, PcState::Running);
  assertTransition(runtime, TransitionEffect::Reset);
  TEST_ASSERT_FALSE(runtime.snapshot().stateChanged);
  const uint8_t resetOrigin = static_cast<uint8_t>(
      Config::ShutdownOriginMin +
      (((250U >> 2) % (Config::ShutdownOriginMax -
                       Config::ShutdownOriginMin + 1U))));
  TEST_ASSERT_TRUE(runtime.ledFrame()[resetOrigin] ==
                   (Aurora::Rgb8{Config::ResetWaveBrightness, 0, 0}));
}

void test_runtime_11_await_shutdown_reaches_off_after_power_filter() {
  AuroraRuntime runtime;
  runtime.reset(12);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);
  releasePowerButton(runtime, 200);

  AuroraInputFrame input = inputFrame(false);
  runtime.step(input, 200 + Config::ShortPowerLedOffIgnoreMs - 1);
  assertState(runtime, PcState::AwaitShutdown);
  runtime.step(input, 200 + Config::ShortPowerLedOffIgnoreMs);
  assertState(runtime, PcState::Off);
  assertTransition(runtime, TransitionEffect::None);
}

void test_runtime_12_await_shutdown_timeout_returns_to_running() {
  AuroraRuntime runtime;
  runtime.reset(13);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);
  releasePowerButton(runtime, 200);

  AuroraInputFrame input = inputFrame(true);
  runtime.step(input, 200 + Config::AwaitShutdownTimeoutMs - 1);
  assertState(runtime, PcState::AwaitShutdown);
  runtime.step(input, 200 + Config::AwaitShutdownTimeoutMs);
  assertState(runtime, PcState::Running);
  assertAnimation(runtime, AnimationMode::Ambient);
  assertTransition(runtime, TransitionEffect::None);
}

void test_runtime_13_same_seed_and_timeline_produce_identical_frames() {
  AuroraRuntime left;
  AuroraRuntime right;
  left.reset(0x12345678UL);
  right.reset(0x12345678UL);
  AuroraInputFrame input = inputFrame(true);

  for (uint16_t frame = 0; frame < 150; ++frame) {
    const uint32_t nowMs = static_cast<uint32_t>(frame) * 20;
    input.hddLed = signalState((frame % 7) < 3);
    input.hddContribution = frame % 11 == 0 ? 64 : 0;
    left.step(input, nowMs);
    right.step(input, nowMs);
    TEST_ASSERT_EQUAL_HEX32(frameHash(left), frameHash(right));
  }
}

void test_runtime_14_fixed_step_aurora_matches_different_step_sizes() {
  AuroraRuntime step5;
  AuroraRuntime step20;
  AuroraRuntime step100;
  step5.reset(321);
  step20.reset(321);
  step100.reset(321);
  const AuroraInputFrame input = inputFrame(true);
  step5.step(input, 0);
  step20.step(input, 0);
  step100.step(input, 0);

  for (uint16_t nowMs = 5; nowMs <= 2000; nowMs += 5) {
    step5.step(input, nowMs);
  }
  for (uint16_t nowMs = 20; nowMs <= 2000; nowMs += 20) {
    step20.step(input, nowMs);
  }
  for (uint16_t nowMs = 100; nowMs <= 2000; nowMs += 100) {
    step100.step(input, nowMs);
  }

  TEST_ASSERT_EQUAL_HEX32(frameHash(step5), frameHash(step20));
  TEST_ASSERT_EQUAL_HEX32(frameHash(step20), frameHash(step100));
}

void test_runtime_15_timestamp_wrap_preserves_hold_and_render_timing() {
  AuroraRuntime runtime;
  const uint32_t resetAt = UINT32_MAX - 20;
  runtime.reset(14, resetAt);
  reconcileRuntimeToRunning(runtime, resetAt);
  startPowerHold(runtime, UINT32_MAX - 10);

  AuroraInputFrame input = inputFrame(true);
  input.powerButton = SignalState::High;
  runtime.step(input, Config::PowerHoldForcedMs - 11);
  TEST_ASSERT_TRUE(runtime.snapshot().forcedShutdownLatched);
  TEST_ASSERT_EQUAL_UINT32(Config::PowerHoldForcedMs,
                           runtime.snapshot().powerHoldElapsedMs);
  TEST_ASSERT_TRUE(runtime.snapshot().frameUpdated);
}

void test_runtime_16_strip_loss_blacks_frame_without_erasing_running_state() {
  AuroraRuntime runtime;
  runtime.reset(15);
  reconcileRuntimeToRunning(runtime);
  AuroraInputFrame litInput = inputFrame(true, true);
  litInput.hddLed = SignalState::High;
  for (uint32_t nowMs = Config::FrameIntervalMs; nowMs <= 600;
       nowMs += Config::FrameIntervalMs) {
    runtime.step(litInput, nowMs);
  }
  const uint32_t litHash = frameHash(runtime);
  const uint32_t prngBeforePowerLoss = runtime.auroraPrngStateForTest();

  AuroraInputFrame input = inputFrame(true, false);
  runtime.step(input, 600 + Config::FrameIntervalMs);
  assertState(runtime, PcState::Running);
  TEST_ASSERT_NOT_EQUAL(litHash, frameHash(runtime));
  for (uint8_t index = 0; index < runtime.ledCount(); ++index) {
    TEST_ASSERT_TRUE(runtime.ledFrame()[index] == Aurora::Rgb8{});
  }
  TEST_ASSERT_EQUAL_HEX32(prngBeforePowerLoss,
                          runtime.auroraPrngStateForTest());

  input.stripPowerPresent = true;
  runtime.step(input, 600 + Config::FrameIntervalMs * 2);
  assertState(runtime, PcState::Running);
  TEST_ASSERT_EQUAL_HEX32(prngBeforePowerLoss,
                          runtime.auroraPrngStateForTest());
}

void test_runtime_17_hdd_smoothing_uses_actual_elapsed_time() {
  AuroraRuntime runtime;
  runtime.reset(16);
  AuroraInputFrame input = inputFrame();
  input.hddLed = SignalState::High;
  runtime.step(input, Config::HddUpdateMs - 1);
  TEST_ASSERT_EQUAL_UINT8(Config::HddActiveRise - 1,
                          runtime.snapshot().hddActivity);

  runtime.step(input, Config::HddUpdateMs);
  TEST_ASSERT_EQUAL_UINT8(Config::HddActiveRise,
                          runtime.snapshot().hddActivity);
}

void test_runtime_18_portable_color_helpers_match_fastled_anchors() {
  struct HsvAnchor {
    uint8_t hue;
    Aurora::Rgb8 rgb;
  };
  const HsvAnchor anchors[] = {
      {0, {255, 0, 0}},   {32, {171, 85, 0}}, {64, {171, 170, 0}},
      {96, {0, 255, 0}},  {128, {0, 171, 85}},
      {160, {0, 0, 255}}, {192, {85, 0, 171}},
      {224, {170, 0, 85}},
  };
  for (const HsvAnchor &anchor : anchors) {
    TEST_ASSERT_TRUE(
        Aurora::hsvToRgb(anchor.hue, UINT8_MAX, UINT8_MAX) == anchor.rgb);
  }

  const Aurora::Rgb8 dimRed = Aurora::hsvToRgb(0, UINT8_MAX, 160);
  TEST_ASSERT_TRUE(dimRed == (Aurora::Rgb8{101, 0, 0}));
  TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, Aurora::scale8(UINT8_MAX, UINT8_MAX));
  TEST_ASSERT_EQUAL_UINT8(64, Aurora::scale8(128, 128));
}

void test_runtime_24_transition_snapshot_start_completion_and_unchanged() {
  AuroraRuntime runtime;
  runtime.reset(19);
  AuroraInputFrame input = inputFrame(false, true);
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::None),
                          static_cast<uint8_t>(
                              runtime.snapshot().previousTransition));
  assertTransition(runtime, TransitionEffect::Startup);

  input.powerButton = SignalState::High;
  runtime.step(input, 101);
  TEST_ASSERT_FALSE(runtime.snapshot().transitionChanged);

  runtime.step(input, 100 + Config::StartupDurationMs);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Startup),
                          static_cast<uint8_t>(
                              runtime.snapshot().previousTransition));
  assertTransition(runtime, TransitionEffect::None);
}

void test_runtime_25_transition_snapshot_forced_replaced_by_shutdown() {
  AuroraRuntime runtime;
  runtime.reset(20);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);
  TEST_ASSERT_FALSE(runtime.snapshot().transitionChanged);
  assertTransition(runtime, TransitionEffect::None);

  AuroraInputFrame held = inputFrame(true);
  held.powerButton = SignalState::High;
  runtime.step(held, 100 + Config::ForcedShutdownDelayMs - 1);
  assertTransition(runtime, TransitionEffect::None);

  runtime.step(held, 100 + Config::ForcedShutdownDelayMs);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  assertTransition(runtime, TransitionEffect::ForcedShutdown);
  TEST_ASSERT_EQUAL_UINT32(100 + Config::ForcedShutdownDelayMs,
                           runtime.snapshot().transitionStartedAtMs);
  TEST_ASSERT_EQUAL_UINT32(
      Config::PowerHoldForcedMs - Config::ForcedShutdownDelayMs,
      runtime.snapshot().transitionDurationMs);
  const Aurora::Rgb8 forcedInitialColor = {
      Aurora::scale8(static_cast<uint8_t>(Config::AuroraColor2Rgb >> 16),
                     Config::ForcedInitialBrightness),
      Aurora::scale8(static_cast<uint8_t>(Config::AuroraColor2Rgb >> 8),
                     Config::ForcedInitialBrightness),
      Aurora::scale8(static_cast<uint8_t>(Config::AuroraColor2Rgb),
                     Config::ForcedInitialBrightness),
  };
  TEST_ASSERT_TRUE(runtime.ledFrame()[0] == forcedInitialColor);

  releasePowerButton(runtime, 700);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TransitionEffect::ForcedShutdown),
      static_cast<uint8_t>(runtime.snapshot().previousTransition));
  assertTransition(runtime, TransitionEffect::Shutdown);
}

void test_runtime_26_transition_snapshot_reset_completes_to_none() {
  AuroraRuntime runtime;
  runtime.reset(21);
  reconcileRuntimeToRunning(runtime);
  AuroraInputFrame input = inputFrame(true);
  input.resetButton = SignalState::Rising;
  runtime.step(input, 100);
  assertTransition(runtime, TransitionEffect::Reset);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);

  input.resetButton = SignalState::High;
  runtime.step(input, 100 + Config::ResetDurationMs);
  assertTransition(runtime, TransitionEffect::None);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Reset),
                          static_cast<uint8_t>(
                              runtime.snapshot().previousTransition));
}

void test_runtime_27_runtime_prepares_transition_timing() {
  AuroraRuntime runtime;
  runtime.reset(22);
  AuroraInputFrame input = inputFrame(false, true);
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);
  input.powerButton = SignalState::High;
  runtime.step(input, 100 + Config::StartupDurationMs / 2);

  TEST_ASSERT_EQUAL_UINT32(Config::StartupDurationMs,
                           runtime.snapshot().transitionDurationMs);
  TEST_ASSERT_EQUAL_UINT32(Config::StartupDurationMs / 2,
                           runtime.snapshot().transitionElapsedMs);
  TEST_ASSERT_EQUAL_UINT8(127,
                          runtime.snapshot().transitionProgress);
}

void test_runtime_28_startup_reuses_field_when_ambient_begins() {
  constexpr uint32_t seed = 23;
  AuroraRuntime runtime;
  runtime.reset(seed);
  const uint32_t resetPrngState = runtime.auroraPrngStateForTest();

  AuroraInputFrame input = inputFrame(false, true);
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);
  assertAnimation(runtime, AnimationMode::Startup);

  input.powerButton = SignalState::High;
  input.powerLed = SignalState::High;
  runtime.step(input, 100 + Config::StartupDurationMs);
  assertAnimation(runtime, AnimationMode::Ambient);
  TEST_ASSERT_NOT_EQUAL(resetPrngState, runtime.auroraPrngStateForTest());
}

void test_runtime_29_signal_transitions_are_one_update_events() {
  AuroraRuntime runtime;
  runtime.reset(0x1234ABCDUL, 0);
  reconcileRuntimeToRunning(runtime);

  AuroraInputFrame input = inputFrame(true, true);
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);
  assertTransition(runtime, TransitionEffect::None);
  TEST_ASSERT_EQUAL_UINT32(0, runtime.snapshot().transitionStartedAtMs);

  input.powerButton = SignalState::High;
  runtime.step(input, 200);
  TEST_ASSERT_FALSE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT32(0, runtime.snapshot().transitionStartedAtMs);

  runtime.step(input, 100 + Config::ForcedShutdownDelayMs);
  assertTransition(runtime, TransitionEffect::ForcedShutdown);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT32(100 + Config::ForcedShutdownDelayMs,
                           runtime.snapshot().transitionStartedAtMs);

  input.powerButton = SignalState::Falling;
  runtime.step(input, 700);
  assertTransition(runtime, TransitionEffect::Shutdown);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  const uint32_t shutdownStartedAt =
      runtime.snapshot().transitionStartedAtMs;

  input.powerButton = SignalState::Low;
  runtime.step(input, 701);
  TEST_ASSERT_FALSE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT32(shutdownStartedAt,
                           runtime.snapshot().transitionStartedAtMs);
}

void test_runtime_30_reset_and_ambient_advance_one_shared_field() {
  AuroraRuntime resetting;
  AuroraRuntime ambient;
  resetting.reset(0x0BADCAFEUL);
  ambient.reset(0x0BADCAFEUL);
  reconcileRuntimeToRunning(resetting);
  reconcileRuntimeToRunning(ambient);

  AuroraInputFrame normal = inputFrame(true, true);
  for (uint32_t nowMs = Config::FrameIntervalMs; nowMs <= 600;
       nowMs += Config::FrameIntervalMs) {
    resetting.step(normal, nowMs);
    ambient.step(normal, nowMs);
  }

  AuroraInputFrame resetInput = normal;
  resetInput.resetButton = SignalState::Rising;
  resetting.step(resetInput, 620);
  ambient.step(normal, 620);
  assertAnimation(resetting, AnimationMode::Reset);

  const uint8_t resetOrigin = static_cast<uint8_t>(
      Config::ShutdownOriginMin +
      (((620U >> 2) % (Config::ShutdownOriginMax -
                       Config::ShutdownOriginMin + 1U))));
  TEST_ASSERT_GREATER_OR_EQUAL_UINT16(
      static_cast<uint16_t>(Config::ResetWaveBrightness) << 8,
      resetting.auroraDiagnostics(resetOrigin).brightnessQ8_8);

  resetInput.resetButton = SignalState::High;
  const uint32_t finishedAt = 620 + Config::ResetDurationMs;
  for (uint32_t nowMs = 640; nowMs <= finishedAt;
       nowMs += Config::FrameIntervalMs) {
    resetting.step(resetInput, nowMs);
    ambient.step(normal, nowMs);
  }
  if ((finishedAt - 640) % Config::FrameIntervalMs != 0) {
    resetting.step(resetInput, finishedAt);
    ambient.step(normal, finishedAt);
  }

  assertAnimation(resetting, AnimationMode::Ambient);
  TEST_ASSERT_EQUAL_HEX32(ambient.auroraPrngStateForTest(),
                          resetting.auroraPrngStateForTest());
}

void test_runtime_31_repeated_reset_restarts_only_animation_clock() {
  AuroraRuntime runtime;
  runtime.reset(31);
  const uint32_t resetPrngState = runtime.auroraPrngStateForTest();
  reconcileRuntimeToRunning(runtime);

  AuroraInputFrame input = inputFrame(true, true);
  for (uint32_t nowMs = Config::FrameIntervalMs; nowMs <= 600;
       nowMs += Config::FrameIntervalMs) {
    runtime.step(input, nowMs);
  }
  input.resetButton = SignalState::Rising;
  runtime.step(input, 700);
  TEST_ASSERT_EQUAL_UINT32(700, runtime.snapshot().transitionStartedAtMs);

  runtime.step(input, 900);
  assertAnimation(runtime, AnimationMode::Reset);
  TEST_ASSERT_EQUAL_UINT32(900, runtime.snapshot().transitionStartedAtMs);
  TEST_ASSERT_NOT_EQUAL(resetPrngState, runtime.auroraPrngStateForTest());
}

void test_runtime_32_power_transitions_override_reset_mode() {
  AuroraRuntime runtime;
  runtime.reset(32);
  reconcileRuntimeToRunning(runtime);

  AuroraInputFrame input = inputFrame(true, true);
  input.resetButton = SignalState::Rising;
  runtime.step(input, 100);
  assertTransition(runtime, TransitionEffect::Reset);

  input.resetButton = SignalState::High;
  input.powerButton = SignalState::Rising;
  runtime.step(input, 200);
  assertTransition(runtime, TransitionEffect::Reset);

  input.resetButton = SignalState::Rising;
  input.powerButton = SignalState::High;
  runtime.step(input, 300);
  assertTransition(runtime, TransitionEffect::Reset);

  input.resetButton = SignalState::High;
  runtime.step(input, 200 + Config::ForcedShutdownDelayMs);
  assertTransition(runtime, TransitionEffect::ForcedShutdown);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Reset),
                          static_cast<uint8_t>(
                              runtime.snapshot().previousTransition));
  TEST_ASSERT_EQUAL_UINT32(200 + Config::ForcedShutdownDelayMs,
                           runtime.snapshot().transitionStartedAtMs);

  input.powerButton = SignalState::Falling;
  runtime.step(input, 800);
  assertTransition(runtime, TransitionEffect::Shutdown);

  input.powerButton = SignalState::Low;
  runtime.step(input, 800 + Config::ShutdownDurationMs);
  assertAnimation(runtime, AnimationMode::Off);
  assertTransition(runtime, TransitionEffect::None);
}

void test_runtime_33_shutdown_wave_fades_behind_its_front() {
  AuroraRuntime runtime;
  runtime.reset(0x51A7D0A1UL);
  AuroraInputFrame input = inputFrame(true, true);
  runtime.step(input, 0);
  for (uint32_t nowMs = Config::FrameIntervalMs; nowMs <= 1200;
       nowMs += Config::FrameIntervalMs) {
    runtime.step(input, nowMs);
  }

  Aurora::Rgb8 ambientFrame[Aurora::LedCount];
  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    ambientFrame[index] = runtime.ledFrame()[index];
  }

  input.powerButton = SignalState::Rising;
  runtime.step(input, 1220);
  input.powerButton = SignalState::Falling;
  runtime.step(input, 1240);

  const uint8_t origin = static_cast<uint8_t>(
      Config::ShutdownOriginMin +
      (((1240U >> 2) % (Config::ShutdownOriginMax -
                        Config::ShutdownOriginMin + 1U))));
  const uint8_t farthest =
      origin > Aurora::LedCount - 1U - origin ? 0 : Aurora::LedCount - 1U;
  TEST_ASSERT_TRUE(runtime.ledFrame()[origin] == Config::ShutdownColor);
  TEST_ASSERT_TRUE(runtime.ledFrame()[farthest] == ambientFrame[farthest]);

  input.powerButton = SignalState::Low;
  const uint32_t originFadeMs =
      ((static_cast<uint32_t>(UINT8_MAX -
                             Config::ShutdownWaveTravelEndProgress) *
        Config::ShutdownDurationMs) +
       UINT8_MAX - 1U) /
      UINT8_MAX;
  runtime.step(input, 1240 + originFadeMs);
  TEST_ASSERT_TRUE(runtime.ledFrame()[origin] == Aurora::Rgb8{});
  TEST_ASSERT_TRUE(runtime.ledFrame()[farthest] == ambientFrame[farthest]);

  runtime.step(input, 1240 + Config::ShutdownDurationMs);
  assertAnimation(runtime, AnimationMode::Off);
  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    TEST_ASSERT_TRUE(runtime.ledFrame()[index] == Aurora::Rgb8{});
  }
}

}  // namespace

void runRuntimeTests() {
  RUN_TEST(test_runtime_01_cold_start_is_off_with_complete_black_frame);
  RUN_TEST(test_runtime_02_power_button_starts_and_waits_for_strip_power);
  RUN_TEST(test_runtime_03_strip_power_loss_cancels_and_restarts_startup);
  RUN_TEST(
      test_runtime_04_startup_needs_visual_completion_and_power_confirmation);
  RUN_TEST(
      test_runtime_05_completed_startup_waits_for_late_power_confirmation);
  RUN_TEST(
      test_runtime_06_running_sleeps_and_wakes_from_classified_power_led);
  RUN_TEST(test_runtime_07_short_power_press_requests_normal_shutdown);
  RUN_TEST(test_runtime_08_forced_shutdown_release_boundaries);
  RUN_TEST(test_runtime_09_forced_shutdown_latches_before_release);
  RUN_TEST(test_runtime_10_reset_transition_keeps_running_persistent_state);
  RUN_TEST(test_runtime_11_await_shutdown_reaches_off_after_power_filter);
  RUN_TEST(test_runtime_12_await_shutdown_timeout_returns_to_running);
  RUN_TEST(test_runtime_13_same_seed_and_timeline_produce_identical_frames);
  RUN_TEST(test_runtime_14_fixed_step_aurora_matches_different_step_sizes);
  RUN_TEST(test_runtime_15_timestamp_wrap_preserves_hold_and_render_timing);
  RUN_TEST(
      test_runtime_16_strip_loss_blacks_frame_without_erasing_running_state);
  RUN_TEST(test_runtime_17_hdd_smoothing_uses_actual_elapsed_time);
  RUN_TEST(test_runtime_18_portable_color_helpers_match_fastled_anchors);
  RUN_TEST(
      test_runtime_24_transition_snapshot_start_completion_and_unchanged);
  RUN_TEST(
      test_runtime_25_transition_snapshot_forced_replaced_by_shutdown);
  RUN_TEST(test_runtime_26_transition_snapshot_reset_completes_to_none);
  RUN_TEST(test_runtime_27_runtime_prepares_transition_timing);
  RUN_TEST(test_runtime_28_startup_reuses_field_when_ambient_begins);
  RUN_TEST(test_runtime_29_signal_transitions_are_one_update_events);
  RUN_TEST(test_runtime_30_reset_and_ambient_advance_one_shared_field);
  RUN_TEST(test_runtime_31_repeated_reset_restarts_only_animation_clock);
  RUN_TEST(test_runtime_32_power_transitions_override_reset_mode);
  RUN_TEST(test_runtime_33_shutdown_wave_fades_behind_its_front);
}
