# AVR Aurora firmware specification

## Hardware

- Controller: Arduino Pro Mini 5 V, ATmega328P, 16 MHz.
- LEDs: 56 WS2812B LEDs.
- Geometry is logically one-dimensional:
  - LED 0: rear-left.
  - LED 22: front-left.
  - LEDs 23..32: front edge.
  - LED 55: rear-right.
- Controller power: PC 5V standby rail.
- LED strip power: main PSU Molex 5 V.
- Keep A0 unconnected; firmware samples it together with `micros()` once at
  startup to seed the Aurora PRNG.
- Add a `strip_power_present` sensing input. When strip power is absent, firmware must not drive the WS2812 data line.
- FastLED hard current limit: 2000 mA at 5 V.
- Recommended electrical protection: 330–470 ohm series resistor on data; power injection at both strip ends.

The AVR pin assignment is:

| Function | Arduino Pro Mini pin | Configuration constant |
| --- | --- | --- |
| WS2812B data | D9 | `AvrConfig::LedDataPin` |
| Power LED sense | D3 | `AvrConfig::PowerLedPin` |
| HDD LED sense | D2 | `AvrConfig::HddLedPin` |
| Power button sense | D4 | `AvrConfig::PowerButtonPin` |
| Reset button sense | D5 | `AvrConfig::ResetButtonPin` |
| Strip power present | D7 | `AvrConfig::StripPowerPresentPin` |
| Aurora entropy input (left unconnected) | A0 | `AvrConfig::AuroraEntropyPin` |

## Observed inputs

The controller passively observes these front-panel lines through safe interface circuitry:

1. Power LED.
2. HDD LED.
3. Power button.
4. Reset button.
5. Strip power present.

Power/HDD LED polarity is not assumed at the logical layer. Hardware adapters
and input configuration normalize digital observations to:

```cpp
enum class SignalState : uint8_t {
    Low,
    Rising,
    High,
    Falling,
    Blinking,
};
```

`Rising` is active now and reports a Low-to-High transition for the current
runtime update. `Falling` is inactive now and reports a High-to-Low transition.
AVR debounce is platform-specific and remains outside the portable core.

Buttons remain directly connected to the motherboard. The controller never blocks or emulates them.

## PC states

Persistent PC state:

```cpp
enum class PcState : uint8_t {
    Off,
    Starting,
    Running,
    Sleeping,
    AwaitShutdown,
};
```

Temporary transition effect:

```cpp
enum class TransitionEffect : uint8_t {
    None,
    Startup,
    Shutdown,
    ForcedShutdown,
    Reset,
};
```

Keeping these separate allows, for example, `PcState::Running` plus `TransitionEffect::Reset`.

The runtime also owns an internal animation mode:

```cpp
enum class AnimationMode : uint8_t {
    Off,
    Startup,
    Ambient,
    Reset,
    Shutdown,
    ForcedShutdown,
    Sleep,
};
```

`AnimationMode` selects the field-update loop and frame interpretation. It is
not another representation of persistent PC state. `TransitionEffect` remains
the public transition diagnostic derived from the active animation mode.

## State transitions

### Off

- Power button press -> `Starting` and mark startup as pending.
- Startup rendering begins only after `strip_power_present` becomes true.
- Stable Power LED on -> `Running`, even if strip power is absent; strip-dependent transitions are not requested until an actual local startup is observed.

### Starting

- Run the startup transition.
- Power LED provides state confirmation.
- At transition completion enter `Running`.
- If strip power disappears, render nothing and return to `Off` when the normalized PC state confirms it.

### Running

- Reset button press -> red reset transition; return to normal ambient effect after completion.
- Power button press starts hold tracking.
- Power button release before 4000 ms -> normal shutdown transition and `AwaitShutdown`.
- Power button remains held for 4000 ms -> forced shutdown is latched, then release enters `AwaitShutdown`.
- Detected Power LED blinking -> `Sleeping`.
- Power LED off, after filtering -> `Off`.

### AwaitShutdown

- Run the outward shutdown wave once for a normal shutdown request, fading
  every LED behind its front to black, then hold every LED at black even while
  the OS continues shutting down.
- Do not return to `Running` merely because the Power LED stays active briefly.
- Power LED blinking does not enter `Sleeping`; shutdown-related states have priority over sleep reconciliation.
- Power LED off -> `Off`.
- Power LED still active after `Config::AwaitShutdownTimeoutMs` -> `Running`;
  the shutdown request is considered ignored and normal ambient rendering
  resumes.

### Sleeping

- Render the sleep ambient effect only when strip power is present.
- Stable Power LED on -> `Running`, even if strip power is absent. This keeps logical PC state aware of a running machine while LED output remains disabled until strip power returns.
- Stable Power LED off -> `Off`.

## Power LED classification

Classify the normalized Power LED as `Off`, `On`, or `Blinking` from edge history. A short off interval must not immediately classify the PC as off. Sleep blinking is expected and must produce `Sleeping`.

Exact timing thresholds are configurable and initially conservative.

## HDD activity

Expose a smoothed activity value in the inclusive range 0..128. The normalized
HDD input contains a `SignalState` plus a signed `hddContribution` in Q8.8
activity units for the elapsed interval.

- `High` and `Rising` increase activity according to elapsed time.
- `Low` and `Falling` decrease activity according to elapsed time.
- `Blinking` applies the normalized contribution without inventing an edge
  boost.
- Integer division remainders are retained so small intervals and
  contributions can accumulate.
- Saturate to 0..128.

The AVR adapter samples the HDD level without an interrupt counter. Reintroduce
an HDD edge interrupt only if measurements on real hardware show that ordinary
sampling loses visually significant activity.

Ambient effect settings contain independent flags for whether HDD activity affects:

- animation speed;
- brightness.

Transition effects initially ignore HDD activity, but architecture should allow enabling it later.

## Rendering priority

1. Forced shutdown.
2. Shutdown.
3. Reset.
4. Startup.
5. Sleep ambient.
6. Normal ambient.
7. Off/black.

All effects are non-blocking functions of current time, the active animation
mode, and local effect state. `AuroraRuntime` owns one `Aurora::Field` for its
entire lifetime. Changing PC state or animation mode must not reset or clear
that field. Only a full runtime reset may reseed and clear it.

## Effects

### Normal ambient: Aurora

A flat, one-dimensional northern-lights style animation along all 56 LEDs. Parameters include base brightness, speed, spread, and color characteristics. HDD activity may increase speed and/or brightness.

The HDD-reactive background illumination is stored separately from flare
brightness as a 56-element Q8.8 array. Each LED uses the maximum of its flare
and background brightness. Its spatial texture averages two adjacent
deterministic 8-bit hash samples with a triangular wave whose phase advances
three steps per LED. HDD activity scales that texture up to the configured
background maximum. At maximum HDD activity the point-spawn rate doubles;
diffusion and fade timing do not speed up. A nonzero HDD background advances
the affected cell's color progress even when its flare brightness is zero. A
cell retains its color progress after both sources reach zero; only a growing
flare ignition may lower it again. The background has an
independent two-second full-scale release, so it fades more slowly without
extending the HDD-driven point-spawn rate. Color progress is diffused as an
independent field with the same center/side kernel as flare brightness. It is
normalized at the physical strip ends so uniform color remains uniform, and
transitions between neighboring flare peaks stay smooth.

New flare points do not appear at full brightness immediately. Up to ten
ignitions are active at once. Each ignition has a random central LED, a target
peak brightness of 168..220, a 600..1400 ms duration aligned to the
20 ms Aurora fixed step, and a radius of 1..3 LEDs. An integer cubic smoothstep
defines the central brightness target over time. After normal diffusion and
fade, the ignition raises its central Q8.8 flare cell to that target when
needed, compensating the losses without lowering an already brighter cell.
Targets of overlapping ignitions at the same position add with saturation at
255. When all slots are occupied, a new ignition replaces the oldest active
one. Existing point-spawn timing, batch size, and HDD rate scaling remain
unchanged.

While an ignition grows, it lowers the shared color-progress ceiling around
its center. Temporal suppression uses the same cubic smoothstep. Spatial
suppression uses the integer complement of smoothstep at `distance/(radius+1)`;
the center reaches color progress zero, while the effect decreases toward the
edge of the radius. Contributions are clipped at the physical strip ends and
never wrap. Overlapping ignitions add their brightness targets with saturation
and apply the lowest color-progress ceiling.

### Startup

Startup applies its own update loop to the existing Aurora field. The field
becomes brighter and spreads outward, flashes, then smoothly settles into the
normal ambient update loop. Entering ambient mode must preserve the field
arrays, ignition pool, fixed-step accumulator, and PRNG state produced during
startup.

### Shutdown

- Choose a random origin from LEDs 23..32.
- Use the same center-out spatial wave as Startup.
- Preserve the current Aurora frame ahead of the wave front.
- Emit a white front in both linear directions and smoothly fade each passed
  LED to zero behind it.
- At completion all LEDs remain black.
- Do not clear or reseed the shared Aurora field; an ignored shutdown resumes
  the preserved ambient field.

### Reset

- Same broad wave concept as shutdown.
- Red, faster, then resume the normal ambient update loop.
- Persistent PC state remains `Running`.
- Reset evolves the existing Aurora field rather than replacing it with an
  independent frame. At completion ambient processing continues from the
  resulting field without reseeding or clearing it.

### Forced shutdown

Triggered by holding the power button:

- 0..500 ms: keep rendering the current ambient/reset animation; do not start
  or report `ForcedShutdown`.
- 500..2000 ms: render `Config::AuroraColor2Rgb` and increase brightness.
- At 2000 ms: flash.
- 2000..4000 ms: fade to black.
- At 4000 ms: latch forced shutdown and remain black.
- If released before 4000 ms, cancel the preview and execute the normal
  shutdown transition. A release before 500 ms never renders a forced-shutdown
  frame.

### Sleep

Maintain one or two random dim points across LEDs 0..55. They appear and fade slowly, with optional overlap. No bright global animation.

## Timing and scheduling

- No runtime `delay()` calls.
- Poll and debounce ordinary inputs on a short periodic timer.
- HDD input is ordinarily sampled; there is no edge counter in the normalized
  core API.
- Initial render target: 50 FPS.
- Serial debug output must be rate-limited and must not affect animation timing.

## Native simulator

The C++ core is the only source of behavior. The SDL2/Dear ImGui simulator
advances explicit `uint32_t` simulation time and calls `AuroraRuntime::step()`
at most once per UI update. It does not use a fixed frontend logic tick or a
substep catch-up loop. Frame scheduling, smoothing, field evolution, and
transitions remain inside the core.

`AuroraRuntime` selects one animation update loop per rendered frame with an
internal `AnimationMode` switch. Mode entry initializes only mode-local timing,
phase, and origin data; it never resets the shared Aurora field.

## Future extension

Reserve a clean way to inject system alerts, especially an over-temperature indication, without rewriting the state machine or base effects.
