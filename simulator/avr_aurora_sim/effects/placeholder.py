from .base import Effect
from ..avr_math import clamp_u8, div_trunc, qadd8, scale8, u8
from ..state_types import Transition

class OffEffect(Effect):
    def render(self, context, leds, diagnostics):
        leds.clear()

class StartupPlaceholder(Effect):
    def render(self, context, leds, diagnostics):
        lit = div_trunc(len(leds) * context.transition_progress, 255, diagnostics, "startup lit")
        for i in range(len(leds)):
            leds[i] = (180, 80, 0) if i <= lit else (0, 0, 0)

class ShutdownPlaceholder(Effect):
    def render(self, context, leds, diagnostics):
        radius = div_trunc(len(leds) * (255 - context.transition_progress), 255, diagnostics, "shutdown radius")
        center = len(leds) // 2
        for i in range(len(leds)):
            leds[i] = (160, 32, 0) if abs(i - center) <= radius else (0, 0, 0)

class ResetPlaceholder(Effect):
    def render(self, context, leds, diagnostics):
        pos = div_trunc((len(leds) - 1) * context.transition_progress, 255, diagnostics, "reset pos")
        for i in range(len(leds)):
            leds[i] = (120, 120, 120) if abs(i - pos) <= 1 else (0, 0, 0)

class ForcedShutdownPlaceholder(Effect):
    def render(self, context, leds, diagnostics):
        duration = max(1, context.transition_duration_ms)
        flash_at = max(1, min(context.forced_flash_at_ms, duration))
        elapsed = min(context.transition_elapsed_ms, duration)
        if elapsed <= flash_at:
            ramp = div_trunc(elapsed * 255, flash_at, diagnostics, "forced rise")
            red = qadd8(10, scale8(ramp, 150, diagnostics, "forced rise red"), diagnostics, "forced rise boost")
        else:
            remaining = duration - elapsed
            fall_duration = max(1, duration - flash_at)
            ramp = div_trunc(max(0, remaining) * 255, fall_duration, diagnostics, "forced fall")
            red = scale8(ramp, 160, diagnostics, "forced fall red")
        red = clamp_u8(red, diagnostics, "forced red clamp")
        for i in range(len(leds)):
            leds[i] = (red, 0, 0)

class SleepPlaceholder(Effect):
    def render(self, context, leds, diagnostics):
        p1 = div_trunc(context.now_ms, 1700, diagnostics, "sleep p1") % len(leds)
        p2 = (p1 + 29) % len(leds)
        phase = u8(div_trunc(context.now_ms, 20, diagnostics, "sleep phase"), diagnostics, "sleep phase wrap")
        b = scale8(phase if phase < 128 else 255 - phase, 48, diagnostics, "sleep breath")
        leds[p1] = (0, b, b)
        leds[p2] = (0, 0, scale8(b, 128, diagnostics, "sleep p2"))

class WarnPlaceholder(Effect):
    def render(self, context, leds, diagnostics):
        amber = div_trunc(context.now_ms, 250, diagnostics, "warn blink") & 1
        color = (180, 60, 0) if amber else (180, 0, 0)
        for i in range(len(leds)):
            leds[i] = color
