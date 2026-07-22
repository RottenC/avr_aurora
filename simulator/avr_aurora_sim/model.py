from dataclasses import dataclass, field
from . import firmware_defaults as defaults
from .avr_math import require_rgb
from .diagnostics import Diagnostics
from .power_led_tracker import PowerLedMode
from .state_types import PcState, Transition

LED_COUNT = 56
RGB = tuple[int, int, int]

@dataclass
class SimulatorConfig:
    led_count: int = LED_COUNT
    frame_interval_ms: int = defaults.FRAME_INTERVAL_MS
    target_fps: int = 1000 // defaults.FRAME_INTERVAL_MS
    power_hold_forced_ms: int = defaults.POWER_HOLD_FORCED_MS
    hdd_update_ms: int = defaults.HDD_UPDATE_MS
    hdd_edge_boost: int = defaults.HDD_EDGE_BOOST
    hdd_active_rise: int = defaults.HDD_ACTIVE_RISE
    hdd_inactive_decay: int = defaults.HDD_INACTIVE_DECAY
    hdd_max: int = defaults.HDD_MAX
    startup_duration_ms: int = defaults.STARTUP_DURATION_MS
    shutdown_duration_ms: int = defaults.SHUTDOWN_DURATION_MS
    reset_duration_ms: int = defaults.RESET_DURATION_MS
    short_power_led_off_ignore_ms: int = defaults.SHORT_POWER_LED_OFF_IGNORE_MS
    power_led_blink_min_half_period_ms: int = defaults.POWER_LED_BLINK_MIN_HALF_PERIOD_MS
    power_led_blink_max_half_period_ms: int = defaults.POWER_LED_BLINK_MAX_HALF_PERIOD_MS
    power_led_blink_stale_ms: int = defaults.POWER_LED_BLINK_STALE_MS
    power_led_blink_edges_required: int = defaults.POWER_LED_BLINK_EDGES_REQUIRED
    starting_timeout_ms: int = defaults.STARTING_TIMEOUT_MS
    shutdown_warning_timeout_ms: int = defaults.SHUTDOWN_WARNING_TIMEOUT_MS

@dataclass
class InputState:
    power_button: bool = False
    reset_button: bool = False
    manual_power_led: bool = False
    manual_hdd_led: bool = False
    strip_power: bool = True

@dataclass
class FrameContext:
    now_ms: int
    dt_ms: int
    frame_number: int
    pc_state: PcState
    transition: Transition
    power_button: bool
    reset_button: bool
    manual_power_led: bool
    manual_hdd_led: bool
    raw_power_led: bool
    power_led_mode: PowerLedMode
    raw_hdd_led: bool
    strip_power: bool
    hdd_activity: int
    transition_started_at_ms: int = 0
    transition_elapsed_ms: int = 0
    transition_duration_ms: int = 0
    transition_progress: int = 0
    preview_elapsed_ms: int = 0
    preview_duration_ms: int = 0
    preview_progress: int = 0

class LedBuffer:
    def __init__(self, diagnostics: Diagnostics, count: int = LED_COUNT, label: str = "led buffer") -> None:
        self._diagnostics = diagnostics
        self._pixels: list[RGB] = [(0, 0, 0)] * count
        self._label = label

    def __len__(self) -> int:
        return len(self._pixels)

    def __getitem__(self, index: int) -> RGB:
        if not isinstance(index, int) or not 0 <= index < len(self._pixels):
            self._diagnostics.record("invalid_led_index", (index,), None, self._label, True)
            return (0, 0, 0)
        return self._pixels[index]

    def __setitem__(self, index: int, rgb: RGB) -> None:
        if not isinstance(index, int) or not 0 <= index < len(self._pixels):
            self._diagnostics.record("invalid_led_index", (index,), None, self._label, True)
            return
        self._pixels[index] = require_rgb(rgb, self._diagnostics, f"{self._label}[{index}]")

    def clear(self) -> None:
        for index in range(len(self._pixels)):
            self._pixels[index] = (0, 0, 0)

    def to_list(self) -> list[RGB]:
        return list(self._pixels)

@dataclass
class SimulationState:
    now_ms: int = 0
    frame_number: int = 0
    pc_state: PcState = PcState.OFF
    transition: Transition = Transition.NONE
    hdd_activity: int = 0
    hdd_pending_ms: int = 0
    raw_power_led: bool = False
    power_led_mode: PowerLedMode = PowerLedMode.OFF
    raw_hdd_led: bool = False
    active_edges_this_frame: int = 0
    preview_started_at_ms: int = 0
    power_hold_ms: int = 0
    random_seed: int = 1
    last_context: FrameContext | None = None
