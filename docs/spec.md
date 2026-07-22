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
- Add a `strip_power_present` sensing input. When strip power is absent, firmware must not drive the WS2812 data line.
- FastLED hard current limit: 2000 mA at 5 V.
- Recommended electrical protection: 330–470 ohm series resistor on data; power injection at both strip ends.

## Observed inputs

The controller passively observes these front-panel lines through safe interface circuitry:

1. Power LED.
2. HDD LED.
3. Power button.
4. Reset button.
5. Strip power present.
6. Temporary debug mode button.

Power/HDD LED polarity is not assumed at the logical layer. Hardware adapters and input configuration normalize each input to an active boolean.

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
    Warn,
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

- Run the white shutdown wave once for a normal shutdown request, then hold every LED at black even while the OS continues shutting down.
- Do not return to `Running` merely because the Power LED stays active briefly.
- Power LED blinking does not enter `Sleeping`; shutdown-related states have priority over sleep reconciliation.
- Power LED off -> `Off`.
- Power LED still active after 120000 ms -> `Warn`.

### Warn

- Minimal deterministic shutdown warning state for a machine that did not power off after the normal shutdown request.
- Remain in `Warn` while Power LED is active.
- Power LED off -> `Off`.

### Sleeping

- Render the sleep ambient effect only when strip power is present.
- Stable Power LED on -> `Running`, even if strip power is absent. This keeps logical PC state aware of a running machine while LED output remains disabled until strip power returns.
- Stable Power LED off -> `Off`.

## Power LED classification

Classify the normalized Power LED as `Off`, `On`, or `Blinking` from edge history. A short off interval must not immediately classify the PC as off. Sleep blinking is expected and must produce `Sleeping`.

Exact timing thresholds are configurable and initially conservative.

## HDD activity

Expose a smoothed activity value in the inclusive range 0..128.

Hybrid model:

- Rising edge adds a configurable boost.
- Active level adds a configurable amount per update interval.
- Activity decays symmetrically/configurably when inactive.
- Saturate to 0..128.

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

All effects are non-blocking functions of current time and local effect state.

## Effects

### Normal ambient: Aurora

A flat, one-dimensional northern-lights style animation along all 56 LEDs. Parameters include base brightness, speed, spread, and color characteristics. HDD activity may increase speed and/or brightness.

### Startup

The aurora becomes brighter and spreads outward, flashes, then smoothly settles into the normal ambient animation.

### Shutdown

- Choose a random origin from LEDs 23..32.
- Emit a white flash/wave in both linear directions.
- LEDs behind the wave remain black.
- At completion all LEDs remain black.

### Reset

- Same broad wave concept as shutdown.
- Red, faster, then resume the normal ambient animation.
- Persistent PC state remains `Running`.

### Forced shutdown

Triggered by holding the power button:

- 0..2000 ms: increase brightness.
- At 2000 ms: flash.
- 2000..4000 ms: fade to black.
- At 4000 ms: latch forced shutdown and remain black.
- If released before 4000 ms, cancel the preview and execute the normal shutdown transition.

### Sleep

Maintain one or two random dim points across LEDs 0..55. They appear and fade slowly, with optional overlap. No bright global animation.

## Timing and scheduling

- No runtime `delay()` calls.
- Poll and debounce ordinary inputs on a short periodic timer.
- Use an HDD edge interrupt only if the selected hardware circuit and FastLED timing make it worthwhile.
- Initial render target: 50 FPS.
- Serial debug output must be rate-limited and must not affect animation timing.

## Debug mode selection

A temporary button cycles available ambient modes for development. No EEPROM persistence is required. The final firmware may expose only the Aurora mode.

## Future extension

Reserve a clean way to inject system alerts, especially an over-temperature indication, without rewriting the state machine or base effects.
