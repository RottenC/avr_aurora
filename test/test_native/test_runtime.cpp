#include <unity.h>

#include "config.h"
#include "core/aurora_runtime.h"
#include "core/portable_math.h"

namespace {

AuroraInputFrame inputFrame(bool powerLed = false,
                            bool stripPowerPresent = true) {
  AuroraInputFrame input;
  input.powerLed = powerLed;
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

void reconcileRuntimeToRunning(AuroraRuntime &runtime, uint32_t nowMs = 0) {
  AuroraInputFrame input = inputFrame(true);
  runtime.step(input, nowMs);
  assertState(runtime, PcState::Running);
}

void startPowerHold(AuroraRuntime &runtime, uint32_t nowMs) {
  AuroraInputFrame input = inputFrame(true);
  input.powerButton = true;
  input.powerButtonPressed = true;
  runtime.step(input, nowMs);
}

void releasePowerButton(AuroraRuntime &runtime, uint32_t nowMs) {
  AuroraInputFrame input = inputFrame(true);
  input.powerButtonReleased = true;
  runtime.step(input, nowMs);
}

void test_runtime_01_cold_start_is_off_with_complete_black_frame() {
  AuroraRuntime runtime(Config::runtimeConfig());
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
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(1);
  AuroraInputFrame input = inputFrame(false, false);
  input.powerButton = true;
  input.powerButtonPressed = true;
  runtime.step(input, 100);

  assertState(runtime, PcState::Starting);
  assertTransition(runtime, TransitionEffect::None);
  TEST_ASSERT_TRUE(runtime.snapshot().stateChanged);
  TEST_ASSERT_EQUAL_UINT32(100, runtime.snapshot().stateStartedAtMs);
}

void test_runtime_03_strip_power_loss_cancels_and_restarts_startup() {
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(1);
  AuroraInputFrame input = inputFrame();
  input.powerButtonPressed = true;
  runtime.step(input, 100);
  assertTransition(runtime, TransitionEffect::Startup);

  input.powerButtonPressed = false;
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
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(5);
  AuroraInputFrame input = inputFrame();
  input.powerButtonPressed = true;
  runtime.step(input, 100);

  input.powerButtonPressed = false;
  input.powerLed = true;
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
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(6);
  AuroraInputFrame input = inputFrame();
  input.powerButtonPressed = true;
  runtime.step(input, 50);
  input.powerButtonPressed = false;
  runtime.step(input, 50 + Config::StartupDurationMs);
  assertState(runtime, PcState::Starting);
  assertTransition(runtime, TransitionEffect::None);

  input.powerLed = true;
  runtime.step(input, 50 + Config::StartupDurationMs + 100);
  assertState(runtime, PcState::Running);
}

void test_runtime_06_running_sleeps_and_wakes_from_classified_power_led() {
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(7);
  AuroraInputFrame input = inputFrame(true);
  runtime.step(input, 0);

  input.powerLed = false;
  runtime.step(input, 200);
  input.powerLed = true;
  runtime.step(input, 400);
  input.powerLed = false;
  runtime.step(input, 600);
  input.powerLed = true;
  runtime.step(input, 800);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PowerLedMode::Blinking),
                          static_cast<uint8_t>(
                              runtime.snapshot().powerLedMode));
  assertState(runtime, PcState::Sleeping);

  runtime.step(input, 800 + Config::PowerLedBlinkStaleMs + 1);
  assertState(runtime, PcState::Running);
}

void test_runtime_07_short_power_press_requests_normal_shutdown() {
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(8);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);
  TEST_ASSERT_EQUAL_UINT32(0, runtime.snapshot().powerHoldElapsedMs);
  assertTransition(runtime, TransitionEffect::ForcedShutdown);

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
    AuroraRuntime runtime(Config::runtimeConfig());
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
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(10);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);

  AuroraInputFrame input = inputFrame(true);
  input.powerButton = true;
  runtime.step(input, 100 + Config::PowerHoldForcedMs);
  TEST_ASSERT_TRUE(runtime.snapshot().forcedShutdownLatched);
  TEST_ASSERT_EQUAL_UINT32(Config::PowerHoldForcedMs,
                           runtime.snapshot().powerHoldElapsedMs);

  input.powerButton = false;
  input.powerButtonReleased = true;
  runtime.step(input, 200 + Config::PowerHoldForcedMs);
  assertState(runtime, PcState::AwaitShutdown);
  assertTransition(runtime, TransitionEffect::ForcedShutdown);
  TEST_ASSERT_TRUE(runtime.snapshot().forcedShutdownLatched);
}

void test_runtime_10_reset_transition_keeps_running_persistent_state() {
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(11);
  reconcileRuntimeToRunning(runtime);
  AuroraInputFrame input = inputFrame(true);
  input.resetButton = true;
  input.resetButtonPressed = true;
  runtime.step(input, 250);

  assertState(runtime, PcState::Running);
  assertTransition(runtime, TransitionEffect::Reset);
  TEST_ASSERT_FALSE(runtime.snapshot().stateChanged);
}

void test_runtime_11_await_shutdown_reaches_off_after_power_filter() {
  AuroraRuntime runtime(Config::runtimeConfig());
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

void test_runtime_12_await_shutdown_timeout_reaches_warn() {
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(13);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);
  releasePowerButton(runtime, 200);

  AuroraInputFrame input = inputFrame(true);
  runtime.step(input, 200 + Config::ShutdownWarningTimeoutMs - 1);
  assertState(runtime, PcState::AwaitShutdown);
  runtime.step(input, 200 + Config::ShutdownWarningTimeoutMs);
  assertState(runtime, PcState::Warn);
}

void test_runtime_13_same_seed_and_timeline_produce_identical_frames() {
  AuroraRuntime left(Config::runtimeConfig());
  AuroraRuntime right(Config::runtimeConfig());
  left.reset(0x12345678UL);
  right.reset(0x12345678UL);
  AuroraInputFrame input = inputFrame(true);

  for (uint16_t frame = 0; frame < 150; ++frame) {
    const uint32_t nowMs = static_cast<uint32_t>(frame) * 20;
    input.hddLed = (frame % 7) < 3;
    input.hddActiveEdges = frame % 11 == 0 ? 1 : 0;
    left.step(input, nowMs);
    right.step(input, nowMs);
    TEST_ASSERT_EQUAL_HEX32(frameHash(left), frameHash(right));
  }
}

void test_runtime_14_fixed_step_aurora_matches_different_step_sizes() {
  AuroraRuntime step5(Config::runtimeConfig());
  AuroraRuntime step20(Config::runtimeConfig());
  AuroraRuntime step100(Config::runtimeConfig());
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
  AuroraRuntime runtime(Config::runtimeConfig());
  const uint32_t resetAt = UINT32_MAX - 20;
  runtime.reset(14, resetAt);
  reconcileRuntimeToRunning(runtime, resetAt);
  startPowerHold(runtime, UINT32_MAX - 10);

  AuroraInputFrame input = inputFrame(true);
  input.powerButton = true;
  runtime.step(input, Config::PowerHoldForcedMs - 11);
  TEST_ASSERT_TRUE(runtime.snapshot().forcedShutdownLatched);
  TEST_ASSERT_EQUAL_UINT32(Config::PowerHoldForcedMs,
                           runtime.snapshot().powerHoldElapsedMs);
  TEST_ASSERT_TRUE(runtime.snapshot().frameChanged);
}

void test_runtime_16_strip_loss_blacks_frame_without_erasing_running_state() {
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(15);
  reconcileRuntimeToRunning(runtime);
  const uint32_t litHash = frameHash(runtime);

  AuroraInputFrame input = inputFrame(true, false);
  runtime.step(input, Config::FrameIntervalMs);
  assertState(runtime, PcState::Running);
  TEST_ASSERT_NOT_EQUAL(litHash, frameHash(runtime));
  for (uint8_t index = 0; index < runtime.ledCount(); ++index) {
    TEST_ASSERT_TRUE(runtime.ledFrame()[index] == Aurora::Rgb8{});
  }

  input.stripPowerPresent = true;
  runtime.step(input, Config::FrameIntervalMs * 2);
  assertState(runtime, PcState::Running);
}

void test_runtime_17_hdd_edges_accumulate_until_core_update_tick() {
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(16);
  AuroraInputFrame input = inputFrame();
  input.hddActiveEdges = 1;
  runtime.step(input, Config::HddUpdateMs - 1);
  TEST_ASSERT_EQUAL_UINT8(0, runtime.snapshot().hddActivity);

  input.hddActiveEdges = 0;
  runtime.step(input, Config::HddUpdateMs);
  TEST_ASSERT_EQUAL_UINT8(Config::HddEdgeBoost - Config::HddInactiveDecay,
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
  RUN_TEST(test_runtime_12_await_shutdown_timeout_reaches_warn);
  RUN_TEST(test_runtime_13_same_seed_and_timeline_produce_identical_frames);
  RUN_TEST(test_runtime_14_fixed_step_aurora_matches_different_step_sizes);
  RUN_TEST(test_runtime_15_timestamp_wrap_preserves_hold_and_render_timing);
  RUN_TEST(
      test_runtime_16_strip_loss_blacks_frame_without_erasing_running_state);
  RUN_TEST(test_runtime_17_hdd_edges_accumulate_until_core_update_tick);
  RUN_TEST(test_runtime_18_portable_color_helpers_match_fastled_anchors);
}
