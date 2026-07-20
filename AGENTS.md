# AVR Aurora — Codex instructions

## Target

- PlatformIO project for `pro16MHzatmega328`.
- Arduino Pro Mini 5 V / ATmega328P, 16 MHz.
- 56 WS2812B LEDs controlled with FastLED.
- Keep SRAM and flash usage appropriate for ATmega328P.

## Implementation rules

- Use C++17 where supported by the AVR toolchain.
- No dynamic allocation, RTTI, exceptions, virtual interfaces, or Arduino `String`.
- Do not use blocking `delay()` in runtime code.
- Use `millis()`/`micros()` based scheduling and polling. Interrupts are reserved for HDD activity if needed.
- Keep hardware input normalization separate from state-machine logic.
- Buttons continue to connect directly to the motherboard. Firmware only observes them.
- Model HDD activity as a smoothed 0..128 value, not only as a boolean.
- Separate persistent PC state from temporary transition effects.
- Transition renderers have priority over ambient renderers.
- All timing, pins, current limits, and effect parameters belong in `src/config.h`.
- Avoid hidden magic numbers.
- Preserve extension points for a future over-temperature indication.

## Validation

Before finishing a task:

1. Run `pio run`.
2. Fix compiler errors and warnings introduced by the change.
3. Report program and SRAM usage.
4. Do not claim hardware behavior was validated without real hardware.

Read `docs/spec.md` before modifying behavior and `TASK.md` for the current milestone.
