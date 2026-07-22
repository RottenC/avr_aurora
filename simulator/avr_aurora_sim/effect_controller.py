from dataclasses import dataclass
from . import firmware_defaults as defaults
from .avr_math import clamp_u8, div_trunc
from .state_types import PcState, Transition

@dataclass
class EffectControllerConfig:
    startup_duration_ms: int = defaults.STARTUP_DURATION_MS
    shutdown_duration_ms: int = defaults.SHUTDOWN_DURATION_MS
    reset_duration_ms: int = defaults.RESET_DURATION_MS

PRIORITY = {Transition.NONE: 0, Transition.STARTUP: 1, Transition.RESET: 2, Transition.SHUTDOWN: 3, Transition.FORCED_SHUTDOWN: 4}

class EffectController:
    def __init__(self, config: EffectControllerConfig | None = None) -> None:
        self.config = config or EffectControllerConfig()
        self.current = Transition.NONE
        self.finished = Transition.NONE
        self.started_at_ms = 0

    def reset_state(self) -> None:
        self.current = Transition.NONE
        self.finished = Transition.NONE
        self.started_at_ms = 0

    def duration(self, effect: Transition | None = None) -> int:
        effect = self.current if effect is None else effect
        if effect is Transition.STARTUP:
            return self.config.startup_duration_ms
        if effect is Transition.SHUTDOWN:
            return self.config.shutdown_duration_ms
        if effect is Transition.RESET:
            return self.config.reset_duration_ms
        return 0

    def elapsed(self, now_ms: int) -> int:
        return now_ms - self.started_at_ms if self.current is not Transition.NONE else 0

    def progress8(self, now_ms: int) -> int:
        duration = self.duration()
        if duration <= 0:
            return 255 if self.current is Transition.FORCED_SHUTDOWN else 0
        return clamp_u8(div_trunc(min(self.elapsed(now_ms), duration) * 255, duration), None, "transition progress")

    def request(self, effect: Transition, now_ms: int) -> None:
        if effect is Transition.NONE:
            self.cancel_all(); return
        if self.current is effect:
            return
        if PRIORITY[effect] < PRIORITY[self.current]:
            return
        self.restart(effect, now_ms)

    def restart(self, effect: Transition, now_ms: int) -> None:
        if effect is Transition.NONE:
            self.cancel_all(); return
        if PRIORITY[effect] < PRIORITY[self.current]:
            return
        self.current = effect
        self.finished = Transition.NONE
        self.started_at_ms = now_ms

    def cancel(self, effect: Transition) -> None:
        if self.current is effect:
            self.cancel_all()

    def cancel_all(self) -> None:
        self.current = Transition.NONE
        self.finished = Transition.NONE

    def reconcile(self, state: PcState) -> None:
        if not self.is_compatible(self.current, state):
            self.cancel_all()

    def update(self, now_ms: int) -> None:
        if self.current in (Transition.NONE, Transition.FORCED_SHUTDOWN):
            return
        if now_ms - self.started_at_ms >= self.duration(self.current):
            self.finished = self.current
            self.current = Transition.NONE

    def consume_finished(self) -> Transition:
        result = self.finished
        self.finished = Transition.NONE
        return result

    @staticmethod
    def is_compatible(effect: Transition, state: PcState) -> bool:
        if effect is Transition.STARTUP:
            return state is PcState.STARTING
        if effect is Transition.RESET:
            return state is PcState.RUNNING
        if effect is Transition.SHUTDOWN:
            return state in (PcState.AWAIT_SHUTDOWN, PcState.WARN)
        if effect is Transition.FORCED_SHUTDOWN:
            return state in (PcState.RUNNING, PcState.AWAIT_SHUTDOWN)
        return True
