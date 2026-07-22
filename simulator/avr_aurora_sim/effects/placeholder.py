from .base import Effect
from ..avr_math import clamp_u8, div_trunc, qadd8, scale8, u8
from ..state_types import Transition

class OffEffect(Effect):
    def render(self, context, leds, diagnostics):
        leds.clear()

class AuroraPlaceholder(Effect):
    def render(self, context, leds, diagnostics):
        phase = u8(div_trunc(context.now_ms, 20, diagnostics, "aurora phase") + scale8(context.hdd_activity, 32, diagnostics, "hdd speed"), diagnostics, "aurora phase wrap")
        bright = qadd8(24, scale8(context.hdd_activity, 80, diagnostics, "hdd bright"), diagnostics, "aurora brightness")
        for i in range(len(leds)):
            wave = u8(i * 9 + phase, diagnostics, "aurora wave")
            leds[i] = (0, scale8(255 - wave, bright, diagnostics, "aurora green"), scale8(wave, bright, diagnostics, "aurora blue"))

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
        progress = context.transition_progress
        if context.transition is Transition.FORCED_SHUTDOWN and context.transition_duration_ms:
            progress = clamp_u8(div_trunc(min(context.transition_elapsed_ms, context.transition_duration_ms) * 255, context.transition_duration_ms, diagnostics, "forced progress"), diagnostics, "forced progress clamp")
        red = qadd8(40, scale8(progress, 180, diagnostics, "forced red"), diagnostics, "forced red boost")
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

# Backward-compatible name for older imports/tests.
PlaceholderEffect = AuroraPlaceholder
