#include <unity.h>

#include "config.h"
#include "effects/aurora_field.h"

namespace {

constexpr uint16_t kQ8_8Max = static_cast<uint16_t>(UINT8_MAX) << 8;

void assertRgb(const Aurora::Rgb8 &actual, uint8_t r, uint8_t g, uint8_t b) {
  TEST_ASSERT_EQUAL_UINT8(r, actual.r);
  TEST_ASSERT_EQUAL_UINT8(g, actual.g);
  TEST_ASSERT_EQUAL_UINT8(b, actual.b);
}

void assertFieldsEqual(const Aurora::Field &left, const Aurora::Field &right) {
  TEST_ASSERT_EQUAL_UINT32(left.prngState(), right.prngState());
  TEST_ASSERT_EQUAL_UINT8(left.ticksUntilNextSpawn(),
                          right.ticksUntilNextSpawn());
  TEST_ASSERT_EQUAL_UINT32(left.fixedStepAccumulatorMs(),
                           right.fixedStepAccumulatorMs());
  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    TEST_ASSERT_EQUAL_UINT16(left.brightnessQ8_8(index),
                             right.brightnessQ8_8(index));
    TEST_ASSERT_EQUAL_UINT8(left.colorProgress(index),
                            right.colorProgress(index));
    const Aurora::Rgb8 leftPixel = left.pixel(index);
    const Aurora::Rgb8 rightPixel = right.pixel(index);
    TEST_ASSERT_EQUAL_UINT8(leftPixel.r, rightPixel.r);
    TEST_ASSERT_EQUAL_UINT8(leftPixel.g, rightPixel.g);
    TEST_ASSERT_EQUAL_UINT8(leftPixel.b, rightPixel.b);
  }
}

uint32_t addHashByte(uint32_t hash, uint8_t value) {
  return (hash ^ value) * 0x01000193UL;
}

uint32_t stateHash(const Aurora::Field &field) {
  uint32_t hash = 0x811C9DC5UL;
  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    const uint16_t brightness = field.brightnessQ8_8(index);
    const Aurora::Rgb8 pixel = field.pixel(index);
    hash = addHashByte(hash, static_cast<uint8_t>(brightness));
    hash = addHashByte(hash, static_cast<uint8_t>(brightness >> 8));
    hash = addHashByte(hash, field.colorProgress(index));
    hash = addHashByte(hash, pixel.r);
    hash = addHashByte(hash, pixel.g);
    hash = addHashByte(hash, pixel.b);
  }
  const uint32_t prngState = field.prngState();
  for (uint8_t shift = 0; shift < 32; shift += 8) {
    hash = addHashByte(hash, static_cast<uint8_t>(prngState >> shift));
  }
  hash = addHashByte(hash, field.ticksUntilNextSpawn());
  const uint32_t accumulator = field.fixedStepAccumulatorMs();
  for (uint8_t shift = 0; shift < 32; shift += 8) {
    hash = addHashByte(hash, static_cast<uint8_t>(accumulator >> shift));
  }
  return hash;
}

void test_aurora_prng_and_reset_anchors() {
  TEST_ASSERT_EQUAL_HEX32(270369UL, Aurora::xorshift32(1));
  TEST_ASSERT_EQUAL_HEX32(67634689UL, Aurora::xorshift32(270369UL));
  TEST_ASSERT_EQUAL_HEX32(2647435461UL, Aurora::xorshift32(67634689UL));

  Aurora::Field field;
  field.reset(1);
  TEST_ASSERT_EQUAL_HEX32(270369UL, field.prngState());
  TEST_ASSERT_EQUAL_UINT8(38, field.ticksUntilNextSpawn());
  TEST_ASSERT_EQUAL_HEX32(0x67F39172UL, stateHash(field));

  field.advance(1234);
  field.reset(1);
  TEST_ASSERT_EQUAL_HEX32(0x67F39172UL, stateHash(field));

  field.reset(0);
  TEST_ASSERT_EQUAL_HEX32(Aurora::xorshift32(Config::AuroraZeroSeedFallback),
                          field.prngState());
}

void test_aurora_initial_rgb_and_zero_elapsed() {
  Aurora::Field field;
  field.reset(456);
  const uint32_t before = stateHash(field);
  field.advance(0);
  TEST_ASSERT_EQUAL_HEX32(before, stateHash(field));
  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    TEST_ASSERT_EQUAL_UINT16(0, field.brightnessQ8_8(index));
    TEST_ASSERT_EQUAL_UINT8(0, field.colorProgress(index));
    assertRgb(field.pixel(index), 9, 30, 55);
  }
}

void test_aurora_diffusion_uses_open_out_of_place_boundaries() {
  Aurora::Field center;
  center.reset(1);
  center.setCellForTest(28, kQ8_8Max, 0);
  center.advance(Config::AuroraFixedStepMs);
  TEST_ASSERT_EQUAL_UINT16(1004, center.brightnessQ8_8(27));
  TEST_ASSERT_EQUAL_UINT16(63271, center.brightnessQ8_8(28));
  TEST_ASSERT_EQUAL_UINT16(1004, center.brightnessQ8_8(29));
  TEST_ASSERT_EQUAL_UINT8(1, center.colorProgress(27));
  TEST_ASSERT_EQUAL_UINT8(1, center.colorProgress(28));
  TEST_ASSERT_EQUAL_UINT8(1, center.colorProgress(29));

  Aurora::Field left;
  left.reset(1);
  left.setCellForTest(0, kQ8_8Max, 0);
  left.advance(Config::AuroraFixedStepMs);
  TEST_ASSERT_EQUAL_UINT16(63271, left.brightnessQ8_8(0));
  TEST_ASSERT_EQUAL_UINT16(1004, left.brightnessQ8_8(1));
  TEST_ASSERT_EQUAL_UINT16(0, left.brightnessQ8_8(Config::LedCount - 1));
}

void test_aurora_overlap_color_peak_and_rgb_anchors() {
  Aurora::Field overlap;
  overlap.reset(1);
  overlap.setCellForTest(27, kQ8_8Max, 0);
  overlap.setCellForTest(28, kQ8_8Max, 128);
  overlap.setCellForTest(29, kQ8_8Max, 255);
  overlap.advance(Config::AuroraFixedStepMs);
  TEST_ASSERT_EQUAL_UINT16(kQ8_8Max, overlap.brightnessQ8_8(28));
  TEST_ASSERT_EQUAL_UINT8(128, overlap.colorProgress(28));

  Aurora::Field peak;
  peak.reset(1);
  peak.setCellForTest(27, kQ8_8Max, 0);
  peak.setCellForTest(28, kQ8_8Max, 128);
  peak.setCellForTest(29, kQ8_8Max, 0);
  peak.advance(Config::AuroraFixedStepMs);
  TEST_ASSERT_EQUAL_UINT8(3, peak.colorProgress(27));
  TEST_ASSERT_EQUAL_UINT8(126, peak.colorProgress(28));
  TEST_ASSERT_EQUAL_UINT8(3, peak.colorProgress(29));

  Aurora::Field rgb;
  rgb.reset(1);
  rgb.setCellForTest(0, kQ8_8Max, 0);
  assertRgb(rgb.pixel(0), 26, 186, 148);
  rgb.setCellForTest(0, kQ8_8Max, UINT8_MAX);
  assertRgb(rgb.pixel(0), 110, 52, 124);
  rgb.setCellForTest(0, static_cast<uint16_t>(128) << 8, 128);
  assertRgb(rgb.pixel(0), 38, 74, 95);
}

void test_aurora_fixed_step_grouping_matches() {
  Aurora::Field step5;
  Aurora::Field step20;
  Aurora::Field step100;
  Aurora::Field step200;
  step5.reset(321);
  step20.reset(321);
  step100.reset(321);
  step200.reset(321);

  for (uint16_t call = 0; call < 400; ++call) step5.advance(5);
  for (uint8_t call = 0; call < 100; ++call) step20.advance(20);
  for (uint8_t call = 0; call < 20; ++call) step100.advance(100);
  for (uint8_t call = 0; call < 10; ++call) step200.advance(200);

  assertFieldsEqual(step5, step20);
  assertFieldsEqual(step20, step100);
  assertFieldsEqual(step100, step200);
}

void test_aurora_python_parity_checkpoints() {
  struct Checkpoint {
    uint16_t tick;
    uint32_t hash;
  };
  const Checkpoint checkpoints[] = {
      {0, 0x67F39172UL},   {1, 0x214C49AFUL},   {38, 0x035A7A87UL},
      {70, 0xD89B4048UL}, {100, 0xDBD17793UL}, {600, 0x38FF6439UL},
  };

  Aurora::Field field;
  field.reset(1);
  uint16_t previousTick = 0;
  for (const Checkpoint &checkpoint : checkpoints) {
    for (uint16_t tick = previousTick; tick < checkpoint.tick; ++tick) {
      field.advance(Config::AuroraFixedStepMs);
    }
    TEST_ASSERT_EQUAL_HEX32(checkpoint.hash, stateHash(field));
    previousTick = checkpoint.tick;
  }
}

}  // namespace

void runAuroraTests() {
  RUN_TEST(test_aurora_prng_and_reset_anchors);
  RUN_TEST(test_aurora_initial_rgb_and_zero_elapsed);
  RUN_TEST(test_aurora_diffusion_uses_open_out_of_place_boundaries);
  RUN_TEST(test_aurora_overlap_color_peak_and_rgb_anchors);
  RUN_TEST(test_aurora_fixed_step_grouping_matches);
  RUN_TEST(test_aurora_python_parity_checkpoints);
}
