from dataclasses import dataclass

from .base import Effect
from ..diagnostics import Diagnostics
from ..firmware_defaults import (
    AURORA_COLOR_PROGRESS_STEP,
    AURORA_DIFFUSION_CENTER_WEIGHT,
    AURORA_DIFFUSION_SIDE_WEIGHT,
    AURORA_FADE_STEP,
    AURORA_FIXED_STEP_MS,
    AURORA_SPAWN_MAX_COUNT,
    AURORA_SPAWN_MAX_TICKS,
    AURORA_SPAWN_MIN_COUNT,
    AURORA_SPAWN_MIN_TICKS,
    AURORA_TICKS_PER_FADE,
)
from ..model import FrameContext, LedBuffer, RGB


BACKGROUND_COLOR: RGB = (9, 30, 55)
COLOR_1: RGB = (26, 186, 148)
COLOR_2: RGB = (110, 52, 124)

_U8_MAX = 255
_Q8_8_FRACTION_BITS = 8
_Q8_8_MAX = _U8_MAX << _Q8_8_FRACTION_BITS
_FADE_STEP_U8 = AURORA_FADE_STEP
_DIFFUSION_KERNEL = (
    AURORA_DIFFUSION_SIDE_WEIGHT,
    AURORA_DIFFUSION_CENTER_WEIGHT,
    AURORA_DIFFUSION_SIDE_WEIGHT,
)
_DIFFUSION_KERNEL_SUM = sum(_DIFFUSION_KERNEL)
_U32_MASK = 0xFFFFFFFF
# xorshift32 locks at zero, so seed zero is mapped to this documented constant.
_ZERO_SEED_FALLBACK = 0xA341316C


def _clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, int(value)))


def _u8_to_q8_8(value: int) -> int:
    return _clamp(value, 0, _U8_MAX) << _Q8_8_FRACTION_BITS


def _q8_8_to_u8(value: int) -> int:
    return _clamp(value, 0, _Q8_8_MAX) >> _Q8_8_FRACTION_BITS


def _clamp_rgb(color: RGB) -> RGB:
    if not isinstance(color, tuple) or len(color) != 3:
        return (0, 0, 0)
    return tuple(_clamp(channel, 0, _U8_MAX) for channel in color)  # type: ignore[return-value]


@dataclass
class AuroraFieldConfig:
    fixed_step_ms: int = AURORA_FIXED_STEP_MS
    spawn_min_ticks: int = AURORA_SPAWN_MIN_TICKS
    spawn_max_ticks: int = AURORA_SPAWN_MAX_TICKS
    spawn_min_count: int = AURORA_SPAWN_MIN_COUNT
    spawn_max_count: int = AURORA_SPAWN_MAX_COUNT
    ticks_per_fade: int = AURORA_TICKS_PER_FADE
    color_progress_step: int = AURORA_COLOR_PROGRESS_STEP
    background_color: RGB = BACKGROUND_COLOR
    color_1: RGB = COLOR_1
    color_2: RGB = COLOR_2
    seed: int = 1

    def __post_init__(self) -> None:
        self.fixed_step_ms = max(1, int(self.fixed_step_ms))
        self.spawn_min_ticks = max(1, int(self.spawn_min_ticks))
        self.spawn_max_ticks = max(self.spawn_min_ticks, int(self.spawn_max_ticks))
        self.spawn_min_count = _clamp(self.spawn_min_count, 1, _U8_MAX)
        self.spawn_max_count = _clamp(
            self.spawn_max_count,
            self.spawn_min_count,
            _U8_MAX,
        )
        self.ticks_per_fade = _clamp(self.ticks_per_fade, 1, _U8_MAX)
        self.color_progress_step = _clamp(self.color_progress_step, 0, _U8_MAX)
        self.background_color = _clamp_rgb(self.background_color)
        self.color_1 = _clamp_rgb(self.color_1)
        self.color_2 = _clamp_rgb(self.color_2)
        self.seed = int(self.seed) & _U32_MASK


def lerp_u8(a: int, b: int, amount: int) -> int:
    """Linear uint8 lerp: (a * (255 - amount) + b * amount) // 255."""
    a8 = _clamp(a, 0, _U8_MAX)
    b8 = _clamp(b, 0, _U8_MAX)
    amount8 = _clamp(amount, 0, _U8_MAX)
    # The numerator is at most 65,025, so uint16_t is sufficient in C++.
    return (a8 * (_U8_MAX - amount8) + b8 * amount8) // _U8_MAX


def lerp_rgb(color_a: RGB, color_b: RGB, amount: int) -> RGB:
    return (
        lerp_u8(color_a[0], color_b[0], amount),
        lerp_u8(color_a[1], color_b[1], amount),
        lerp_u8(color_a[2], color_b[2], amount),
    )


class AuroraFieldEffect(Effect):
    """Deterministic 1-D flare field with no persistent flare objects."""

    def __init__(self, led_count: int = 56, config: AuroraFieldConfig | None = None) -> None:
        if int(led_count) <= 0:
            raise ValueError("led_count must be positive")
        self.led_count = int(led_count)
        self.config = config or AuroraFieldConfig()

        # Brightness buffers use unsigned Q8.8 values: 0..255 maps to
        # 0..65,280. Python integers model the future uint16_t storage.
        # These are allocated once and then reused by swapping references.
        self._brightness: list[int] = [0] * self.led_count
        self._color_progress: list[int] = [0] * self.led_count
        self._next_brightness: list[int] = [0] * self.led_count
        self._next_color_progress: list[int] = [0] * self.led_count

        self._prng_state = 1
        self._ticks_until_next_spawn = 1
        self._ticks_until_fade = self.config.ticks_per_fade
        self._fixed_step_accumulator_ms = 0
        self.reset(None)

    @property
    def brightness(self) -> tuple[int, ...]:
        return tuple(_q8_8_to_u8(value) for value in self._brightness)

    @property
    def brightness_q8_8(self) -> tuple[int, ...]:
        return tuple(self._brightness)

    @property
    def color_progress(self) -> tuple[int, ...]:
        return tuple(self._color_progress)

    @property
    def next_brightness(self) -> tuple[int, ...]:
        return tuple(_q8_8_to_u8(value) for value in self._next_brightness)

    @property
    def next_brightness_q8_8(self) -> tuple[int, ...]:
        return tuple(self._next_brightness)

    @property
    def next_color_progress(self) -> tuple[int, ...]:
        return tuple(self._next_color_progress)

    @property
    def prng_state(self) -> int:
        return self._prng_state

    @property
    def ticks_until_next_spawn(self) -> int:
        return self._ticks_until_next_spawn

    @property
    def fixed_step_accumulator_ms(self) -> int:
        return self._fixed_step_accumulator_ms

    def reset(self, context: FrameContext | None) -> None:
        del context
        for index in range(self.led_count):
            self._brightness[index] = 0
            self._color_progress[index] = 0
            self._next_brightness[index] = 0
            self._next_color_progress[index] = 0
        self._prng_state = self.config.seed or _ZERO_SEED_FALLBACK
        self._ticks_until_fade = self.config.ticks_per_fade
        self._fixed_step_accumulator_ms = 0
        self._schedule_next_spawn()

    def next_u32(self) -> int:
        value = self._prng_state
        value ^= (value << 13) & _U32_MASK
        value ^= value >> 17
        value ^= (value << 5) & _U32_MASK
        self._prng_state = value & _U32_MASK
        return self._prng_state

    def range_inclusive(self, minimum: int, maximum: int) -> int:
        minimum = int(minimum)
        maximum = int(maximum)
        if maximum < minimum:
            raise ValueError("maximum must not be less than minimum")
        span = maximum - minimum + 1
        return minimum + self.next_u32() % span

    def render(self, context: FrameContext, leds: LedBuffer, diagnostics: Diagnostics) -> None:
        del diagnostics
        if len(leds) != self.led_count:
            raise ValueError("Aurora field and LED buffer sizes differ")

        dt_ms = max(0, int(context.dt_ms))
        self._fixed_step_accumulator_ms += dt_ms
        while self._fixed_step_accumulator_ms >= self.config.fixed_step_ms:
            self._fixed_step_accumulator_ms -= self.config.fixed_step_ms
            self._fixed_tick()

        self._render_rgb(leds)

    def _fixed_tick(self) -> None:
        self._diffuse_tick()
        self._ticks_until_fade -= 1
        apply_fade = self._ticks_until_fade <= 0
        if apply_fade:
            self._ticks_until_fade = self.config.ticks_per_fade
        self._apply_fade_and_color_tick(apply_fade)
        self._brightness, self._next_brightness = self._next_brightness, self._brightness
        self._color_progress, self._next_color_progress = self._next_color_progress, self._color_progress

        self._ticks_until_next_spawn -= 1
        if self._ticks_until_next_spawn <= 0:
            self._spawn_stars()
            self._schedule_next_spawn()

    def _diffuse_tick(self) -> None:
        """Apply the normalized 1/65, 63/65, 1/65 diffusion kernel."""
        for destination in range(self.led_count):
            weighted_brightness = 0
            weighted_progress = 0

            for offset, weight in zip((-1, 0, 1), _DIFFUSION_KERNEL):
                source = destination + offset
                if source < 0 or source >= self.led_count:
                    continue

                contribution = self._brightness[source] * weight
                weighted_brightness += contribution
                weighted_progress += contribution * self._color_progress[source]
            brightness_q8_8 = weighted_brightness // _DIFFUSION_KERNEL_SUM
            self._next_brightness[destination] = min(
                _Q8_8_MAX,
                brightness_q8_8,
            )
            self._next_color_progress[destination] = (
                weighted_progress // weighted_brightness
                if brightness_q8_8 > 0
                else 0
            )

        # Evaluate local maxima only after the complete next-state field exists.
        # Applying the previous result one iteration later keeps all three values
        # used by the comparison unchanged without allocating another buffer.
        previous_is_peak = False
        for destination in range(1, self.led_count - 1):
            current_is_peak = (
                self._next_color_progress[destination]
                > self._next_color_progress[destination - 1]
                and self._next_color_progress[destination]
                > self._next_color_progress[destination + 1]
            )
            if previous_is_peak:
                previous = destination - 1
                self._next_color_progress[previous] = min(
                    _U8_MAX,
                    self._next_color_progress[previous] + 1,
                )
            previous_is_peak = current_is_peak
        if previous_is_peak:
            previous = self.led_count - 2
            self._next_color_progress[previous] = min(
                _U8_MAX,
                self._next_color_progress[previous] + 1,
            )

        # With Q8.8 brightness, the kernel accumulator is at most 4,243,200
        # and the brightness-weighted progress is at most 1,082,016,000.
        # Both scratch values require uint32_t in the future C++ port.

    def _apply_fade_and_color_tick(self, apply_fade: bool) -> None:
        for index in range(self.led_count):
            brightness_q8_8 = self._next_brightness[index]
            if brightness_q8_8 <= 0:
                self._next_brightness[index] = 0
                self._next_color_progress[index] = 0
                continue

            brightness_q8_8 = min(_Q8_8_MAX, brightness_q8_8)
            if apply_fade:
                brightness_q8_8 = max(
                    0,
                    brightness_q8_8 - _u8_to_q8_8(_FADE_STEP_U8),
                )
            self._next_brightness[index] = brightness_q8_8
            self._next_color_progress[index] = (
                min(
                    _U8_MAX,
                    self._next_color_progress[index]
                    + self.config.color_progress_step,
                )
                if brightness_q8_8 > 0
                else 0
            )

    def _spawn_stars(self) -> tuple[int, ...]:
        count = self.range_inclusive(
            self.config.spawn_min_count,
            self.config.spawn_max_count,
        )
        positions: list[int] = []
        for _ in range(count):
            position = self.range_inclusive(0, self.led_count - 1)
            self._brightness[position] = _Q8_8_MAX
            self._color_progress[position] = 0
            positions.append(position)
        return tuple(positions)

    def _schedule_next_spawn(self) -> None:
        self._ticks_until_next_spawn = self.range_inclusive(
            self.config.spawn_min_ticks,
            self.config.spawn_max_ticks,
        )

    def _render_rgb(self, leds: LedBuffer) -> None:
        for index in range(self.led_count):
            flare_color = lerp_rgb(
                self.config.color_1,
                self.config.color_2,
                self._color_progress[index],
            )
            leds[index] = lerp_rgb(
                self.config.background_color,
                flare_color,
                _q8_8_to_u8(self._brightness[index]),
            )
