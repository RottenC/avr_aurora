from collections import deque
from dataclasses import dataclass
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPainter
from PySide6.QtWidgets import QWidget

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
        self.samples.clear(); self._last_sample = None

    def add_sample(self, sample: TimelineSample) -> bool:
        changed = self._last_sample is None or (sample.raw_power_led, sample.power_led_mode, sample.raw_hdd_led, sample.pc_state, sample.transition) != (self._last_sample.raw_power_led, self._last_sample.power_led_mode, self._last_sample.raw_hdd_led, self._last_sample.pc_state, self._last_sample.transition)
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

class TimelineWidget(QWidget):
    PC_COLORS = {"Off": QColor(30,30,30), "Starting": QColor(180,120,0), "Running": QColor(0,120,120), "Sleeping": QColor(0,40,120), "AwaitShutdown": QColor(120,80,0), "Warn": QColor(180,40,0)}
    TRANSITION_COLORS = {"None": QColor(30,30,30), "Startup": QColor(200,120,0), "Shutdown": QColor(160,40,0), "Reset": QColor(200,200,200), "ForcedShutdown": QColor(200,0,0)}
    def __init__(self, window_ms: int = 15000) -> None:
        super().__init__(); self.history = TimelineHistory(window_ms); self.setMinimumHeight(150)

    def clear(self) -> None:
        self.history.clear(); self.update()

    def add_sample(self, sample: TimelineSample) -> None:
        if self.history.add_sample(sample): self.update()

    @property
    def samples(self): return self.history.samples
    @property
    def window_ms(self): return self.history.window_ms

    def paintEvent(self, _):
        painter = QPainter(self); painter.fillRect(self.rect(), Qt.black)
        if not self.samples: return
        labels = ["PWR raw", "PWR mode", "HDD raw", "HDD act", "PC", "Trans"]
        row_h = max(18, self.height() // len(labels)); start = max(0, self.samples[-1].now_ms - self.window_ms); span = max(1, self.window_ms)
        for row, label in enumerate(labels):
            y = row * row_h; painter.setPen(Qt.gray); painter.drawText(2, y + 13, label); last_x = 70
            for sample in self.samples:
                x = 70 + int((sample.now_ms - start) * max(1, self.width() - 75) / span)
                painter.fillRect(last_x, y + 2, max(1, x - last_x + 1), row_h - 4, self._color(row, sample)); last_x = x

    def _color(self, row: int, sample: TimelineSample) -> QColor:
        if row == 0: return QColor(0, 180, 0) if sample.raw_power_led else QColor(20, 40, 20)
        if row == 1: return {"On": QColor(0, 160, 0), "Blinking": QColor(0, 120, 200), "Off": QColor(40, 40, 40)}[sample.power_led_mode]
        if row == 2: return QColor(200, 200, 0) if sample.raw_hdd_led else QColor(40, 40, 20)
        if row == 3: return QColor(min(255, sample.hdd_activity * 2), min(255, sample.hdd_activity * 2), 0)
        if row == 4: return self.PC_COLORS.get(sample.pc_state, QColor(80,80,80))
        return self.TRANSITION_COLORS.get(sample.transition, QColor(80,80,80))
