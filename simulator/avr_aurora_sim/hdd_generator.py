from dataclasses import dataclass, replace
from enum import Enum
import random

class HddMode(str, Enum):
    MANUAL = "Manual"
    LIGHT = "Light"
    MEDIUM = "Medium"
    HEAVY = "Heavy"
    RANDOM = "Random / Quake"

@dataclass
class HddParams:
    activity_rate: int = 35
    pulse_duration_ms: int = 60
    burst_size: int = 3
    randomness: int = 30

PRESETS = {
    HddMode.MANUAL: HddParams(0, 50, 1, 0),
    HddMode.LIGHT: HddParams(12, 45, 1, 20),
    HddMode.MEDIUM: HddParams(45, 70, 4, 35),
    HddMode.HEAVY: HddParams(85, 90, 8, 25),
    HddMode.RANDOM: HddParams(55, 80, 6, 85),
}

@dataclass(frozen=True)
class HddTransition:
    offset_ms: int
    active: bool
    active_edge: bool

class HddGenerator:
    def __init__(self, seed: int = 1, mode: HddMode = HddMode.MANUAL, params: HddParams | None = None):
        self.seed = seed
        self.mode = mode
        self.params = replace(params or PRESETS[mode])
        self.rng = random.Random(seed)
        self.raw = False
        self.remaining_ms = 0
        self._manual_raw = False

    def reset(self, seed: int | None = None) -> None:
        if seed is not None:
            self.seed = seed
        self.rng = random.Random(self.seed)
        self.raw = False
        self.remaining_ms = 0
        self._manual_raw = False

    def set_mode(self, mode: HddMode, reset: bool = True) -> None:
        self.mode = mode
        self.params = replace(PRESETS[mode])
        if reset:
            self.reset()

    def set_params(self, params: HddParams) -> None:
        self.params = replace(params)

    def update(self, dt_ms: int, manual: bool = False) -> tuple[bool, list[HddTransition]]:
        if self.mode is HddMode.MANUAL:
            old = self._manual_raw
            self._manual_raw = bool(manual)
            self.raw = self._manual_raw
            if (not old) and self.raw:
                return self.raw, [HddTransition(0, True, True)]
            if old != self.raw:
                return self.raw, [HddTransition(0, False, False)]
            return self.raw, []

        transitions: list[HddTransition] = []
        elapsed = 0
        while elapsed < dt_ms:
            if self.remaining_ms <= 0:
                old = self.raw
                self.raw = self._next_state()
                self.remaining_ms = self._next_duration(self.raw)
                if self.raw != old:
                    transitions.append(HddTransition(elapsed, self.raw, (not old) and self.raw))
            step = min(dt_ms - elapsed, self.remaining_ms)
            elapsed += step
            self.remaining_ms -= step
        return self.raw, transitions

    def _next_state(self) -> bool:
        if self.mode is HddMode.HEAVY:
            return self.rng.randrange(100) < self.params.activity_rate
        if self.mode is HddMode.RANDOM:
            activity = max(5, min(95, self.params.activity_rate + self.rng.randrange(-self.params.randomness, self.params.randomness + 1)))
            return self.rng.randrange(100) < activity
        return (not self.raw) if self.rng.randrange(100) < self.params.activity_rate else False

    def _next_duration(self, active: bool) -> int:
        base = self.params.pulse_duration_ms if active else self.params.pulse_duration_ms * max(1, self.params.burst_size)
        jitter = (base * self.params.randomness) // 100
        return max(5, base + (self.rng.randrange(-jitter, jitter + 1) if jitter else 0))
