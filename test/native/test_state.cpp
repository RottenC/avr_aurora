#include <unity.h>
#include "pc_state.h"
#include "effect_controller.h"
#include "hdd_activity.h"
#include "config_values.h"

namespace {
PcStateInputs inputs(PowerLedMode mode) {
  PcStateInputs in = {true, false, false, false, false, mode, false};
  return in;
}

void enterRunning(PcStateMachine &pc) {
  PcStateInputs in = inputs(PowerLedMode::On);
  pc.update(in, 0);
}
}

void test_boot_reconciles_to_running() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  pc.update(inputs(PowerLedMode::On), 10);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_boot_reconciles_to_sleeping() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::Blinking);
  in.stripPowerPresent = false;
  pc.update(in, 10);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Sleeping), static_cast<uint8_t>(pc.state()));
}

void test_normal_startup_requests_effect_and_completes_after_power_led() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::Off);
  in.powerButtonPressed = true;
  PcStateEvents events = pc.update(in, 100);
  TEST_ASSERT_TRUE(events.startupRequested);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting), static_cast<uint8_t>(pc.state()));
  in.powerButtonPressed = false;
  in.startupAnimationFinished = true;
  in.powerMode = PowerLedMode::On;
  pc.update(in, 100 + FirmwareConfig::StartupDurationMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_startup_waits_for_power_led_after_animation() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::Off);
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  in.startupAnimationFinished = true;
  PcStateEvents events = pc.update(in, 500);
  TEST_ASSERT_FALSE(events.startupRequested);
  TEST_ASSERT_TRUE(pc.startupAnimationFinished());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting), static_cast<uint8_t>(pc.state()));
  in.startupAnimationFinished = false;
  events = pc.update(in, 1000);
  TEST_ASSERT_FALSE(events.startupRequested);
}

void test_strip_power_loss_and_return_restarts_startup_once() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::Off);
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  in.stripPowerPresent = false;
  PcStateEvents events = pc.update(in, 200);
  TEST_ASSERT_TRUE(events.cancelStartupRequested);
  TEST_ASSERT_TRUE(pc.startupWaitingForStripPower());
  in.stripPowerPresent = true;
  events = pc.update(in, 300);
  TEST_ASSERT_TRUE(events.startupRequested);
  events = pc.update(in, 400);
  TEST_ASSERT_FALSE(events.startupRequested);
}

void test_startup_timeout_returns_off() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::Off);
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  pc.update(in, 100 + FirmwareConfig::StartingTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Off), static_cast<uint8_t>(pc.state()));
}

void test_short_power_button_press_cancels_preview_and_requests_shutdown() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  enterRunning(pc);
  PcStateInputs in = inputs(PowerLedMode::On);
  in.powerButton = true;
  in.powerButtonPressed = true;
  PcStateEvents events = pc.update(in, 100);
  TEST_ASSERT_TRUE(events.forcedShutdownRequested);
  in.powerButtonPressed = false;
  in.powerButton = false;
  in.powerButtonReleased = true;
  events = pc.update(in, 500);
  TEST_ASSERT_TRUE(events.forcedShutdownCancelRequested);
  TEST_ASSERT_TRUE(events.shutdownRequested);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::ShuttingDown), static_cast<uint8_t>(pc.state()));
}

void test_four_second_forced_shutdown_latches() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  enterRunning(pc);
  PcStateInputs in = inputs(PowerLedMode::On);
  in.powerButton = true;
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  PcStateEvents events = pc.update(in, 100 + FirmwareConfig::PowerHoldForcedMs);
  TEST_ASSERT_FALSE(events.shutdownRequested);
  TEST_ASSERT_TRUE(pc.forcedLatched());
}

void test_release_after_forced_latch_enters_shutdown_without_normal_effect() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  enterRunning(pc);
  PcStateInputs in = inputs(PowerLedMode::On);
  in.powerButton = true;
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  pc.update(in, 100 + FirmwareConfig::PowerHoldForcedMs);
  in.powerButton = false;
  in.powerButtonReleased = true;
  PcStateEvents events = pc.update(in, 200 + FirmwareConfig::PowerHoldForcedMs);
  TEST_ASSERT_FALSE(events.shutdownRequested);
  TEST_ASSERT_FALSE(events.forcedShutdownCancelRequested);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::ShuttingDown), static_cast<uint8_t>(pc.state()));
}

void test_sleep_and_wake_reconciliation() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  enterRunning(pc);
  pc.update(inputs(PowerLedMode::Blinking), 100);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Sleeping), static_cast<uint8_t>(pc.state()));
  pc.update(inputs(PowerLedMode::On), 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_shutdown_completes_only_after_power_led_off() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  enterRunning(pc);
  PcStateInputs in = inputs(PowerLedMode::On);
  in.powerButton = true;
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  in.powerButton = false;
  in.powerButtonReleased = true;
  pc.update(in, 200);
  in.powerButtonReleased = false;
  pc.update(in, 1000);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::ShuttingDown), static_cast<uint8_t>(pc.state()));
  in.powerMode = PowerLedMode::Off;
  pc.update(in, 1100);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Off), static_cast<uint8_t>(pc.state()));
}

void test_reconciliation_does_not_interrupt_power_hold() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  enterRunning(pc);
  PcStateInputs in = inputs(PowerLedMode::On);
  in.powerButton = true;
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  in.powerMode = PowerLedMode::Blinking;
  pc.update(in, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_transition_priorities_and_same_request_no_reset() {
  EffectController effects;
  effects.request(TransitionEffect::Shutdown, 100);
  effects.request(TransitionEffect::Startup, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Shutdown), static_cast<uint8_t>(effects.current()));
  TEST_ASSERT_EQUAL_UINT32(100, effects.startedAt());
  effects.request(TransitionEffect::Shutdown, 300);
  TEST_ASSERT_EQUAL_UINT32(100, effects.startedAt());
  effects.request(TransitionEffect::ForcedShutdown, 400);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown), static_cast<uint8_t>(effects.current()));
  effects.update(400 + FirmwareConfig::PowerHoldForcedMs + 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown), static_cast<uint8_t>(effects.current()));
}

void test_effect_cancellation_rules() {
  EffectController effects;
  effects.request(TransitionEffect::ForcedShutdown, 100);
  effects.cancel(TransitionEffect::Startup);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown), static_cast<uint8_t>(effects.current()));
  effects.cancel(TransitionEffect::ForcedShutdown);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::None), static_cast<uint8_t>(effects.current()));
  effects.request(TransitionEffect::Startup, 200);
  effects.cancel(TransitionEffect::Startup);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::None), static_cast<uint8_t>(effects.current()));
}

void test_multiple_hdd_edges_accumulate_in_one_interval() {
  HddActivity hdd;
  hdd.update(false, 3, FirmwareConfig::HddUpdateMs);
  TEST_ASSERT_EQUAL_UINT8((FirmwareConfig::HddEdgeBoost * 3) - FirmwareConfig::HddInactiveDecay, hdd.value());
  hdd.update(true, 10, FirmwareConfig::HddUpdateMs);
  TEST_ASSERT_EQUAL_UINT8(FirmwareConfig::HddMax, hdd.value());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_reconciles_to_running);
  RUN_TEST(test_boot_reconciles_to_sleeping);
  RUN_TEST(test_normal_startup_requests_effect_and_completes_after_power_led);
  RUN_TEST(test_startup_waits_for_power_led_after_animation);
  RUN_TEST(test_strip_power_loss_and_return_restarts_startup_once);
  RUN_TEST(test_startup_timeout_returns_off);
  RUN_TEST(test_short_power_button_press_cancels_preview_and_requests_shutdown);
  RUN_TEST(test_four_second_forced_shutdown_latches);
  RUN_TEST(test_release_after_forced_latch_enters_shutdown_without_normal_effect);
  RUN_TEST(test_sleep_and_wake_reconciliation);
  RUN_TEST(test_shutdown_completes_only_after_power_led_off);
  RUN_TEST(test_reconciliation_does_not_interrupt_power_hold);
  RUN_TEST(test_transition_priorities_and_same_request_no_reset);
  RUN_TEST(test_effect_cancellation_rules);
  RUN_TEST(test_multiple_hdd_edges_accumulate_in_one_interval);
  return UNITY_END();
}
