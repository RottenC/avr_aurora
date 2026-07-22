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
        self._pending: list[PowerLedTransition] = []

    def reset(self) -> None:
        self.phase_ms = 0
        self.raw = False
        self._pending.clear()

    def set_mode(self, mode: PowerLedSourceMode, reset_phase: bool = True) -> None:
        if mode is self.mode:
            return
        self.mode = mode
        if mode is PowerLedSourceMode.BLINKING and reset_phase:
            self.phase_ms = 0
            self._queue_reconcile(False)

    def set_half_period_ms(self, value: int, reset_phase: bool = True) -> None:
        value = max(1, int(value))
        if value == self.half_period_ms:
            return
        self.half_period_ms = value
        if self.mode is PowerLedSourceMode.BLINKING and reset_phase:
            self.phase_ms = 0
            self._queue_reconcile(False)

    def update(self, dt_ms: int, manual_power_led: bool) -> tuple[bool, list[PowerLedTransition]]:
        transitions = self._drain_pending()
        if self.mode is PowerLedSourceMode.MANUAL:
            final, more = self._hold(bool(manual_power_led)); return final, transitions + more
        if self.mode is PowerLedSourceMode.OFF:
            final, more = self._hold(False); return final, transitions + more
        if self.mode is PowerLedSourceMode.ON:
            final, more = self._hold(True); return final, transitions + more
        final, more = self._blink(dt_ms)
        return final, transitions + more

    def _queue_reconcile(self, active: bool) -> None:
        self._pending.clear()
        if self.raw != active:
            self._pending.append(PowerLedTransition(0, active))

    def _drain_pending(self) -> list[PowerLedTransition]:
        transitions = self._pending
        self._pending = []
        for transition in transitions:
            self.raw = transition.active
        return transitions

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
