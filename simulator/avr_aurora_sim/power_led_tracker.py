from dataclasses import dataclass
from enum import Enum
from . import firmware_defaults as defaults

class PowerLedMode(str, Enum):
    OFF = "Off"
    ON = "On"
    BLINKING = "Blinking"

@dataclass
class PowerLedTrackerConfig:
    short_off_ignore_ms: int = defaults.SHORT_POWER_LED_OFF_IGNORE_MS
    blink_min_half_period_ms: int = defaults.POWER_LED_BLINK_MIN_HALF_PERIOD_MS
    blink_max_half_period_ms: int = defaults.POWER_LED_BLINK_MAX_HALF_PERIOD_MS
    blink_stale_ms: int = defaults.POWER_LED_BLINK_STALE_MS
    blink_edges_required: int = defaults.POWER_LED_BLINK_EDGES_REQUIRED

class PowerLedTracker:
    def __init__(self, config: PowerLedTrackerConfig | None = None) -> None:
        self.config = config or PowerLedTrackerConfig()
        self.last = False
        self.initialized = False
        self.seen_on = False
        self.last_change_ms = 0
        self.last_on_ms = 0
        self.last_valid_blink_edge_ms = 0
        self.blink_edges = 0

    def reset(self) -> None:
        self.__init__(self.config)

    def update(self, active: bool, now_ms: int) -> None:
        active = bool(active)
        if not self.initialized:
            self.initialized = True
            self.last = active
            self.last_change_ms = now_ms
            if active:
                self.seen_on = True
                self.last_on_ms = now_ms
            return
        if active:
            self.seen_on = True
            self.last_on_ms = now_ms
        if active != self.last:
            interval = now_ms - self.last_change_ms
            if self.config.blink_min_half_period_ms <= interval <= self.config.blink_max_half_period_ms:
                if self.blink_edges < self.config.blink_edges_required:
                    self.blink_edges += 1
                self.last_valid_blink_edge_ms = now_ms
            else:
                self.blink_edges = 0
            self.last = active
            self.last_change_ms = now_ms

    def mode(self, now_ms: int) -> PowerLedMode:
        if self.blink_edges >= self.config.blink_edges_required and now_ms - self.last_valid_blink_edge_ms <= self.config.blink_stale_ms:
            return PowerLedMode.BLINKING
        if self.last:
            return PowerLedMode.ON
        if self.seen_on and now_ms - self.last_on_ms < self.config.short_off_ignore_ms:
            return PowerLedMode.ON
        return PowerLedMode.OFF
