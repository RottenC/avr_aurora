#include <unity.h>

#include "config.h"
#include "effect_controller.h"
#include "hdd_activity.h"
#include "pc_state.h"
#include "power_led_tracker.h"

void runAuroraTests();

namespace {
PcStateMachine makePcStateMachine() {
  return PcStateMachine({Config::PowerHoldForcedMs, Config::StartingTimeoutMs, Config::ShutdownWarningTimeoutMs});
}

EffectController makeEffectController() {
  return EffectController({Config::StartupDurationMs,
                           Config::ShutdownDurationMs,
                           Config::ResetDurationMs});
}

PcStateInputs makeInputs(PowerLedMode mode, bool stripPowerPresent = true) {
  return {stripPowerPresent, false, false, false, false, mode, false};
}

void reconcileToRunning(PcStateMachine &pc) {
  pc.update(makeInputs(PowerLedMode::On), 0);
}

void beginPowerHold(PcStateMachine &pc, uint32_t nowMs) {
  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButton = true;
  inputs.powerButtonPressed = true;
  pc.update(inputs, nowMs);
}
}

void test_auto_01_off_continuously_reconciles_to_observed_state() {
  PcStateMachine pc = makePcStateMachine();
  pc.update(makeInputs(PowerLedMode::Off), 10);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Off), static_cast<uint8_t>(pc.state()));

  pc.update(makeInputs(PowerLedMode::On), 20);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));

  pc.update(makeInputs(PowerLedMode::Off), 30);
  PcStateInputs sleeping = makeInputs(PowerLedMode::Blinking, false);
  pc.update(sleeping, 40);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Sleeping), static_cast<uint8_t>(pc.state()));
}

void test_auto_02_power_button_starts_before_observed_on_reconciliation() {
  PcStateMachine pc = makePcStateMachine();
  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButtonPressed = true;
  const PcStateEvents events = pc.update(inputs, 100);
  TEST_ASSERT_TRUE(events.requestStartup);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting), static_cast<uint8_t>(pc.state()));
}

void test_auto_03_startup_requires_animation_and_power_confirmation() {
  PcStateMachine pc = makePcStateMachine();
  PcStateInputs inputs = makeInputs(PowerLedMode::Off);
  inputs.powerButtonPressed = true;
  PcStateEvents events = pc.update(inputs, 100);
  TEST_ASSERT_TRUE(events.requestStartup);

  inputs.powerButtonPressed = false;
  inputs.powerMode = PowerLedMode::On;
  pc.update(inputs, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting), static_cast<uint8_t>(pc.state()));

  inputs.startupTransitionFinished = true;
  pc.update(inputs, 300);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_auto_04_startup_waits_for_strip_power_then_requests_once() {
  PcStateMachine pc = makePcStateMachine();
  PcStateInputs inputs = makeInputs(PowerLedMode::Off, false);
  inputs.powerButtonPressed = true;
  PcStateEvents events = pc.update(inputs, 100);
  TEST_ASSERT_FALSE(events.requestStartup);

  inputs.powerButtonPressed = false;
  events = pc.update(inputs, 200);
  TEST_ASSERT_FALSE(events.requestStartup);

  inputs.stripPowerPresent = true;
  events = pc.update(inputs, 300);
  TEST_ASSERT_TRUE(events.requestStartup);
  events = pc.update(inputs, 400);
  TEST_ASSERT_FALSE(events.requestStartup);
}

void test_auto_05_strip_power_loss_cancels_and_return_restarts_startup() {
  PcStateMachine pc = makePcStateMachine();
  PcStateInputs inputs = makeInputs(PowerLedMode::Off);
  inputs.powerButtonPressed = true;
  pc.update(inputs, 100);

  inputs.powerButtonPressed = false;
  inputs.stripPowerPresent = false;
  PcStateEvents events = pc.update(inputs, 200);
  TEST_ASSERT_TRUE(events.cancelStartup);

  events = pc.update(inputs, 300);
  TEST_ASSERT_FALSE(events.cancelStartup);
  inputs.stripPowerPresent = true;
  events = pc.update(inputs, 400);
  TEST_ASSERT_TRUE(events.requestStartup);
}

void test_auto_06_startup_times_out_or_reconciles_to_sleep() {
  PcStateMachine pc = makePcStateMachine();
  PcStateInputs inputs = makeInputs(PowerLedMode::Off);
  inputs.powerButtonPressed = true;
  pc.update(inputs, 100);
  inputs.powerButtonPressed = false;
  PcStateEvents events = pc.update(inputs, 100 + Config::StartingTimeoutMs);
  TEST_ASSERT_TRUE(events.cancelStartup);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Off), static_cast<uint8_t>(pc.state()));

  inputs.powerButtonPressed = true;
  pc.update(inputs, 50000);
  inputs.powerButtonPressed = false;
  inputs.powerMode = PowerLedMode::Blinking;
  events = pc.update(inputs, 50100);
  TEST_ASSERT_TRUE(events.cancelStartup);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Sleeping), static_cast<uint8_t>(pc.state()));
}

void test_auto_07_short_power_press_cancels_preview_and_starts_shutdown() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  beginPowerHold(pc, 100);

  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButtonReleased = true;
  const PcStateEvents events = pc.update(inputs, 500);
  TEST_ASSERT_TRUE(events.cancelForcedShutdown);
  TEST_ASSERT_TRUE(events.requestShutdown);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::AwaitShutdown), static_cast<uint8_t>(pc.state()));
}

void test_auto_08_forced_hold_latches_and_release_uses_no_normal_shutdown() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  beginPowerHold(pc, 100);

  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButton = true;
  PcStateEvents events = pc.update(inputs, 100 + Config::PowerHoldForcedMs);
  TEST_ASSERT_TRUE(pc.forcedLatched());

  inputs.powerButton = false;
  inputs.powerButtonReleased = true;
  events = pc.update(inputs, 200 + Config::PowerHoldForcedMs);
  TEST_ASSERT_FALSE(events.cancelForcedShutdown);
  TEST_ASSERT_FALSE(events.requestShutdown);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::AwaitShutdown), static_cast<uint8_t>(pc.state()));
}

void test_auto_09_release_at_forced_threshold_latches_without_intermediate_update() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  beginPowerHold(pc, 100);

  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButtonReleased = true;
  const PcStateEvents events = pc.update(inputs, 100 + Config::PowerHoldForcedMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::AwaitShutdown), static_cast<uint8_t>(pc.state()));
  TEST_ASSERT_TRUE(pc.forcedLatched());
  TEST_ASSERT_FALSE(events.requestShutdown);
  TEST_ASSERT_FALSE(events.cancelForcedShutdown);
}

void test_auto_10_power_mode_reconciliation_does_not_interrupt_hold() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  beginPowerHold(pc, 100);

  PcStateInputs inputs = makeInputs(PowerLedMode::Blinking);
  inputs.powerButton = true;
  pc.update(inputs, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_auto_11_sleep_wake_and_shutdown_completion_reconcile_continuously() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  pc.update(makeInputs(PowerLedMode::Blinking), 100);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Sleeping), static_cast<uint8_t>(pc.state()));
  pc.update(makeInputs(PowerLedMode::On), 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));

  beginPowerHold(pc, 300);
  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButtonReleased = true;
  pc.update(inputs, 400);
  pc.update(makeInputs(PowerLedMode::Off), 500);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Off), static_cast<uint8_t>(pc.state()));
}

void test_auto_12_transition_priority_restart_and_completion() {
  EffectController effects = makeEffectController();
  effects.request(TransitionEffect::Shutdown, 100);
  effects.request(TransitionEffect::Startup, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Shutdown),
                          static_cast<uint8_t>(effects.current()));
  TEST_ASSERT_EQUAL_UINT32(100, effects.startedAt());

  effects.request(TransitionEffect::Shutdown, 300);
  TEST_ASSERT_EQUAL_UINT32(100, effects.startedAt());
  effects.restart(TransitionEffect::Shutdown, 400);
  TEST_ASSERT_EQUAL_UINT32(400, effects.startedAt());

  effects.update(400 + Config::ShutdownDurationMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::None),
                          static_cast<uint8_t>(effects.current()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Shutdown),
                          static_cast<uint8_t>(effects.consumeFinished()));
}

void test_auto_13_forced_transition_has_highest_priority_and_stays_latched() {
  EffectController effects = makeEffectController();
  effects.request(TransitionEffect::Reset, 100);
  effects.request(TransitionEffect::ForcedShutdown, 200);
  effects.request(TransitionEffect::Shutdown, 300);
  effects.update(200 + Config::PowerHoldForcedMs + 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown),
                          static_cast<uint8_t>(effects.current()));
}

void test_auto_14_effect_cancellation_is_specific_and_state_compatible() {
  EffectController effects = makeEffectController();
  effects.request(TransitionEffect::ForcedShutdown, 100);
  effects.cancel(TransitionEffect::Startup);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown),
                          static_cast<uint8_t>(effects.current()));

  effects.reconcile(PcState::Off);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::None),
                          static_cast<uint8_t>(effects.current()));

  effects.request(TransitionEffect::Startup, 200);
  effects.reconcile(PcState::Starting);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Startup),
                          static_cast<uint8_t>(effects.current()));
  effects.reconcile(PcState::Running);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::None),
                          static_cast<uint8_t>(effects.current()));
}

void test_auto_15_hdd_edges_accumulate_without_changing_fixed_time_ticks() {
  HddActivity hdd({Config::HddUpdateMs,
                   Config::HddEdgeBoost,
                   Config::HddActiveRise,
                   Config::HddInactiveDecay,
                   Config::HddMax});
  hdd.update(false, 3, Config::HddUpdateMs);
  TEST_ASSERT_EQUAL_UINT8(Config::HddEdgeBoost * 3 - Config::HddInactiveDecay, hdd.value());

  const uint16_t elapsed = Config::HddUpdateMs * 3 + Config::HddUpdateMs - 1;
  hdd.update(false, 0, elapsed);
  TEST_ASSERT_EQUAL_UINT8(Config::HddEdgeBoost * 3 - Config::HddInactiveDecay * 4, hdd.value());

  hdd.update(true, UINT8_MAX, Config::HddUpdateMs);
  TEST_ASSERT_EQUAL_UINT8(Config::HddMax, hdd.value());
}

void test_auto_16_power_led_tracker_preserves_off_grace_and_blink_timing() {
  PowerLedTracker tracker({Config::ShortPowerLedOffIgnoreMs,
                           Config::PowerLedBlinkMinHalfPeriodMs,
                           Config::PowerLedBlinkMaxHalfPeriodMs,
                           Config::PowerLedBlinkStaleMs,
                           Config::PowerLedBlinkEdgesRequired});
  tracker.update(false, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PowerLedMode::Off),
                          static_cast<uint8_t>(tracker.mode(1)));

  tracker.update(true, 200);
  tracker.update(false, 400);
  tracker.update(true, 600);
  tracker.update(false, 800);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PowerLedMode::Blinking),
                          static_cast<uint8_t>(tracker.mode(800)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PowerLedMode::Off),
                          static_cast<uint8_t>(tracker.mode(800 + Config::PowerLedBlinkStaleMs + 1)));
}


void test_auto_17_forced_shutdown_boundary() {
  const uint32_t releases[] = {Config::PowerHoldForcedMs - 1, Config::PowerHoldForcedMs, Config::PowerHoldForcedMs + 1};
  const bool forced[] = {false, true, true};
  for (uint8_t i = 0; i < 3; ++i) {
    PcStateMachine pc = makePcStateMachine();
    reconcileToRunning(pc);
    beginPowerHold(pc, 100);
    PcStateInputs inputs = makeInputs(PowerLedMode::On);
    inputs.powerButtonReleased = true;
    const PcStateEvents events = pc.update(inputs, 100 + releases[i]);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::AwaitShutdown), static_cast<uint8_t>(pc.state()));
    TEST_ASSERT_EQUAL(forced[i], pc.forcedLatched());
    TEST_ASSERT_EQUAL(!forced[i], events.requestShutdown);
    TEST_ASSERT_EQUAL(!forced[i], events.cancelForcedShutdown);
  }
}

void test_auto_18_millis_overflow_state_and_effect_timing() {
  PcStateMachine pc = makePcStateMachine();
  pc.update(makeInputs(PowerLedMode::On), UINT32_MAX - 20);
  beginPowerHold(pc, UINT32_MAX - 10);
  PcStateInputs hold = makeInputs(PowerLedMode::On);
  hold.powerButton = true;
  pc.update(hold, Config::PowerHoldForcedMs - 11);
  TEST_ASSERT_TRUE(pc.forcedLatched());

  PcStateMachine starting = makePcStateMachine();
  PcStateInputs start = makeInputs(PowerLedMode::Off);
  start.powerButtonPressed = true;
  starting.update(start, UINT32_MAX - 5);
  start.powerButtonPressed = false;
  starting.update(start, Config::StartingTimeoutMs - 6);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Off), static_cast<uint8_t>(starting.state()));

  PcStateMachine shutdown = makePcStateMachine();
  reconcileToRunning(shutdown);
  beginPowerHold(shutdown, UINT32_MAX - 30);
  PcStateInputs release = makeInputs(PowerLedMode::On);
  release.powerButtonReleased = true;
  shutdown.update(release, UINT32_MAX - 20);
  shutdown.update(makeInputs(PowerLedMode::On), Config::ShutdownWarningTimeoutMs - 21);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Warn), static_cast<uint8_t>(shutdown.state()));

  EffectController effects = makeEffectController();
  effects.request(TransitionEffect::Reset, UINT32_MAX - 100);
  effects.update(Config::ResetDurationMs - 101);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::None), static_cast<uint8_t>(effects.current()));
}

void test_auto_19_20_power_led_on_without_strip_power_then_strip_returns() {
  PcStateMachine pc = makePcStateMachine();
  PcStateEvents events = pc.update(makeInputs(PowerLedMode::On, false), 10);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
  TEST_ASSERT_FALSE(events.requestStartup);
  events = pc.update(makeInputs(PowerLedMode::On, true), 20);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
  TEST_ASSERT_FALSE(events.requestStartup);
}

void test_auto_21_shutdown_states_ignore_blinking_until_off() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  beginPowerHold(pc, 100);
  PcStateInputs release = makeInputs(PowerLedMode::On);
  release.powerButtonReleased = true;
  pc.update(release, 200);
  pc.update(makeInputs(PowerLedMode::Blinking), 300);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::AwaitShutdown), static_cast<uint8_t>(pc.state()));
  pc.update(makeInputs(PowerLedMode::On), 200 + Config::ShutdownWarningTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Warn), static_cast<uint8_t>(pc.state()));
  pc.update(makeInputs(PowerLedMode::Blinking), 300 + Config::ShutdownWarningTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Warn), static_cast<uint8_t>(pc.state()));
}

void test_auto_22_reset_priority_and_restart() {
  EffectController effects = makeEffectController();
  effects.request(TransitionEffect::Startup, 100);
  effects.restart(TransitionEffect::Reset, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Reset), static_cast<uint8_t>(effects.current()));
  effects.request(TransitionEffect::Shutdown, 300);
  effects.restart(TransitionEffect::Reset, 400);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Shutdown), static_cast<uint8_t>(effects.current()));
  effects.request(TransitionEffect::ForcedShutdown, 500);
  effects.restart(TransitionEffect::Reset, 600);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown), static_cast<uint8_t>(effects.current()));
  EffectController reset = makeEffectController();
  reset.restart(TransitionEffect::Reset, 1000);
  reset.restart(TransitionEffect::Reset, 1300);
  TEST_ASSERT_EQUAL_UINT32(1300, reset.startedAt());
}

void test_auto_23_strip_power_loss_while_running_preserves_logic() {
  PcStateMachine pc = makePcStateMachine();
  pc.update(makeInputs(PowerLedMode::On), 0);
  PcStateEvents events = pc.update(makeInputs(PowerLedMode::On, false), 10);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
  TEST_ASSERT_FALSE(events.requestStartup);
  events = pc.update(makeInputs(PowerLedMode::On, true), 20);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
  TEST_ASSERT_FALSE(events.requestStartup);
}

void test_auto_24_large_hdd_elapsed_interval_saturates_and_decays() {
  HddActivity hdd({Config::HddUpdateMs, Config::HddEdgeBoost, Config::HddActiveRise, Config::HddInactiveDecay, Config::HddMax});
  hdd.update(true, UINT8_MAX, UINT32_MAX);
  TEST_ASSERT_EQUAL_UINT8(Config::HddMax, hdd.value());
  hdd.update(false, 0, UINT32_MAX);
  TEST_ASSERT_EQUAL_UINT8(0, hdd.value());
  hdd.update(false, 2, Config::HddUpdateMs);
  TEST_ASSERT_TRUE(hdd.value() <= Config::HddMax);
}

void test_auto_25_invalid_power_led_blink_periods_and_stale() {
  PowerLedTracker tracker({Config::ShortPowerLedOffIgnoreMs, Config::PowerLedBlinkMinHalfPeriodMs, Config::PowerLedBlinkMaxHalfPeriodMs, Config::PowerLedBlinkStaleMs, Config::PowerLedBlinkEdgesRequired});
  tracker.update(false, 0);
  tracker.update(true, Config::PowerLedBlinkMinHalfPeriodMs - 1);
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(PowerLedMode::Blinking), static_cast<uint8_t>(tracker.mode(Config::PowerLedBlinkMinHalfPeriodMs)));
  PowerLedTracker mixed({Config::ShortPowerLedOffIgnoreMs, Config::PowerLedBlinkMinHalfPeriodMs, Config::PowerLedBlinkMaxHalfPeriodMs, Config::PowerLedBlinkStaleMs, Config::PowerLedBlinkEdgesRequired});
  mixed.update(false, 0); mixed.update(true, 200); mixed.update(false, 5000); mixed.update(true, 5200); mixed.update(false, 5400); mixed.update(true, 5600);
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(PowerLedMode::Blinking), static_cast<uint8_t>(mixed.mode(5600)));
}

void test_auto_26_startup_animation_finish_waits_for_power_led() {
  PcStateMachine pc = makePcStateMachine();
  PcStateInputs inputs = makeInputs(PowerLedMode::Off);
  inputs.powerButtonPressed = true;
  PcStateEvents events = pc.update(inputs, 100);
  TEST_ASSERT_TRUE(events.requestStartup);
  inputs.powerButtonPressed = false;
  inputs.startupTransitionFinished = true;
  events = pc.update(inputs, 100 + Config::StartupDurationMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting), static_cast<uint8_t>(pc.state()));
  TEST_ASSERT_FALSE(events.requestStartup);
  pc.update(inputs, 100 + Config::StartupDurationMs + 1000);
  inputs.powerMode = PowerLedMode::On;
  events = pc.update(inputs, 100 + Config::StartupDurationMs + 2000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
  TEST_ASSERT_FALSE(events.requestStartup);
}

void test_auto_27_boot_running_and_sleeping_are_separate() {
  PowerLedTracker running({Config::ShortPowerLedOffIgnoreMs, Config::PowerLedBlinkMinHalfPeriodMs, Config::PowerLedBlinkMaxHalfPeriodMs, Config::PowerLedBlinkStaleMs, Config::PowerLedBlinkEdgesRequired});
  running.update(true, 0);
  PcStateMachine pc = makePcStateMachine();
  pc.update(makeInputs(running.mode(0)), 0);
  pc.update(makeInputs(running.mode(10000)), 10000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));

  PowerLedTracker sleep({Config::ShortPowerLedOffIgnoreMs, Config::PowerLedBlinkMinHalfPeriodMs, Config::PowerLedBlinkMaxHalfPeriodMs, Config::PowerLedBlinkStaleMs, Config::PowerLedBlinkEdgesRequired});
  PcStateMachine sleeper = makePcStateMachine();
  sleep.update(true, 0); sleeper.update(makeInputs(sleep.mode(0)), 0);
  sleep.update(false, 500); sleeper.update(makeInputs(sleep.mode(500)), 500);
  sleep.update(true, 1000); sleeper.update(makeInputs(sleep.mode(1000)), 1000);
  sleep.update(false, 1500); sleeper.update(makeInputs(sleep.mode(1500)), 1500);
  sleep.update(true, 2000); sleeper.update(makeInputs(sleep.mode(2000)), 2000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Sleeping), static_cast<uint8_t>(sleeper.state()));
}

void test_auto_28_await_shutdown_timeout_and_off() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc); beginPowerHold(pc, 100);
  PcStateInputs release = makeInputs(PowerLedMode::On); release.powerButtonReleased = true;
  pc.update(release, 200);
  pc.update(makeInputs(PowerLedMode::On), 200 + Config::ShutdownWarningTimeoutMs - 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::AwaitShutdown), static_cast<uint8_t>(pc.state()));
  pc.update(makeInputs(PowerLedMode::On), 200 + Config::ShutdownWarningTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Warn), static_cast<uint8_t>(pc.state()));
  pc.update(makeInputs(PowerLedMode::Off), 200 + Config::ShutdownWarningTimeoutMs + 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Off), static_cast<uint8_t>(pc.state()));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_auto_01_off_continuously_reconciles_to_observed_state);
  RUN_TEST(test_auto_02_power_button_starts_before_observed_on_reconciliation);
  RUN_TEST(test_auto_03_startup_requires_animation_and_power_confirmation);
  RUN_TEST(test_auto_04_startup_waits_for_strip_power_then_requests_once);
  RUN_TEST(test_auto_05_strip_power_loss_cancels_and_return_restarts_startup);
  RUN_TEST(test_auto_06_startup_times_out_or_reconciles_to_sleep);
  RUN_TEST(test_auto_07_short_power_press_cancels_preview_and_starts_shutdown);
  RUN_TEST(test_auto_08_forced_hold_latches_and_release_uses_no_normal_shutdown);
  RUN_TEST(test_auto_09_release_at_forced_threshold_latches_without_intermediate_update);
  RUN_TEST(test_auto_10_power_mode_reconciliation_does_not_interrupt_hold);
  RUN_TEST(test_auto_11_sleep_wake_and_shutdown_completion_reconcile_continuously);
  RUN_TEST(test_auto_12_transition_priority_restart_and_completion);
  RUN_TEST(test_auto_13_forced_transition_has_highest_priority_and_stays_latched);
  RUN_TEST(test_auto_14_effect_cancellation_is_specific_and_state_compatible);
  RUN_TEST(test_auto_15_hdd_edges_accumulate_without_changing_fixed_time_ticks);
  RUN_TEST(test_auto_16_power_led_tracker_preserves_off_grace_and_blink_timing);
  RUN_TEST(test_auto_17_forced_shutdown_boundary);
  RUN_TEST(test_auto_18_millis_overflow_state_and_effect_timing);
  RUN_TEST(test_auto_19_20_power_led_on_without_strip_power_then_strip_returns);
  RUN_TEST(test_auto_21_shutdown_states_ignore_blinking_until_off);
  RUN_TEST(test_auto_22_reset_priority_and_restart);
  RUN_TEST(test_auto_23_strip_power_loss_while_running_preserves_logic);
  RUN_TEST(test_auto_24_large_hdd_elapsed_interval_saturates_and_decays);
  RUN_TEST(test_auto_25_invalid_power_led_blink_periods_and_stale);
  RUN_TEST(test_auto_26_startup_animation_finish_waits_for_power_led);
  RUN_TEST(test_auto_27_boot_running_and_sleeping_are_separate);
  RUN_TEST(test_auto_28_await_shutdown_timeout_and_off);
  runAuroraTests();
  return UNITY_END();
}
