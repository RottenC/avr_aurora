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

class TimelineWidget(QWidget):
    def __init__(self, window_ms: int = 15000) -> None:
        super().__init__()
        self.window_ms = window_ms
        self.samples = deque(maxlen=1000)
        self.setMinimumHeight(150)

    def clear(self) -> None:
        self.samples.clear(); self.update()

    def add_sample(self, sample: TimelineSample) -> None:
        self.samples.append(sample)
        cutoff = sample.now_ms - self.window_ms
        while self.samples and self.samples[0].now_ms < cutoff:
            self.samples.popleft()
        self.update()

    def paintEvent(self, _):
        painter = QPainter(self)
        painter.fillRect(self.rect(), Qt.black)
        if not self.samples:
            return
        labels = ["PWR raw", "PWR mode", "HDD raw", "HDD act", "PC", "Trans"]
        row_h = max(18, self.height() // len(labels))
        start = max(0, self.samples[-1].now_ms - self.window_ms)
        span = max(1, self.window_ms)
        for row, label in enumerate(labels):
            y = row * row_h
            painter.setPen(Qt.gray); painter.drawText(2, y + 13, label)
            last_x = 70
            for sample in self.samples:
                x = 70 + int((sample.now_ms - start) * max(1, self.width() - 75) / span)
                color = self._color(row, sample)
                painter.fillRect(last_x, y + 2, max(1, x - last_x + 1), row_h - 4, color)
                last_x = x

    def _color(self, row: int, sample: TimelineSample) -> QColor:
        if row == 0: return QColor(0, 180, 0) if sample.raw_power_led else QColor(20, 40, 20)
        if row == 1: return {"On": QColor(0, 160, 0), "Blinking": QColor(0, 120, 200)}.get(sample.power_led_mode, QColor(40, 40, 40))
        if row == 2: return QColor(200, 200, 0) if sample.raw_hdd_led else QColor(40, 40, 20)
        if row == 3: return QColor(min(255, sample.hdd_activity * 2), min(255, sample.hdd_activity * 2), 0)
        if row == 4: return QColor(abs(hash(sample.pc_state)) % 200, 80, 120)
        return QColor(abs(hash(sample.transition)) % 200, 120, 80)
