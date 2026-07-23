from collections import deque
from dataclasses import dataclass

@dataclass(frozen=True)
class TimelineSample:
    now_ms: int
    raw_power_led: bool
    power_led_mode: str
    raw_hdd_led: bool
    hdd_activity: int
    pc_state: str
    transition: str

class TimelineHistory:
    def __init__(self, window_ms: int = 15000, bucket_ms: int = 20, max_samples: int | None = None) -> None:
        self.window_ms = window_ms
        self.bucket_ms = bucket_ms
        self.max_samples = max_samples or max(128, (window_ms // max(1, bucket_ms)) + 64)
        self.samples = deque(maxlen=self.max_samples)
        self._last_sample: TimelineSample | None = None

    def clear(self) -> None:
        self.samples.clear()
        self._last_sample = None

    def add_sample(self, sample: TimelineSample) -> bool:
        changed = self._last_sample is None or (
            sample.raw_power_led,
            sample.power_led_mode,
            sample.raw_hdd_led,
            sample.pc_state,
            sample.transition,
        ) != (
            self._last_sample.raw_power_led,
            self._last_sample.power_led_mode,
            self._last_sample.raw_hdd_led,
            self._last_sample.pc_state,
            self._last_sample.transition,
        )
        bucket_due = self._last_sample is None or sample.now_ms - self._last_sample.now_ms >= self.bucket_ms
        if changed or bucket_due:
            self.samples.append(sample)
            self._last_sample = sample
            self._trim(sample.now_ms)
            return True
        self._trim(sample.now_ms)
        return False

    def _trim(self, now_ms: int) -> None:
        cutoff = now_ms - self.window_ms
        while len(self.samples) > 1 and self.samples[0].now_ms < cutoff:
            self.samples.popleft()
