# AVR Aurora simulator

Desktop workbench for developing `avr_aurora` visual effects without flashing Arduino hardware. It implements one rendering model only: **AVR-like mode**. It is not cycle-accurate and is not a photometric model of real WS2812 LEDs.

## Windows setup (cmd)

```cmd
cd simulator
bootstrap_venv.cmd
run.cmd
```

PowerShell launchers are intentionally not required, avoiding script execution policy friction.

## Linux setup

```bash
cd simulator
./bootstrap_venv.sh
./run.sh
```

## Purpose and limitations

The simulator mirrors observable inputs, 50 FPS / 20 ms frame timing, 56 LED geometry, HDD smoothing, Power LED classification, PC state transitions, transition priority and fixed-width arithmetic diagnostics. It does not bind or compile firmware C++, use Arduino/FastLED dependencies directly, include NumPy, implement final artistic effects, require networking, or require hardware.

## AVR-like arithmetic

All effect math should go through `avr_math.py`. `scale8()` implements FastLED fixed scaling semantics (`FASTLED_SCALE8_FIXED=1`):

```text
(value8 * scale8 + value8) >> 8
```

Examples: `scale8(255, 255) == 255`, `scale8(255, 0) == 0`, `scale8(128, 128) == 64`, and `scale8(300, 128) == 22` after recording the `300 -> uint8_t 44` wrap. `lerp8()` is documented as a simulator helper with C++-style truncation, not a claimed byte-for-byte FastLED function.

## LED geometry

Exactly 56 LEDs are rendered in linear and physical views:

- `0..22`: left side, rear to front.
- `23..32`: front side, left to right.
- `33..55`: right side, front to rear.

Disable **Strip power** to black out the visualized strip without implying a PC power-state change.

## Raw/classified Power LED

The Power LED source can be Manual, Off, On, or Blinking. Source-mode and blink-period changes are reconciled through timestamped transitions on the next simulation update instead of silently changing raw state. Blinking starts LOW at phase 0 and emits every timestamped transition crossed by a simulation step; a boundary exactly at the end of a step belongs to that current step, avoiding duplicates in the next step. The raw boolean source is then passed through a Python port of the firmware Power LED tracker, which classifies it as Off, On, or Blinking using the same short-off grace period, blink half-period limits, edge count and stale timeout defaults.

## Manual versus generated HDD signal

The Manual HDD checkbox is separate from the observed raw HDD signal. Manual mode reads the checkbox and automatic modes ignore it. The generated raw state, active edges this frame, pending active edges, pending HDD milliseconds and smoothed `0..128` activity are shown separately.

## State-driven rendering and preview

In **Auto** render mode, the PC state machine and effect controller select the placeholder renderer. Transition priority is `ForcedShutdown > Shutdown > Reset > Startup > None`. `ForcedShutdown` remains indefinite in the controller, but its placeholder visual progress uses the firmware 4000 ms forced-hold window. The state-machine and controller ports intentionally reproduce firmware control flow, but full electrical debounce and final polished visuals remain outside this phase.

Forced preview modes (`Force Aurora`, `Force Startup`, `Force Shutdown`, `Force Reset`, `Force ForcedShutdown`, `Force Sleep`, `Force Warn`, `Force Off`) render the selected diagnostic placeholder without mutating PC state, Power LED tracking, the effect controller or inputs. Use **Restart preview** to reset preview elapsed/progress.

## Diagnostics and timeline

Diagnostics keep cumulative counters and a bounded retained event history. The UI shows both a counter summary and a throttled newest-first table with time, frame, operation, label, inputs and result. Strict mode raises for unexpected invalid LED indices/RGB/ranges while intentional wrapping, saturation and explicit clamps remain recorded and allowed.

The timeline is a lightweight custom PySide6 widget, not Matplotlib. It uses time-based sampled retention, deterministic colors, and shows about the last 15 seconds of simulated time for raw Power LED, classified Power LED mode, raw HDD LED, smoothed HDD activity, PC state and active transition.

## Adding an effect

Create an `Effect` implementation with `reset(context)` and `render(context, leds, diagnostics)`. Use ordinary Python integers, fixed-size lists/buffers, `LedBuffer` writes, `FrameContext.now_ms`, `started_at_ms`/progress values and helpers from `avr_math.py`. Keep effect logic deterministic for the same seed and avoid floating-point calculations inside rendering.
