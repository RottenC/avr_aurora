#include <unity.h>

#include "config.h"
#include "effect_controller.h"
#include "hdd_activity.h"
#include "pc_state.h"
#include "power_led_tracker.h"

namespace {
PcStateMachine makePcStateMachine() {
  return PcStateMachine({Config::PowerHoldForcedMs, Config::StartingTimeoutMs});
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

void test_off_continuously_reconciles_to_observed_state() {
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

void test_power_button_starts_before_observed_on_reconciliation() {
  PcStateMachine pc = makePcStateMachine();
  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButtonPressed = true;
  const PcStateEvents events = pc.update(inputs, 100);
  TEST_ASSERT_TRUE(events.requestStartup);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting), static_cast<uint8_t>(pc.state()));
}

void test_startup_requires_animation_and_power_confirmation() {
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

void test_startup_waits_for_strip_power_then_requests_once() {
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

void test_strip_power_loss_cancels_and_return_restarts_startup() {
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

void test_startup_times_out_or_reconciles_to_sleep() {
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

void test_short_power_press_cancels_preview_and_starts_shutdown() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  beginPowerHold(pc, 100);

  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButtonReleased = true;
  const PcStateEvents events = pc.update(inputs, 500);
  TEST_ASSERT_TRUE(events.cancelForcedShutdown);
  TEST_ASSERT_TRUE(events.requestShutdown);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::ShuttingDown), static_cast<uint8_t>(pc.state()));
}

void test_forced_hold_latches_and_release_uses_no_normal_shutdown() {
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::ShuttingDown), static_cast<uint8_t>(pc.state()));
}

void test_release_at_forced_threshold_latches_without_intermediate_update() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  beginPowerHold(pc, 100);

  PcStateInputs inputs = makeInputs(PowerLedMode::On);
  inputs.powerButtonReleased = true;
  const PcStateEvents events = pc.update(inputs, 100 + Config::PowerHoldForcedMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::ShuttingDown), static_cast<uint8_t>(pc.state()));
  TEST_ASSERT_TRUE(pc.forcedLatched());
  TEST_ASSERT_FALSE(events.requestShutdown);
  TEST_ASSERT_FALSE(events.cancelForcedShutdown);
}

void test_power_mode_reconciliation_does_not_interrupt_hold() {
  PcStateMachine pc = makePcStateMachine();
  reconcileToRunning(pc);
  beginPowerHold(pc, 100);

  PcStateInputs inputs = makeInputs(PowerLedMode::Blinking);
  inputs.powerButton = true;
  pc.update(inputs, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_sleep_wake_and_shutdown_completion_reconcile_continuously() {
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

void test_transition_priority_restart_and_completion() {
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

void test_forced_transition_has_highest_priority_and_stays_latched() {
  EffectController effects = makeEffectController();
  effects.request(TransitionEffect::Reset, 100);
  effects.request(TransitionEffect::ForcedShutdown, 200);
  effects.request(TransitionEffect::Shutdown, 300);
  effects.update(200 + Config::PowerHoldForcedMs + 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown),
                          static_cast<uint8_t>(effects.current()));
}

void test_effect_cancellation_is_specific_and_state_compatible() {
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

void test_hdd_edges_accumulate_without_changing_fixed_time_ticks() {
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

void test_power_led_tracker_preserves_off_grace_and_blink_timing() {
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

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_off_continuously_reconciles_to_observed_state);
  RUN_TEST(test_power_button_starts_before_observed_on_reconciliation);
  RUN_TEST(test_startup_requires_animation_and_power_confirmation);
  RUN_TEST(test_startup_waits_for_strip_power_then_requests_once);
  RUN_TEST(test_strip_power_loss_cancels_and_return_restarts_startup);
  RUN_TEST(test_startup_times_out_or_reconciles_to_sleep);
  RUN_TEST(test_short_power_press_cancels_preview_and_starts_shutdown);
  RUN_TEST(test_forced_hold_latches_and_release_uses_no_normal_shutdown);
  RUN_TEST(test_release_at_forced_threshold_latches_without_intermediate_update);
  RUN_TEST(test_power_mode_reconciliation_does_not_interrupt_hold);
  RUN_TEST(test_sleep_wake_and_shutdown_completion_reconcile_continuously);
  RUN_TEST(test_transition_priority_restart_and_completion);
  RUN_TEST(test_forced_transition_has_highest_priority_and_stays_latched);
  RUN_TEST(test_effect_cancellation_is_specific_and_state_compatible);
  RUN_TEST(test_hdd_edges_accumulate_without_changing_fixed_time_ticks);
  RUN_TEST(test_power_led_tracker_preserves_off_grace_and_blink_timing);
  return UNITY_END();
}
