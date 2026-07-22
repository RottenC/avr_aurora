from dataclasses import dataclass, field
from .avr_math import require_rgb
from .diagnostics import Diagnostics
from .power_led_tracker import PowerLedMode
from .state_types import PcState, Transition

LED_COUNT = 56
RGB = tuple[int, int, int]

@dataclass
class SimulatorConfig:
    led_count: int = LED_COUNT
    frame_interval_ms: int = 20
    target_fps: int = 50
    power_hold_forced_ms: int = 4000
    hdd_update_ms: int = 10
    hdd_edge_boost: int = 20
    hdd_active_rise: int = 3
    hdd_inactive_decay: int = 2
    hdd_max: int = 128
    startup_duration_ms: int = 2200
    shutdown_duration_ms: int = 1800
    reset_duration_ms: int = 900

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
