#include <unity.h>

#include "avr/strip_power_safety.h"
#include "core/aurora_runtime.h"

namespace {

constexpr uint16_t kRestoreConfirmMs = 25;

void test_strip_power_raw_loss_is_immediately_safe() {
  StripPowerSafety safety(kRestoreConfirmMs);
  safety.reset(true, true, 0);
  StripPowerSafetyResult result =
      safety.update(true, true, kRestoreConfirmMs);
  TEST_ASSERT_TRUE(result.outputAllowed);
  TEST_ASSERT_TRUE(result.requestFrameUpdate);

  result = safety.update(false, true, kRestoreConfirmMs + 1);
  TEST_ASSERT_TRUE(result.safeStateRequired);
  TEST_ASSERT_FALSE(result.outputAllowed);
  TEST_ASSERT_FALSE(result.requestFrameUpdate);
}

void test_strip_power_restore_waits_and_requests_fresh_frame() {
  StripPowerSafety safety(kRestoreConfirmMs);
  safety.reset(false, false, 100);

  StripPowerSafetyResult result = safety.update(true, false, 110);
  TEST_ASSERT_TRUE(result.safeStateRequired);
  TEST_ASSERT_FALSE(result.outputAllowed);

  result = safety.update(true, true, 110 + kRestoreConfirmMs - 1);
  TEST_ASSERT_TRUE(result.safeStateRequired);
  TEST_ASSERT_FALSE(result.outputAllowed);
  TEST_ASSERT_FALSE(result.requestFrameUpdate);

  result = safety.update(true, true, 110 + kRestoreConfirmMs);
  TEST_ASSERT_FALSE(result.safeStateRequired);
  TEST_ASSERT_TRUE(result.outputAllowed);
  TEST_ASSERT_TRUE(result.requestFrameUpdate);

  result = safety.update(true, true, 110 + kRestoreConfirmMs + 1);
  TEST_ASSERT_TRUE(result.outputAllowed);
  TEST_ASSERT_FALSE(result.requestFrameUpdate);
}

void test_strip_power_short_raw_dropout_still_requires_confirmation() {
  StripPowerSafety safety(kRestoreConfirmMs);
  safety.reset(true, true, 0);
  safety.update(true, true, kRestoreConfirmMs);
  safety.update(false, true, 30);

  StripPowerSafetyResult result = safety.update(true, true, 31);
  TEST_ASSERT_FALSE(result.outputAllowed);
  result = safety.update(true, true, 31 + kRestoreConfirmMs);
  TEST_ASSERT_TRUE(result.outputAllowed);
  TEST_ASSERT_TRUE(result.requestFrameUpdate);
}

void test_strip_power_policy_fresh_render_preserves_logical_state() {
  AuroraRuntime runtime;
  runtime.reset(123, 0);

  AuroraInputFrame inputs;
  inputs.powerLed = SignalState::High;
  inputs.stripPowerPresent = true;
  runtime.step(inputs, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running),
                          static_cast<uint8_t>(runtime.snapshot().pcState));

  StripPowerSafety safety(kRestoreConfirmMs);
  safety.reset(true, true, 0);
  safety.update(true, true, kRestoreConfirmMs);
  StripPowerSafetyResult result = safety.update(false, true, 30);
  runtime.step(inputs, 30);
  TEST_ASSERT_TRUE(result.safeStateRequired);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running),
                          static_cast<uint8_t>(runtime.snapshot().pcState));

  result = safety.update(true, true, 31);
  runtime.step(inputs, 31);
  TEST_ASSERT_FALSE(result.outputAllowed);

  result = safety.update(true, true, 31 + kRestoreConfirmMs);
  TEST_ASSERT_TRUE(result.requestFrameUpdate);
  runtime.requestFrameUpdate();
  runtime.step(inputs, 31 + kRestoreConfirmMs);
  TEST_ASSERT_TRUE(runtime.snapshot().frameUpdated);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PcState::Running),
                          static_cast<uint8_t>(runtime.snapshot().pcState));
}

}  // namespace

void runStripPowerSafetyTests() {
  RUN_TEST(test_strip_power_raw_loss_is_immediately_safe);
  RUN_TEST(test_strip_power_restore_waits_and_requests_fresh_frame);
  RUN_TEST(test_strip_power_short_raw_dropout_still_requires_confirmation);
  RUN_TEST(test_strip_power_policy_fresh_render_preserves_logical_state);
}
