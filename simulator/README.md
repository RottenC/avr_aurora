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

The simulator mirrors observable inputs, 50 FPS / 20 ms frame timing, 56 LED geometry, HDD smoothing, Power LED classification, PC state transitions, transition priority and fixed-width arithmetic diagnostics. It does not bind or compile firmware C++, use Arduino/FastLED dependencies directly, include NumPy, require networking, or require hardware.

## Aurora field

The normal ambient effect is a deterministic one-dimensional cellular field mirrored by the firmware's platform-independent AVR/C++ implementation. Its complete visual field is held in two fixed-size arrays: brightness and teal-to-purple color progress. Reused next-state buffers keep diffusion out of place without allocating a new 56-element list per frame; no particles, stars, flare objects, floating-point effect math, HSV conversion, or Python random module are used.

The field advances only in fixed 20 ms ticks, independent of display FPS or simulator speed-step grouping. A portable `xorshift32` generator injects one to three maximum-brightness points at deterministic 20–70 tick intervals. Brightness then diffuses along the open LED chain with a normalized `1/65, 63/65, 1/65` kernel, overlapping contributions combine with saturation, and color progress travels with brightness through an integer weighted average. RGB composition linearly interpolates from `#1aba94` to `#6e347c`, then from the static dark-blue `#091e37` background to that flare color.

Restarting the simulation or Aurora preview resets the buffers, fixed-step accumulator, spawn timer, and PRNG sequence. Switching away and back also restarts Aurora; it does not advance while another effect or a strip-power blackout is active.

Future work includes color drift, HDD influence, alternative diffusion kernels, nonlinear color interpolation, and hardware calibration.

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

The physical U-shaped view lives in a separate right-hand panel and is enabled
with **Show U-shaped layout**. Below the linear strip, the scrollable Aurora
field chart shows one point per LED. Point height is the field brightness,
point color is interpolated from `color_1` to `color_2` using color progress,
and the exact brightness (`B`) and progress (`P`) values are printed below
each point.

Disable **Strip power** to black out the visualized strip without implying a PC power-state change.

## Raw/classified Power LED

The Power LED source can be Manual, Off, On, or Blinking. Source-mode and blink-period changes are reconciled through timestamped transitions on the next simulation update instead of silently changing raw state. Blinking starts LOW at phase 0 and emits every timestamped transition crossed by a simulation step; a boundary exactly at the end of a step belongs to that current step, avoiding duplicates in the next step. The raw boolean source is then passed through a Python port of the firmware Power LED tracker, which classifies it as Off, On, or Blinking using the same short-off grace period, blink half-period limits, edge count and stale timeout defaults.

## Manual versus generated HDD signal

The Manual HDD checkbox is separate from the observed raw HDD signal. Manual mode reads the checkbox and automatic modes ignore it. The generated raw state, active edges this frame, pending active edges, pending HDD milliseconds and smoothed `0..128` activity are shown separately.

## State-driven rendering and preview

In **Auto** render mode, the PC state machine and effect controller select the renderer. Running uses the final-form Aurora field; other visuals remain placeholders. Transition priority is `ForcedShutdown > Shutdown > Reset > Startup > None`. `ForcedShutdown` remains indefinite in the controller, but its placeholder visual progress uses the firmware 4000 ms forced-hold window. The state-machine and controller ports intentionally reproduce firmware control flow, but full electrical debounce and the remaining polished visuals remain outside this phase.

Forced preview modes (`Force Aurora`, `Force Startup`, `Force Shutdown`, `Force Reset`, `Force ForcedShutdown`, `Force Sleep`, `Force Warn`, `Force Off`) render the selected effect without mutating PC state, Power LED tracking, the effect controller or inputs. Use **Restart preview** to reset preview elapsed/progress and stateful effect data.

## Diagnostics and timeline

Diagnostics keep cumulative counters and a bounded retained event history. The UI shows both a counter summary and a throttled newest-first table with time, frame, operation, label, inputs and result. Strict mode raises for unexpected invalid LED indices/RGB/ranges while intentional wrapping, saturation and explicit clamps remain recorded and allowed.

The timeline is a lightweight custom PySide6 widget, not Matplotlib. Its `timeline_model.py` retention classes are Qt-independent for headless unit testing. The widget uses time-based sampled retention, deterministic colors, and shows about the last 15 seconds of simulated time for raw Power LED, classified Power LED mode, raw HDD LED, smoothed HDD activity, PC state and active transition.


## Test groups

Core simulator tests are headless and do not install or import PySide6:

```bash
python -m pip install -r simulator/requirements-core.txt
PYTHONPATH=simulator python -m pytest -q simulator/tests/core
```

GUI smoke tests are intentionally small and run separately with Qt offscreen mode:

```bash
python -m pip install -r simulator/requirements-gui.txt
QT_QPA_PLATFORM=offscreen PYTHONPATH=simulator python -m pytest -q simulator/tests/gui
```

Pull requests run both groups: core tests validate the firmware-like simulator model without Qt, and GUI smoke tests validate that the PySide6 workbench can construct, process one event cycle, refresh widgets and close. Qt is loaded only by GUI modules or when launching the desktop application with `run.cmd` / `run.sh`.

## Adding an effect

Create an `Effect` implementation with `reset(context)` and `render(context, leds, diagnostics)`. Use ordinary Python integers, fixed-size lists/buffers, `LedBuffer` writes, `FrameContext.now_ms`, `started_at_ms`/progress values and helpers from `avr_math.py`. Keep effect logic deterministic for the same seed and avoid floating-point calculations inside rendering.

## Final workbench cleanup notes

Power LED source changes are treated as workbench configuration reconciliation, not as physical blink edges. Reconciliation is emitted at `offset_ms = 0` on the next simulation update, coalesces multiple pending mode/period changes to the final selected source, and rebases the tracker without incrementing blink-edge accumulation. Manual checkbox changes while already in Manual mode remain normal observed raw transitions.

Restart releases the momentary Power and Reset buttons to avoid synthetic press events on the first post-restart frame. Persistent controls such as manual LED checkboxes, strip power, source modes, seed, strict mode, FPS/configuration and preview selection remain preserved.

The simulator stores two contexts after each frame: the control context produced by the state machine/effect controller, and the render context actually passed to the selected effect. In Auto they are equivalent; in forced preview the control context remains real while the render context reflects the preview transition and progress.

Forced-shutdown visual timing is configurable through `SimulatorConfig.power_hold_forced_ms` and `SimulatorConfig.forced_flash_at_ms`. The flash point is clamped to the hold duration so shorter custom timings remain safe.

Diagnostics expose a monotonic revision counter in addition to bounded retained events and cumulative counters. UI consumers use the revision so tables keep updating after the event deque is full.

Timeline retention is primarily time-based and keeps approximately the last 15 seconds of simulated time, with a defensive absolute sample cap so repeated zero-duration/state-change samples cannot grow memory without bound.
