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
  input.powerButton = SignalState::Rising;
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
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);
  assertTransition(runtime, TransitionEffect::Startup);

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
  AuroraRuntime runtime(Config::runtimeConfig());
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
  AuroraRuntime runtime(Config::runtimeConfig());
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
  AuroraRuntime runtime(Config::runtimeConfig());
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
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(11);
  reconcileRuntimeToRunning(runtime);
  AuroraInputFrame input = inputFrame(true);
  input.resetButton = SignalState::Rising;
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
    input.hddLed = signalState((frame % 7) < 3);
    input.hddContribution = frame % 11 == 0 ? 64 : 0;
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
  input.powerButton = SignalState::High;
  runtime.step(input, Config::PowerHoldForcedMs - 11);
  TEST_ASSERT_TRUE(runtime.snapshot().forcedShutdownLatched);
  TEST_ASSERT_EQUAL_UINT32(Config::PowerHoldForcedMs,
                           runtime.snapshot().powerHoldElapsedMs);
  TEST_ASSERT_TRUE(runtime.snapshot().frameUpdated);
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

void test_runtime_17_hdd_smoothing_uses_actual_elapsed_time() {
  AuroraRuntime runtime(Config::runtimeConfig());
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

void assertConfigError(const AuroraRuntimeConfig &config,
                       AuroraConfigError expected) {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(expected),
      static_cast<uint8_t>(validateAuroraRuntimeConfig(config)));
  AuroraRuntime runtime(config);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                          static_cast<uint8_t>(runtime.configError()));
  TEST_ASSERT_FALSE(runtime.configValid());
  runtime.step(inputFrame(true), 100);
  assertState(runtime, PcState::Off);
}

void test_runtime_19_default_configuration_is_valid() {
  const AuroraRuntimeConfig config = Config::runtimeConfig();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AuroraConfigError::None),
      static_cast<uint8_t>(validateAuroraRuntimeConfig(config)));
  AuroraRuntime runtime(config);
  TEST_ASSERT_TRUE(runtime.configValid());
}

void test_runtime_20_invalid_update_intervals_are_rejected() {
  AuroraRuntimeConfig config = Config::runtimeConfig();
  config.frameIntervalMs = 0;
  assertConfigError(config, AuroraConfigError::FrameIntervalZero);

  config = Config::runtimeConfig();
  config.hdd.updateMs = 0;
  assertConfigError(config, AuroraConfigError::HddUpdateIntervalZero);

  config = Config::runtimeConfig();
  config.auroraField.fixedStepMs = 0;
  assertConfigError(config, AuroraConfigError::AuroraFixedStepZero);

  config = Config::runtimeConfig();
  config.auroraField.ticksPerFade = 0;
  assertConfigError(config, AuroraConfigError::AuroraFadePeriodZero);
}

void test_runtime_21_invalid_spawn_ranges_are_rejected() {
  AuroraRuntimeConfig config = Config::runtimeConfig();
  config.auroraField.spawnMinTicks = 0;
  assertConfigError(config, AuroraConfigError::AuroraSpawnTickRange);

  config = Config::runtimeConfig();
  config.auroraField.spawnMinTicks =
      config.auroraField.spawnMaxTicks + 1;
  assertConfigError(config, AuroraConfigError::AuroraSpawnTickRange);

  config = Config::runtimeConfig();
  config.auroraField.spawnMinCount = 0;
  assertConfigError(config, AuroraConfigError::AuroraSpawnCountRange);

  config = Config::runtimeConfig();
  config.auroraField.spawnMinCount =
      config.auroraField.spawnMaxCount + 1;
  assertConfigError(config, AuroraConfigError::AuroraSpawnCountRange);

  config = Config::runtimeConfig();
  config.auroraField.spawnMaxCount = Aurora::LedCount + 1;
  assertConfigError(
      config, AuroraConfigError::AuroraSpawnCountExceedsLedCount);
}

void test_runtime_22_invalid_diffusion_is_rejected() {
  AuroraRuntimeConfig config = Config::runtimeConfig();
  config.auroraField.diffusionKernelSum = 0;
  assertConfigError(config,
                    AuroraConfigError::AuroraDiffusionKernelSumZero);

  config = Config::runtimeConfig();
  ++config.auroraField.diffusionKernelSum;
  assertConfigError(config,
                    AuroraConfigError::AuroraDiffusionKernelSumMismatch);
}

void test_runtime_23_invalid_renderer_configuration_is_rejected() {
  AuroraRuntimeConfig config = Config::runtimeConfig();
  config.renderer.sleep.travelIntervalMs = 0;
  assertConfigError(config, AuroraConfigError::SleepIntervalZero);

  config = Config::runtimeConfig();
  config.renderer.sleep.secondaryBrightnessDivisor = 0;
  assertConfigError(config, AuroraConfigError::SleepBrightnessDivisorZero);

  config = Config::runtimeConfig();
  config.renderer.transition.shutdownOriginMin =
      config.renderer.transition.shutdownOriginMax + 1;
  assertConfigError(config, AuroraConfigError::ShutdownOriginRange);

  config = Config::runtimeConfig();
  config.renderer.transition.shutdownOriginMax = Aurora::LedCount;
  assertConfigError(config, AuroraConfigError::ShutdownOriginRange);

  config = Config::runtimeConfig();
  config.renderer.transition.forcedFlashAtMs =
      config.pcState.forcedHoldMs + 1;
  assertConfigError(config,
                    AuroraConfigError::ForcedFlashAfterForcedHold);
}

void test_runtime_24_transition_snapshot_start_completion_and_unchanged() {
  AuroraRuntime runtime(Config::runtimeConfig());
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
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(20);
  reconcileRuntimeToRunning(runtime);
  startPowerHold(runtime, 100);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  assertTransition(runtime, TransitionEffect::ForcedShutdown);

  releasePowerButton(runtime, 500);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TransitionEffect::ForcedShutdown),
      static_cast<uint8_t>(runtime.snapshot().previousTransition));
  assertTransition(runtime, TransitionEffect::Shutdown);
}

void test_runtime_26_transition_snapshot_reset_completes_to_none() {
  AuroraRuntime runtime(Config::runtimeConfig());
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
  AuroraRuntime runtime(Config::runtimeConfig());
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

uint8_t countLitPixels(const AuroraRenderer &renderer) {
  uint8_t count = 0;
  for (uint8_t index = 0; index < renderer.ledCount(); ++index) {
    if (renderer.frame()[index] != Aurora::Rgb8{}) ++count;
  }
  return count;
}

void test_runtime_28_renderer_consumes_prepared_timing_context() {
  AuroraRenderer renderer(Config::rendererConfig(),
                          Config::auroraFieldConfig());
  renderer.reset(23);
  AuroraRenderContext context;
  context.pcState = PcState::Starting;
  context.transition = TransitionEffect::Startup;
  context.transitionElapsedMs = 100;
  context.transitionDurationMs = 200;
  context.transitionProgress = 127;
  context.logicalStripPowerPresent = true;
  renderer.render(context);
  TEST_ASSERT_EQUAL_UINT8(29, countLitPixels(renderer));

  context.transitionDurationMs = 400;
  context.transitionProgress = 63;
  renderer.render(context);
  TEST_ASSERT_EQUAL_UINT8(15, countLitPixels(renderer));
}

void test_runtime_29_signal_transitions_are_one_update_events() {
  AuroraRuntime runtime(Config::runtimeConfig());
  runtime.reset(0x1234ABCDUL, 0);
  reconcileRuntimeToRunning(runtime);

  AuroraInputFrame input = inputFrame(true, true);
  input.powerButton = SignalState::Rising;
  runtime.step(input, 100);
  assertTransition(runtime, TransitionEffect::ForcedShutdown);
  TEST_ASSERT_EQUAL_UINT32(100, runtime.snapshot().transitionStartedAtMs);

  input.powerButton = SignalState::High;
  runtime.step(input, 200);
  TEST_ASSERT_FALSE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT32(100, runtime.snapshot().transitionStartedAtMs);

  input.powerButton = SignalState::Falling;
  runtime.step(input, 500);
  assertTransition(runtime, TransitionEffect::Shutdown);
  TEST_ASSERT_TRUE(runtime.snapshot().transitionChanged);
  const uint32_t shutdownStartedAt =
      runtime.snapshot().transitionStartedAtMs;

  input.powerButton = SignalState::Low;
  runtime.step(input, 501);
  TEST_ASSERT_FALSE(runtime.snapshot().transitionChanged);
  TEST_ASSERT_EQUAL_UINT32(shutdownStartedAt,
                           runtime.snapshot().transitionStartedAtMs);
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
  RUN_TEST(test_runtime_17_hdd_smoothing_uses_actual_elapsed_time);
  RUN_TEST(test_runtime_18_portable_color_helpers_match_fastled_anchors);
  RUN_TEST(test_runtime_19_default_configuration_is_valid);
  RUN_TEST(test_runtime_20_invalid_update_intervals_are_rejected);
  RUN_TEST(test_runtime_21_invalid_spawn_ranges_are_rejected);
  RUN_TEST(test_runtime_22_invalid_diffusion_is_rejected);
  RUN_TEST(test_runtime_23_invalid_renderer_configuration_is_rejected);
  RUN_TEST(
      test_runtime_24_transition_snapshot_start_completion_and_unchanged);
  RUN_TEST(
      test_runtime_25_transition_snapshot_forced_replaced_by_shutdown);
  RUN_TEST(test_runtime_26_transition_snapshot_reset_completes_to_none);
  RUN_TEST(test_runtime_27_runtime_prepares_transition_timing);
  RUN_TEST(test_runtime_28_renderer_consumes_prepared_timing_context);
  RUN_TEST(test_runtime_29_signal_transitions_are_one_update_events);
}
