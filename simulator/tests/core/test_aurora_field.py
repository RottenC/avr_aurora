from avr_aurora_sim.diagnostics import Diagnostics
from avr_aurora_sim.effects.aurora_field import (
    AuroraFieldConfig,
    AuroraFieldEffect,
    BACKGROUND_COLOR,
    COLOR_1,
    COLOR_2,
    lerp_rgb,
)
from avr_aurora_sim.model import FrameContext, LedBuffer
from avr_aurora_sim.power_led_tracker import PowerLedMode
from avr_aurora_sim.state_types import PcState, Transition


def context(dt_ms: int = 0) -> FrameContext:
    return FrameContext(
        now_ms=dt_ms,
        dt_ms=dt_ms,
        frame_number=1,
        pc_state=PcState.RUNNING,
        transition=Transition.NONE,
        power_button=False,
        reset_button=False,
        manual_power_led=False,
        manual_hdd_led=False,
        raw_power_led=True,
        power_led_mode=PowerLedMode.ON,
        raw_hdd_led=False,
        strip_power=True,
        hdd_activity=0,
    )


def render(effect: AuroraFieldEffect, dt_ms: int = 0) -> list[tuple[int, int, int]]:
    leds = LedBuffer(Diagnostics(), effect.led_count)
    effect.render(context(dt_ms), leds, Diagnostics())
    return leds.to_list()


def snapshot(effect: AuroraFieldEffect):
    return (
        effect.brightness,
        effect.color_progress,
        effect.ticks_until_next_spawn,
        effect.prng_state,
        effect.fixed_step_accumulator_ms,
    )


def state_hash(effect: AuroraFieldEffect, pixels: list[tuple[int, int, int]]) -> int:
    result = 0x811C9DC5

    def add_byte(value: int) -> None:
        nonlocal result
        result = ((result ^ (value & 0xFF)) * 0x01000193) & 0xFFFFFFFF

    for brightness, progress, pixel in zip(
        effect.brightness_q8_8,
        effect.color_progress,
        pixels,
    ):
        add_byte(brightness)
        add_byte(brightness >> 8)
        add_byte(progress)
        for channel in pixel:
            add_byte(channel)
    for shift in (0, 8, 16, 24):
        add_byte(effect.prng_state >> shift)
    add_byte(effect.ticks_until_next_spawn)
    for shift in (0, 8, 16, 24):
        add_byte(effect.fixed_step_accumulator_ms >> shift)
    return result


def test_initial_state_is_clear_and_renders_background():
    effect = AuroraFieldEffect()
    effect.reset(context())
    assert effect.brightness == (0,) * 56
    assert effect.color_progress == (0,) * 56
    assert effect.next_brightness == (0,) * 56
    assert effect.next_color_progress == (0,) * 56
    assert render(effect) == [BACKGROUND_COLOR] * 56


def test_determinism_for_matching_seeds_and_difference_for_another_seed():
    config_a = AuroraFieldConfig(seed=12345)
    config_b = AuroraFieldConfig(seed=12345)
    same_a = AuroraFieldEffect(config=config_a)
    same_b = AuroraFieldEffect(config=config_b)
    different = AuroraFieldEffect(config=AuroraFieldConfig(seed=54321))

    leds_a = leds_b = None
    differed = False
    for _ in range(600):
        leds_a = render(same_a, 20)
        leds_b = render(same_b, 20)
        render(different, 20)
        differed = differed or snapshot(same_a) != snapshot(different)

    assert snapshot(same_a) == snapshot(same_b)
    assert leds_a == leds_b
    assert differed


def test_one_star_diffuses_without_in_place_updates():
    effect = AuroraFieldEffect(config=AuroraFieldConfig(spawn_min_ticks=1000, spawn_max_ticks=1000))
    center = 28
    effect._brightness[center] = 255 << 8
    effect._color_progress[center] = 0

    render(effect, 20)

    assert effect.brightness[center - 1 : center + 2] == (3, 247, 3)
    assert effect.color_progress[center - 1 : center + 2] == (1, 1, 1)


def test_diffusion_has_open_boundaries():
    left = AuroraFieldEffect(config=AuroraFieldConfig(spawn_min_ticks=1000, spawn_max_ticks=1000))
    left._brightness[0] = 255 << 8
    render(left, 20)
    assert left.brightness[0:2] == (247, 3)
    assert left.brightness[-1] == 0

    right = AuroraFieldEffect(config=AuroraFieldConfig(spawn_min_ticks=1000, spawn_max_ticks=1000))
    right._brightness[-1] = 255 << 8
    render(right, 20)
    assert right.brightness[-2:] == (3, 247)
    assert right.brightness[0] == 0


def test_overlap_combines_contributions_and_saturates():
    config = AuroraFieldConfig(
        spawn_min_ticks=1000,
        spawn_max_ticks=1000,
    )
    effect = AuroraFieldEffect(config=config)
    for index, progress in ((27, 0), (28, 128), (29, 255)):
        effect._brightness[index] = 255 << 8
        effect._color_progress[index] = progress

    render(effect, 20)

    assert effect.brightness[28] == 255
    assert effect.color_progress[28] == 128
    assert all(0 <= value <= 255 for value in effect.brightness)
    assert all(0 <= value <= 255 for value in effect.color_progress)


def test_stale_color_is_cleared_and_reused_cell_starts_at_color_1():
    effect = AuroraFieldEffect(
        config=AuroraFieldConfig(
            spawn_min_ticks=1000,
            spawn_max_ticks=1000,
        )
    )
    index = 12
    effect._brightness[index] = 1
    effect._color_progress[index] = 200
    render(effect, 20)
    assert effect.brightness[index] == 0
    assert effect.color_progress[index] == 0

    effect._brightness[index] = 255 << 8
    effect._color_progress[index] = 0
    assert render(effect)[index] == COLOR_1


def test_spawn_counts_positions_and_intervals_stay_inclusive_bounds():
    effect = AuroraFieldEffect(config=AuroraFieldConfig(seed=9876))
    for _ in range(1000):
        positions = effect._spawn_stars()
        assert 1 <= len(positions) <= 3
        assert all(0 <= position < 56 for position in positions)
        effect._schedule_next_spawn()
        assert 20 <= effect.ticks_until_next_spawn <= 70


def test_fixed_step_results_do_not_depend_on_render_grouping():
    def scenario(step_ms: int, calls: int):
        effect = AuroraFieldEffect(config=AuroraFieldConfig(seed=321))
        pixels = None
        for _ in range(calls):
            pixels = render(effect, step_ms)
        return snapshot(effect), pixels

    assert scenario(5, 400) == scenario(20, 100) == scenario(100, 20) == scenario(200, 10)


def test_portable_state_hash_checkpoints_for_cpp_parity():
    expected = {
        0: 0x67F39172,
        1: 0x214C49AF,
        38: 0x035A7A87,
        70: 0xD89B4048,
        100: 0xDBD17793,
        600: 0x38FF6439,
    }
    effect = AuroraFieldEffect(config=AuroraFieldConfig(seed=1))
    previous_tick = 0

    for tick, expected_hash in expected.items():
        pixels = []
        for _ in range(tick - previous_tick):
            pixels = render(effect, 20)
        if not pixels:
            pixels = render(effect, 0)
        assert state_hash(effect, pixels) == expected_hash
        previous_tick = tick


def test_fixed_buffers_are_reused_across_ticks():
    effect = AuroraFieldEffect()
    buffer_ids = {
        id(effect._brightness),
        id(effect._color_progress),
        id(effect._next_brightness),
        id(effect._next_color_progress),
    }
    for _ in range(200):
        render(effect, 20)
    assert {
        id(effect._brightness),
        id(effect._color_progress),
        id(effect._next_brightness),
        id(effect._next_color_progress),
    } == buffer_ids
    assert all(len(values) == 56 for values in (
        effect.brightness,
        effect.color_progress,
        effect.next_brightness,
        effect.next_color_progress,
    ))


def test_zero_time_does_not_advance_field_timer_or_prng():
    effect = AuroraFieldEffect(config=AuroraFieldConfig(seed=456))
    render(effect, 123)
    before = snapshot(effect)
    pixels_before = render(effect)
    assert snapshot(effect) == before
    assert render(effect) == pixels_before
    assert snapshot(effect) == before


def test_rgb_anchors_and_integer_midpoints():
    effect = AuroraFieldEffect()
    index = 0

    effect._brightness[index] = 0
    effect._color_progress[index] = 255
    assert render(effect)[index] == BACKGROUND_COLOR

    effect._brightness[index] = 255 << 8
    effect._color_progress[index] = 0
    assert render(effect)[index] == COLOR_1

    effect._color_progress[index] = 255
    assert render(effect)[index] == COLOR_2

    assert lerp_rgb(COLOR_1, COLOR_2, 128) == (68, 118, 135)
    effect._brightness[index] = 128 << 8
    effect._color_progress[index] = 128
    assert render(effect)[index] == (38, 74, 95)


def test_local_color_peak_uses_complete_next_field_and_saturates():
    effect = AuroraFieldEffect(
        config=AuroraFieldConfig(spawn_min_ticks=1000, spawn_max_ticks=1000)
    )
    center = 28
    for index, progress in ((center - 1, 0), (center, 128), (center + 1, 0)):
        effect._brightness[index] = 255 << 8
        effect._color_progress[index] = progress

    render(effect, 20)

    assert effect.color_progress[center - 1 : center + 2] == (3, 126, 3)


def test_fade_is_applied_once_per_configured_period():
    base_config = dict(
        spawn_min_ticks=1000,
        spawn_max_ticks=1000,
        color_progress_step=0,
    )
    faded = AuroraFieldEffect(
        config=AuroraFieldConfig(ticks_per_fade=10, **base_config)
    )
    unfaded = AuroraFieldEffect(
        config=AuroraFieldConfig(ticks_per_fade=255, **base_config)
    )
    center = 28
    faded._brightness[center] = unfaded._brightness[center] = 255 << 8

    for _ in range(10):
        render(faded, 20)
        render(unfaded, 20)

    assert any(
        without_fade - with_fade == 1 << 8
        for with_fade, without_fade in zip(
            faded.brightness_q8_8,
            unfaded.brightness_q8_8,
        )
    )


def test_zero_seed_uses_reproducible_nonzero_xorshift_state():
    first = AuroraFieldEffect(config=AuroraFieldConfig(seed=0))
    second = AuroraFieldEffect(config=AuroraFieldConfig(seed=0))
    assert first.prng_state != 0
    assert snapshot(first) == snapshot(second)


def test_xorshift32_sequence_has_a_portable_exact_anchor():
    effect = AuroraFieldEffect(config=AuroraFieldConfig(seed=1))
    # reset() consumes the first value (270369) to schedule the first spawn.
    assert effect.prng_state == 270369
    assert effect.next_u32() == 67634689
    assert effect.next_u32() == 2647435461


def test_invalid_tuning_values_are_safely_clamped():
    config = AuroraFieldConfig(
        fixed_step_ms=0,
        spawn_min_ticks=-2,
        spawn_max_ticks=-8,
        spawn_min_count=0,
        spawn_max_count=-1,
        ticks_per_fade=0,
        color_progress_step=-1,
        background_color=(-1, 300, 4),
    )
    assert config.fixed_step_ms == 1
    assert config.spawn_min_ticks == config.spawn_max_ticks == 1
    assert config.spawn_min_count == config.spawn_max_count == 1
    assert config.ticks_per_fade == 1
    assert config.color_progress_step == 0
    assert config.background_color == (0, 255, 4)
