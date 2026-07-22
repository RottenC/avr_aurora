from .base import Effect
from ..avr_math import qadd8, scale8, u8
class PlaceholderEffect(Effect):
    def render(self, context, leds, diagnostics):
        phase = u8(context.frame_number * 3 + scale8(context.hdd_activity, 32, diagnostics, "hdd speed"), diagnostics, "phase")
        bright = qadd8(24, scale8(context.hdd_activity, 80, diagnostics, "hdd bright"), diagnostics, "placeholder brightness")
        for i in range(len(leds)):
            wave = u8(i * 9 + phase, diagnostics, "gradient wave")
            r = scale8(wave, bright, diagnostics, "red scale")
            g = scale8(255 - wave, bright, diagnostics, "green scale")
            b = scale8((i * 5) & 0xff, qadd8(8, context.hdd_activity, diagnostics, "blue boost"), diagnostics, "blue scale")
            leds[i] = (r, g, b)
