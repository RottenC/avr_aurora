#include <unity.h>

#include "config.h"
#include "core/portable_math.h"
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
    TEST_ASSERT_EQUAL_UINT16(left.backgroundBrightnessQ8_8(index),
                             right.backgroundBrightnessQ8_8(index));
    TEST_ASSERT_EQUAL_UINT8(left.colorProgress(index),
                            right.colorProgress(index));
    const Aurora::Rgb8 leftPixel = left.pixel(index);
    const Aurora::Rgb8 rightPixel = right.pixel(index);
    TEST_ASSERT_EQUAL_UINT8(leftPixel.r, rightPixel.r);
    TEST_ASSERT_EQUAL_UINT8(leftPixel.g, rightPixel.g);
    TEST_ASSERT_EQUAL_UINT8(leftPixel.b, rightPixel.b);
  }
  TEST_ASSERT_EQUAL_UINT8(left.spawnRateRemainderQ0_8(),
                          right.spawnRateRemainderQ0_8());
  TEST_ASSERT_EQUAL_UINT8(left.activeIgnitionCountForTest(),
                          right.activeIgnitionCountForTest());
  for (uint8_t slot = 0; slot < Config::AuroraIgnitionCapacity; ++slot) {
    const Aurora::Field::IgnitionTestState leftIgnition =
        left.ignitionForTest(slot);
    const Aurora::Field::IgnitionTestState rightIgnition =
        right.ignitionForTest(slot);
    TEST_ASSERT_EQUAL(leftIgnition.active, rightIgnition.active);
    TEST_ASSERT_EQUAL_UINT8(leftIgnition.position, rightIgnition.position);
    TEST_ASSERT_EQUAL_UINT8(leftIgnition.peakBrightness,
                            rightIgnition.peakBrightness);
    TEST_ASSERT_EQUAL_UINT8(leftIgnition.durationTicks,
                            rightIgnition.durationTicks);
    TEST_ASSERT_EQUAL_UINT8(leftIgnition.elapsedTicks,
                            rightIgnition.elapsedTicks);
    TEST_ASSERT_EQUAL_UINT8(leftIgnition.radius, rightIgnition.radius);
  }
}

void test_aurora_prng_and_reset_anchors() {
  TEST_ASSERT_EQUAL_HEX32(270369UL, Aurora::xorshift32(1));
  TEST_ASSERT_EQUAL_HEX32(67634689UL, Aurora::xorshift32(270369UL));
  TEST_ASSERT_EQUAL_HEX32(2647435461UL, Aurora::xorshift32(67634689UL));

  Aurora::Field field;
  field.reset(1);
  TEST_ASSERT_EQUAL_HEX32(270369UL, field.prngState());
  const uint8_t spawnSpan = static_cast<uint8_t>(
      Config::AuroraSpawnMaxTicks - Config::AuroraSpawnMinTicks + 1U);
  TEST_ASSERT_EQUAL_UINT8(
      Config::AuroraSpawnMinTicks + Aurora::xorshift32(1) % spawnSpan,
      field.ticksUntilNextSpawn());

  field.advance(1234);
  field.reset(1);
  Aurora::Field clean;
  clean.reset(1);
  assertFieldsEqual(clean, field);

  field.reset(0);
  TEST_ASSERT_EQUAL_HEX32(Aurora::xorshift32(Config::AuroraZeroSeedFallback),
                          field.prngState());
}

void test_aurora_initial_rgb_and_zero_elapsed() {
  Aurora::Field field;
  field.reset(456);
  const Aurora::Field before = field;
  field.advance(0);
  assertFieldsEqual(before, field);
  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    TEST_ASSERT_EQUAL_UINT16(0, field.brightnessQ8_8(index));
    TEST_ASSERT_EQUAL_UINT8(0, field.colorProgress(index));
    assertRgb(field.pixel(index), 0, 0, 0);
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
  TEST_ASSERT_EQUAL_UINT8(Config::AuroraColorProgressStep,
                          center.colorProgress(27));
  TEST_ASSERT_EQUAL_UINT8(Config::AuroraColorProgressStep,
                          center.colorProgress(28));
  TEST_ASSERT_EQUAL_UINT8(Config::AuroraColorProgressStep,
                          center.colorProgress(29));

  Aurora::Field left;
  left.reset(1);
  left.setCellForTest(0, kQ8_8Max, 0);
  left.advance(Config::AuroraFixedStepMs);
  TEST_ASSERT_EQUAL_UINT16(63271, left.brightnessQ8_8(0));
  TEST_ASSERT_EQUAL_UINT16(1004, left.brightnessQ8_8(1));
  TEST_ASSERT_EQUAL_UINT16(0, left.brightnessQ8_8(Config::LedCount - 1));
}

void test_aurora_color_progress_uses_independent_blur() {
  Aurora::Field overlap;
  overlap.reset(1);
  overlap.setCellForTest(27, kQ8_8Max, 0);
  overlap.setCellForTest(28, kQ8_8Max, 128);
  overlap.setCellForTest(29, kQ8_8Max, 255);
  overlap.advance(Config::AuroraFixedStepMs);
  TEST_ASSERT_EQUAL_UINT16(kQ8_8Max, overlap.brightnessQ8_8(28));
  const uint8_t expectedOverlapProgress = static_cast<uint8_t>(
      (128U * Config::AuroraDiffusionCenterWeight +
       255U * Config::AuroraDiffusionSideWeight) /
          Config::AuroraDiffusionKernelSum +
      Config::AuroraColorProgressStep);
  TEST_ASSERT_EQUAL_UINT8(expectedOverlapProgress,
                          overlap.colorProgress(28));

  Aurora::Field peak;
  peak.reset(1);
  peak.setCellForTest(27, kQ8_8Max, 0);
  peak.setCellForTest(28, kQ8_8Max, 128);
  peak.setCellForTest(29, kQ8_8Max, 0);
  peak.advance(Config::AuroraFixedStepMs);
  const uint8_t expectedPeakSideProgress = static_cast<uint8_t>(
      (128U * Config::AuroraDiffusionSideWeight) /
          Config::AuroraDiffusionKernelSum +
      Config::AuroraColorProgressStep);
  const uint8_t expectedPeakCenterProgress = static_cast<uint8_t>(
      (128U * Config::AuroraDiffusionCenterWeight) /
          Config::AuroraDiffusionKernelSum +
      Config::AuroraColorProgressStep);
  TEST_ASSERT_EQUAL_UINT8(expectedPeakSideProgress,
                          peak.colorProgress(27));
  TEST_ASSERT_EQUAL_UINT8(expectedPeakCenterProgress,
                          peak.colorProgress(28));
  TEST_ASSERT_EQUAL_UINT8(expectedPeakSideProgress,
                          peak.colorProgress(29));

  Aurora::Field rgb;
  rgb.reset(1);
  rgb.setCellForTest(0, kQ8_8Max, 0);
  assertRgb(rgb.pixel(0), 26, 186, 148);
  rgb.setCellForTest(0, kQ8_8Max, UINT8_MAX);
  assertRgb(rgb.pixel(0), 110, 52, 124);
  rgb.setCellForTest(0, static_cast<uint16_t>(128) << 8, 128);
  assertRgb(rgb.pixel(0), 34, 59, 67);
}

void test_aurora_color_progress_survives_flare_fade_to_zero() {
  Aurora::Field field;
  field.reset(1);
  field.setSpawnCountdownForTest(UINT8_MAX);
  field.setCellForTest(
      28, static_cast<uint16_t>(Config::AuroraFadeStep) << 8, 42);

  field.advance(Config::AuroraFixedStepMs * Config::AuroraTicksPerFade);

  TEST_ASSERT_EQUAL_UINT16(0, field.brightnessQ8_8(28));
  TEST_ASSERT_GREATER_THAN_UINT8(0, field.colorProgress(28));
}

void test_aurora_ignition_starts_without_immediate_brightness() {
  Aurora::Field field;
  field.reset(1);
  field.setSpawnCountdownForTest(UINT8_MAX);
  field.setCellForTest(28, 0, 77);

  field.spawnIgnitionForTest(28, 200, 30, 2);

  TEST_ASSERT_EQUAL_UINT16(0, field.brightnessQ8_8(28));
  TEST_ASSERT_EQUAL_UINT8(77, field.colorProgress(28));
  TEST_ASSERT_EQUAL_UINT8(1, field.activeIgnitionCountForTest());
}

void test_aurora_ignition_target_curve_is_monotonic_and_exact() {
  constexpr uint8_t durationTicks = 70;
  constexpr uint8_t peakBrightness = 200;
  uint8_t previousEase = 0;
  uint16_t previousBrightness = 0;

  TEST_ASSERT_EQUAL_UINT8(
      0, Aurora::Field::ignitionEaseForTest(0, durationTicks));
  for (uint8_t elapsed = 1; elapsed <= durationTicks; ++elapsed) {
    const uint8_t ease =
        Aurora::Field::ignitionEaseForTest(elapsed, durationTicks);
    const uint16_t brightness =
        Aurora::Field::ignitionTargetBrightnessForTest(peakBrightness, ease);
    TEST_ASSERT_TRUE(ease >= previousEase);
    TEST_ASSERT_TRUE(brightness >= previousBrightness);
    previousEase = ease;
    previousBrightness = brightness;
  }

  TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, previousEase);
  TEST_ASSERT_EQUAL_UINT16(
      static_cast<uint16_t>(peakBrightness) << 8, previousBrightness);

  Aurora::Field field;
  field.reset(1);
  field.spawnIgnitionForTest(28, peakBrightness, durationTicks, 2);
  for (uint8_t tick = 0; tick < durationTicks; ++tick) {
    field.advanceIgnitionsForTest();
  }
  TEST_ASSERT_EQUAL_UINT16(
      static_cast<uint16_t>(peakBrightness) << 8,
      field.brightnessQ8_8(28));
  TEST_ASSERT_EQUAL_UINT8(0, field.activeIgnitionCountForTest());
}

void test_aurora_ignition_compensates_diffusion_and_fade() {
  constexpr uint8_t durationTicks = 30;
  constexpr uint8_t peakBrightness = 200;

  Aurora::Field field;
  field.reset(1);
  field.setSpawnCountdownForTest(UINT8_MAX);
  field.spawnIgnitionForTest(28, peakBrightness, durationTicks, 2);
  for (uint8_t tick = 0; tick < durationTicks; ++tick) {
    field.advance(Config::AuroraFixedStepMs);
  }

  TEST_ASSERT_EQUAL_UINT16(
      static_cast<uint16_t>(peakBrightness) << 8,
      field.brightnessQ8_8(28));
  TEST_ASSERT_EQUAL_UINT8(0, field.activeIgnitionCountForTest());
}

void test_aurora_ignition_color_profile_and_open_boundaries() {
  TEST_ASSERT_EQUAL_UINT8(
      255, Aurora::Field::ignitionSpatialWeightForTest(1, 0));
  TEST_ASSERT_EQUAL_UINT8(
      128, Aurora::Field::ignitionSpatialWeightForTest(1, 1));
  TEST_ASSERT_EQUAL_UINT8(
      255, Aurora::Field::ignitionSpatialWeightForTest(2, 0));
  TEST_ASSERT_EQUAL_UINT8(
      189, Aurora::Field::ignitionSpatialWeightForTest(2, 1));
  TEST_ASSERT_EQUAL_UINT8(
      66, Aurora::Field::ignitionSpatialWeightForTest(2, 2));
  TEST_ASSERT_EQUAL_UINT8(
      255, Aurora::Field::ignitionSpatialWeightForTest(3, 0));
  TEST_ASSERT_EQUAL_UINT8(
      215, Aurora::Field::ignitionSpatialWeightForTest(3, 1));
  TEST_ASSERT_EQUAL_UINT8(
      128, Aurora::Field::ignitionSpatialWeightForTest(3, 2));
  TEST_ASSERT_EQUAL_UINT8(
      40, Aurora::Field::ignitionSpatialWeightForTest(3, 3));
  TEST_ASSERT_EQUAL_UINT8(
      0, Aurora::Field::ignitionSpatialWeightForTest(3, 4));

  Aurora::Field center;
  center.reset(1);
  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    center.setCellForTest(index, 0, UINT8_MAX);
  }
  center.spawnIgnitionForTest(28, 0, 4, 3);
  for (uint8_t tick = 0; tick < 4; ++tick) {
    center.advanceIgnitionsForTest();
  }
  TEST_ASSERT_EQUAL_UINT8(0, center.colorProgress(28));
  TEST_ASSERT_EQUAL_UINT8(40, center.colorProgress(27));
  TEST_ASSERT_EQUAL_UINT8(40, center.colorProgress(29));
  TEST_ASSERT_EQUAL_UINT8(127, center.colorProgress(26));
  TEST_ASSERT_EQUAL_UINT8(127, center.colorProgress(30));
  TEST_ASSERT_EQUAL_UINT8(215, center.colorProgress(25));
  TEST_ASSERT_EQUAL_UINT8(215, center.colorProgress(31));
  TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, center.colorProgress(24));

  Aurora::Field edge;
  edge.reset(1);
  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    edge.setCellForTest(index, 0, UINT8_MAX);
  }
  edge.spawnIgnitionForTest(0, 0, 4, 3);
  for (uint8_t tick = 0; tick < 4; ++tick) {
    edge.advanceIgnitionsForTest();
  }
  TEST_ASSERT_EQUAL_UINT8(0, edge.colorProgress(0));
  TEST_ASSERT_EQUAL_UINT8(40, edge.colorProgress(1));
  TEST_ASSERT_EQUAL_UINT8(127, edge.colorProgress(2));
  TEST_ASSERT_EQUAL_UINT8(215, edge.colorProgress(3));
  TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, edge.colorProgress(4));
  TEST_ASSERT_EQUAL_UINT8(UINT8_MAX,
                          edge.colorProgress(Config::LedCount - 1));
}

void test_aurora_ignition_pool_evicts_oldest_and_saturates() {
  Aurora::Field pool;
  pool.reset(1);
  for (uint8_t slot = 0; slot < Config::AuroraIgnitionCapacity; ++slot) {
    pool.spawnIgnitionForTest(slot, 128, 30, 1);
  }
  TEST_ASSERT_EQUAL_UINT8(Config::AuroraIgnitionCapacity,
                          pool.activeIgnitionCountForTest());
  pool.advanceIgnitionsForTest();
  const uint16_t brightnessBeforeEviction = pool.brightnessQ8_8(0);
  TEST_ASSERT_GREATER_THAN_UINT16(0, brightnessBeforeEviction);
  pool.spawnIgnitionForTest(Config::AuroraIgnitionCapacity, 200, 30, 3);
  TEST_ASSERT_EQUAL_UINT8(Config::AuroraIgnitionCapacity,
                          pool.activeIgnitionCountForTest());
  const Aurora::Field::IgnitionTestState replaced = pool.ignitionForTest(0);
  TEST_ASSERT_TRUE(replaced.active);
  TEST_ASSERT_EQUAL_UINT8(Config::AuroraIgnitionCapacity,
                          replaced.position);
  TEST_ASSERT_EQUAL_UINT8(200, replaced.peakBrightness);
  TEST_ASSERT_EQUAL_UINT8(30, replaced.durationTicks);
  TEST_ASSERT_EQUAL_UINT8(3, replaced.radius);
  for (uint8_t tick = 0; tick < 30; ++tick) {
    pool.advanceIgnitionsForTest();
  }
  TEST_ASSERT_EQUAL_UINT16(brightnessBeforeEviction,
                           pool.brightnessQ8_8(0));

  Aurora::Field saturated;
  saturated.reset(1);
  saturated.spawnIgnitionForTest(28, UINT8_MAX, 1, 1);
  saturated.spawnIgnitionForTest(28, UINT8_MAX, 1, 1);
  saturated.advanceIgnitionsForTest();
  TEST_ASSERT_EQUAL_UINT16(kQ8_8Max, saturated.brightnessQ8_8(28));

  pool.reset(1);
  TEST_ASSERT_EQUAL_UINT8(0, pool.activeIgnitionCountForTest());
}

void test_aurora_hdd_background_advances_color_progress() {
  Aurora::Field idle;
  Aurora::Field active;
  idle.reset(1);
  active.reset(1);

  idle.advance(Config::AuroraFixedStepMs, 0, Config::AuroraFixedStepMs);
  active.advance(Config::AuroraFixedStepMs, Config::HddMax,
                 Config::AuroraFixedStepMs);

  TEST_ASSERT_EQUAL_UINT16(0, active.brightnessQ8_8(0));
  TEST_ASSERT_GREATER_THAN_UINT16(0, active.backgroundBrightnessQ8_8(0));
  TEST_ASSERT_EQUAL_UINT8(0, idle.colorProgress(0));
  TEST_ASSERT_EQUAL_UINT8(Config::AuroraColorProgressStep,
                          active.colorProgress(0));

  active.advance(Config::AuroraFixedStepMs, 0,
                 Config::AuroraFixedStepMs * 2U);
  TEST_ASSERT_GREATER_THAN_UINT16(0, active.backgroundBrightnessQ8_8(0));
  TEST_ASSERT_EQUAL_UINT8(Config::AuroraColorProgressStep * 2U,
                          active.colorProgress(0));

  active.advance(Config::AuroraHddBackgroundReleaseMs, 0,
                 Config::AuroraFixedStepMs * 2U +
                     Config::AuroraHddBackgroundReleaseMs);
  TEST_ASSERT_EQUAL_UINT16(0, active.backgroundBrightnessQ8_8(0));
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

void test_aurora_hdd_background_is_separate_and_composited_with_max() {
  TEST_ASSERT_EQUAL_UINT8(168, Aurora::hash8(0));
  TEST_ASSERT_EQUAL_UINT8(139, Aurora::hash8(1));
  TEST_ASSERT_EQUAL_UINT8(0, Aurora::tri8(0));
  TEST_ASSERT_EQUAL_UINT8(254, Aurora::tri8(127));
  TEST_ASSERT_EQUAL_UINT8(254, Aurora::tri8(128));
  TEST_ASSERT_EQUAL_UINT8(0, Aurora::tri8(UINT8_MAX));

  Aurora::Field field;
  field.reset(1);
  field.advance(0, Config::HddMax, 0);

  const uint32_t baseBackground =
      static_cast<uint16_t>(Config::AuroraHddBackgroundMaxBrightness) << 8;
  for (uint8_t index = 0; index < Config::LedCount; ++index) {
    const uint16_t cell = static_cast<uint16_t>(index) << 8;
    const uint8_t a = Aurora::hash8(static_cast<uint8_t>(cell >> 8));
    const uint8_t b =
        Aurora::hash8(static_cast<uint8_t>((cell >> 8) + 1U));
    const uint8_t c = Aurora::tri8(static_cast<uint8_t>(
        index * Config::AuroraHddBackgroundWaveLedScale));
    const uint8_t result = static_cast<uint8_t>(
        (static_cast<uint16_t>(a) + b + c) / 3U);
    const uint32_t pulsedBackground = (baseBackground * result) >> 8;
    const uint16_t expectedBackground = static_cast<uint16_t>(
        pulsedBackground < kQ8_8Max ? pulsedBackground : kQ8_8Max);
    TEST_ASSERT_EQUAL_UINT16(expectedBackground,
                             field.backgroundBrightnessQ8_8(index));
    TEST_ASSERT_EQUAL_UINT16(0, field.brightnessQ8_8(index));
  }
  const uint16_t expectedBackgroundAtZero = field.backgroundBrightnessQ8_8(0);
  const Aurora::Rgb8 backgroundPixel = field.pixel(0);

  field.setCellForTest(0, static_cast<uint16_t>(32) << 8, 0);
  const Aurora::Rgb8 dimFlarePixel = field.pixel(0);
  TEST_ASSERT_EQUAL_UINT8(backgroundPixel.r, dimFlarePixel.r);
  TEST_ASSERT_EQUAL_UINT8(backgroundPixel.g, dimFlarePixel.g);
  TEST_ASSERT_EQUAL_UINT8(backgroundPixel.b, dimFlarePixel.b);
  field.setCellForTest(0, kQ8_8Max, 0);
  assertRgb(field.pixel(0), 26, 186, 148);

  const Aurora::FieldCellDiagnostics diagnostics = field.diagnostics(0);
  TEST_ASSERT_EQUAL_UINT16(kQ8_8Max, diagnostics.brightnessQ8_8);
  TEST_ASSERT_EQUAL_UINT16(expectedBackgroundAtZero,
                           diagnostics.backgroundBrightnessQ8_8);
  TEST_ASSERT_EQUAL_UINT8(0, diagnostics.colorProgress);
  assertRgb(diagnostics.color, 26, 186, 148);

  field.advance(0, UINT8_MAX, 0);
  TEST_ASSERT_EQUAL_UINT16(expectedBackgroundAtZero,
                           field.backgroundBrightnessQ8_8(0));

  field.advance(0, Config::HddMax, Config::AuroraFixedStepMs * 2U);
  TEST_ASSERT_NOT_EQUAL(expectedBackgroundAtZero,
                        field.backgroundBrightnessQ8_8(0));
}

void test_aurora_hdd_doubles_spawn_rate_at_maximum() {
  Aurora::Field idle;
  Aurora::Field half;
  Aurora::Field saturated;
  idle.reset(1);
  half.reset(1);
  saturated.reset(1);

  const uint8_t initialCountdown = idle.ticksUntilNextSpawn();

  idle.advance(Config::AuroraFixedStepMs * 2U, 0);
  half.advance(Config::AuroraFixedStepMs * 2U, Config::HddMax / 2);
  saturated.advance(Config::AuroraFixedStepMs * 2U, Config::HddMax);
  TEST_ASSERT_EQUAL_UINT8(initialCountdown - 2U,
                          idle.ticksUntilNextSpawn());
  TEST_ASSERT_EQUAL_UINT8(initialCountdown - 3U,
                          half.ticksUntilNextSpawn());
  TEST_ASSERT_EQUAL_UINT8(initialCountdown - 4U,
                          saturated.ticksUntilNextSpawn());

  saturated.reset(1);
  saturated.setSpawnCountdownForTest(4);
  const uint32_t prngBeforeSpawn = saturated.prngState();
  saturated.advance(Config::AuroraFixedStepMs * 2U, Config::HddMax);
  TEST_ASSERT_NOT_EQUAL(prngBeforeSpawn, saturated.prngState());
  TEST_ASSERT_GREATER_THAN_UINT8(0,
                                 saturated.activeIgnitionCountForTest());
}

void test_aurora_seeded_ignitions_are_deterministic_and_bounded() {
  Aurora::Field left;
  Aurora::Field right;
  left.reset(1);
  right.reset(1);

  for (uint16_t tick = 0; tick < 600; ++tick) {
    left.advance(Config::AuroraFixedStepMs);
    right.advance(Config::AuroraFixedStepMs);
    assertFieldsEqual(left, right);

    for (uint8_t slot = 0; slot < Config::AuroraIgnitionCapacity; ++slot) {
      const Aurora::Field::IgnitionTestState ignition =
          left.ignitionForTest(slot);
      if (!ignition.active) continue;
      TEST_ASSERT_TRUE(
          ignition.peakBrightness >=
          Config::AuroraIgnitionMinPeakBrightness);
      TEST_ASSERT_TRUE(
          ignition.peakBrightness <=
          Config::AuroraIgnitionMaxPeakBrightness);
      TEST_ASSERT_TRUE(
          ignition.durationTicks >=
          Config::AuroraIgnitionMinDurationMs / Config::AuroraFixedStepMs);
      TEST_ASSERT_TRUE(
          ignition.durationTicks <=
          Config::AuroraIgnitionMaxDurationMs / Config::AuroraFixedStepMs);
      TEST_ASSERT_TRUE(ignition.radius >= Config::AuroraIgnitionMinRadius);
      TEST_ASSERT_TRUE(ignition.radius <= Config::AuroraIgnitionMaxRadius);
    }
  }
}

}  // namespace

void runAuroraTests() {
  RUN_TEST(test_aurora_prng_and_reset_anchors);
  RUN_TEST(test_aurora_initial_rgb_and_zero_elapsed);
  RUN_TEST(test_aurora_diffusion_uses_open_out_of_place_boundaries);
  RUN_TEST(test_aurora_color_progress_uses_independent_blur);
  RUN_TEST(test_aurora_color_progress_survives_flare_fade_to_zero);
  RUN_TEST(test_aurora_ignition_starts_without_immediate_brightness);
  RUN_TEST(test_aurora_ignition_target_curve_is_monotonic_and_exact);
  RUN_TEST(test_aurora_ignition_compensates_diffusion_and_fade);
  RUN_TEST(test_aurora_ignition_color_profile_and_open_boundaries);
  RUN_TEST(test_aurora_ignition_pool_evicts_oldest_and_saturates);
  RUN_TEST(test_aurora_hdd_background_advances_color_progress);
  RUN_TEST(test_aurora_fixed_step_grouping_matches);
  RUN_TEST(test_aurora_hdd_background_is_separate_and_composited_with_max);
  RUN_TEST(test_aurora_hdd_doubles_spawn_rate_at_maximum);
  RUN_TEST(test_aurora_seeded_ignitions_are_deterministic_and_bounded);
}
