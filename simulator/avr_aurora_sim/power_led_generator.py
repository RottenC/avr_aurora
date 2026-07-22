from enum import Enum

class PowerLedSourceMode(str, Enum):
    MANUAL = "Manual"
    OFF = "Off"
    ON = "On"
    BLINKING = "Blinking"

class PowerLedGenerator:
    def __init__(self, mode: PowerLedSourceMode = PowerLedSourceMode.MANUAL, half_period_ms: int = 500) -> None:
        self.mode = mode
        self.half_period_ms = half_period_ms
        self.phase_ms = 0
        self.raw = False

    def reset(self) -> None:
        self.phase_ms = 0
        self.raw = False

    def update(self, dt_ms: int, manual_power_led: bool) -> bool:
        if self.mode is PowerLedSourceMode.MANUAL:
            self.raw = bool(manual_power_led)
        elif self.mode is PowerLedSourceMode.OFF:
            self.raw = False
        elif self.mode is PowerLedSourceMode.ON:
            self.raw = True
        else:
            period = max(1, self.half_period_ms * 2)
            self.phase_ms = (self.phase_ms + dt_ms) % period
            self.raw = self.phase_ms < self.half_period_ms
        return self.raw
