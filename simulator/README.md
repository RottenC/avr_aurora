# AVR Aurora simulator

Desktop skeleton for tuning `avr_aurora` visuals without flashing Arduino hardware. It implements one rendering mode only: **AVR-like mode**. It is not cycle-accurate and is not a photometric model of real WS2812 LEDs.

## Windows setup (cmd)

```cmd
cd simulator
bootstrap_venv.cmd
run.cmd
```

PowerShell launchers are intentionally not required, avoiding local script execution policy friction.

## Linux setup

```bash
cd simulator
./bootstrap_venv.sh
./run.sh
```

## Purpose and limitations

The simulator mirrors observable inputs, 50 FPS / 20 ms frame timing, 56 LED geometry, HDD smoothing, and fixed-width arithmetic diagnostics. It does not compile or bind the firmware, use Arduino/FastLED dependencies, include NumPy, implement final Aurora/Startup/Shutdown/Reset/Sleep/ForcedShutdown effects, or require hardware.

## LED geometry

Exactly 56 LEDs are rendered in two simultaneous views:

- `0..22`: left side, rear to front.
- `23..32`: front side, left to right.
- `33..55`: right side, front to rear.

Disable **Strip power** to black out the visualized strip.

## HDD modes

Modes are Manual, Light, Medium, Heavy, and Random / Quake. Non-manual modes use a deterministic PRNG; resetting the editable numeric seed reproduces the generated sequence. Parameters for activity rate, pulse duration, burst size, and randomness are exposed as UI placeholders for tuning presets.

## Strict arithmetic diagnostics

All simulated fixed-width conversions and saturating math go through `avr_math.py`. Diagnostics count u8/u16/signed wraps, saturating add/subtract, clamps, truncating divisions, invalid RGB writes, invalid LED indices, and slow frames. Strict mode raises on unexpected invalid LED/RGB/range violations while still allowing intentional wrap and saturation events to be recorded.

## Adding an effect

Create a class implementing `Effect.reset(context)` and `Effect.render(context, leds, diagnostics)`. Keep calculations close to AVR C++: ordinary integers, fixed-size lists, and helpers from `avr_math.py` for wrapping, saturation, scaling, and truncating division. Swap the effect in `Simulation` when ready.
