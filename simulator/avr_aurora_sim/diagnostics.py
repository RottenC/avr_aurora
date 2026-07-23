from collections import Counter, deque
from dataclasses import dataclass, field
from typing import Deque

DEFAULT_MAX_EVENTS = 500

class StrictDiagnosticError(RuntimeError):
    pass

@dataclass(frozen=True)
class DiagnosticEvent:
    operation: str
    inputs: tuple
    result: object
    label: str = ""
    frame_number: int = 0
    now_ms: int = 0

@dataclass
class Diagnostics:
    strict: bool = False
    max_events: int = DEFAULT_MAX_EVENTS
    frame_number: int = 0
    now_ms: int = 0
    counters: Counter = field(default_factory=Counter)
    revision: int = 0
    events: Deque[DiagnosticEvent] = field(init=False)

    def __post_init__(self) -> None:
        self.events = deque(maxlen=self.max_events)

    def set_context(self, frame_number: int, now_ms: int) -> None:
        self.frame_number = frame_number
        self.now_ms = now_ms

    def record(self, operation: str, inputs=(), result=None, label: str = "", strict_error: bool = False) -> None:
        self.counters[operation] += 1
        event = DiagnosticEvent(operation, tuple(inputs), result, label, self.frame_number, self.now_ms)
        self.events.append(event)
        self.revision += 1
        if self.strict and strict_error:
            raise StrictDiagnosticError(f"{operation}: inputs={event.inputs} result={result} label={label}")

    def clear(self) -> None:
        self.counters.clear()
        self.events.clear()
        self.revision += 1
