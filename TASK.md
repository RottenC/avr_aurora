# Current task: milestone 1 — buildable firmware skeleton

Implement the first buildable firmware milestone described by `docs/spec.md`.

## Required deliverables

1. Configure PlatformIO for Arduino Pro Mini 5 V / ATmega328P 16 MHz and FastLED.
2. Replace the generated `src/main.cpp` example.
3. Add small modules for:
   - configuration and pin constants;
   - normalized/debounced inputs;
   - Power LED mode classification (`Off`, `On`, `Blinking`);
   - HDD hybrid activity accumulator, 0..128;
   - persistent PC state machine;
   - temporary transition effect controller;
   - LED output and strip-power safety;
   - placeholder renderers for Aurora, startup, shutdown, reset, forced shutdown, sleep, and black/off;
   - rate-limited serial diagnostics.
4. Use a fixed 56-element `CRGB` buffer.
5. Apply FastLED 5 V / 2000 mA power limiting.
6. Run at an initial target of 50 FPS without `delay()`.
7. Keep all hardware-active polarities configurable because the front-panel electrical interface is not finalized.

## Suggested source layout

```text
src/
  main.cpp
  config.h
  inputs.h
  inputs.cpp
  power_led_tracker.h
  power_led_tracker.cpp
  hdd_activity.h
  hdd_activity.cpp
  pc_state.h
  pc_state.cpp
  effect_controller.h
  effect_controller.cpp
  led_output.h
  led_output.cpp
  serial_debug.h
  serial_debug.cpp
  effects/
    effects.h
    effects.cpp
```

A smaller layout is acceptable when it remains clear and testable on AVR.

## Milestone boundaries

This milestone is about architecture and verified compilation, not polished visuals.

Placeholder effects must visibly differ and be non-blocking, but detailed Aurora tuning belongs to the next milestone.

Do not add EEPROM persistence, PC-side communication, temperature sensors, or automatic pin polarity detection.

## Acceptance criteria

- `pio run` succeeds.
- No generated example code remains.
- Runtime code contains no `delay()`.
- No dynamic allocation or Arduino `String`.
- Input polarity and pin assignments are centralized.
- State and transition effect are modeled separately.
- Absence of strip power prevents driving the LED data pin.
- Serial diagnostics expose normalized inputs, PC state, transition, and HDD activity.
- Final response reports flash and SRAM usage and identifies anything that requires hardware testing.
