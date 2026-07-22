from dataclasses import dataclass
from enum import Enum

class PowerLedSourceMode(str, Enum):
    MANUAL = "Manual"
    OFF = "Off"
    ON = "On"
    BLINKING = "Blinking"

@dataclass(frozen=True)
class PowerLedTransition:
    offset_ms: int
    active: bool

class PowerLedGenerator:
    """Raw Power LED source.

    Blinking mode starts LOW at phase 0, stays LOW for the first half-period,
    then toggles every half-period. Transitions at offset == dt_ms are emitted by the current update; the
    next update starts after that boundary, avoiding duplicate boundary edges.
    """
    def __init__(self, mode: PowerLedSourceMode = PowerLedSourceMode.MANUAL, half_period_ms: int = 500) -> None:
        self.mode = mode
        self.half_period_ms = half_period_ms
        self.phase_ms = 0
        self.raw = False

    def reset(self) -> None:
        self.phase_ms = 0
        self.raw = False

    def update(self, dt_ms: int, manual_power_led: bool) -> tuple[bool, list[PowerLedTransition]]:
        if self.mode is PowerLedSourceMode.MANUAL:
            return self._hold(bool(manual_power_led))
        if self.mode is PowerLedSourceMode.OFF:
            return self._hold(False)
        if self.mode is PowerLedSourceMode.ON:
            return self._hold(True)
        return self._blink(dt_ms)

    def _hold(self, active: bool) -> tuple[bool, list[PowerLedTransition]]:
        transitions = [] if self.raw == active else [PowerLedTransition(0, active)]
        self.raw = active
        return self.raw, transitions

    def _blink(self, dt_ms: int) -> tuple[bool, list[PowerLedTransition]]:
        half = max(1, self.half_period_ms)
        period = half * 2
        transitions: list[PowerLedTransition] = []
        elapsed = 0
        while elapsed < dt_ms:
            next_boundary = half - (self.phase_ms % half)
            if next_boundary == 0:
                next_boundary = half
            if elapsed + next_boundary > dt_ms:
                self.phase_ms = (self.phase_ms + (dt_ms - elapsed)) % period
                elapsed = dt_ms
            else:
                elapsed += next_boundary
                self.phase_ms = (self.phase_ms + next_boundary) % period
                active = self.phase_ms >= half
                if active != self.raw:
                    self.raw = active
                    transitions.append(PowerLedTransition(elapsed, active))
        self.raw = self.phase_ms >= half
        return self.raw, transitions
