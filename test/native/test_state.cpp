#include <unity.h>
#include "pc_state.h"
#include "effect_controller.h"
#include "config_values.h"

namespace {
PcStateInputs inputs(PowerLedMode mode) {
  PcStateInputs in = {true, false, false, false, false, mode, false};
  return in;
}
}

void test_boot_reconciles_to_running() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::On);
  pc.update(in, 10);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_boot_reconciles_to_sleeping() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::Blinking);
  in.stripPowerPresent = false;
  pc.update(in, 10);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Sleeping), static_cast<uint8_t>(pc.state()));
}

void test_power_button_requests_shutdown() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::On);
  pc.update(in, 0);
  in.powerButton = true;
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  in.powerButton = false;
  in.powerButtonReleased = true;
  const PcStateEvents events = pc.update(in, 500);
  TEST_ASSERT_TRUE(events.shutdownRequested);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::ShuttingDown), static_cast<uint8_t>(pc.state()));
}

void test_long_press_requests_forced_shutdown() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::On);
  pc.update(in, 0);
  in.powerButton = true;
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  const PcStateEvents events = pc.update(in, 100 + FirmwareConfig::PowerHoldForcedMs);
  TEST_ASSERT_TRUE(events.forcedShutdownRequested);
  TEST_ASSERT_TRUE(pc.forcedLatched());
}

void test_startup_timeout_returns_off() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::Off);
  in.powerButtonPressed = true;
  pc.update(in, 100);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting), static_cast<uint8_t>(pc.state()));
  in.powerButtonPressed = false;
  pc.update(in, 100 + FirmwareConfig::StartingTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Off), static_cast<uint8_t>(pc.state()));
}

void test_startup_waits_for_animation_and_power_led() {
  PcStateMachine pc({FirmwareConfig::PowerHoldForcedMs, FirmwareConfig::StartingTimeoutMs});
  PcStateInputs in = inputs(PowerLedMode::Off);
  in.powerButtonPressed = true;
  pc.update(in, 100);
  in.powerButtonPressed = false;
  in.startupAnimationFinished = true;
  pc.update(in, 500);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Starting), static_cast<uint8_t>(pc.state()));
  in.powerMode = PowerLedMode::On;
  pc.update(in, 600);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running), static_cast<uint8_t>(pc.state()));
}

void test_effect_priority_rules() {
  EffectController effects;
  effects.request(TransitionEffect::Shutdown, 100);
  effects.request(TransitionEffect::Startup, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::Shutdown), static_cast<uint8_t>(effects.current()));
  TEST_ASSERT_EQUAL_UINT32(100, effects.startedAt());
  effects.request(TransitionEffect::ForcedShutdown, 300);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown), static_cast<uint8_t>(effects.current()));
  effects.cancel(TransitionEffect::Startup);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::ForcedShutdown), static_cast<uint8_t>(effects.current()));
  effects.cancel();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransitionEffect::None), static_cast<uint8_t>(effects.current()));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_reconciles_to_running);
  RUN_TEST(test_boot_reconciles_to_sleeping);
  RUN_TEST(test_power_button_requests_shutdown);
  RUN_TEST(test_long_press_requests_forced_shutdown);
  RUN_TEST(test_startup_timeout_returns_off);
  RUN_TEST(test_startup_waits_for_animation_and_power_led);
  RUN_TEST(test_effect_priority_rules);
  return UNITY_END();
}
