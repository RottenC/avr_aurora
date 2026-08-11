# AVR Aurora — Codex instructions

## Target hardware

- PlatformIO project for `pro16MHzatmega328`.
- Arduino Pro Mini 5 V / ATmega328P, 16 MHz.
- 56 WS2812B LEDs controlled with FastLED.
- Keep SRAM and flash usage appropriate for ATmega328P.

## Architecture

### Shared portable C++ core

The shared C++17 core is the canonical implementation of Aurora behavior. It
contains `AuroraRuntime`, the PC state machine, transition/effect controller,
Power LED classification, HDD activity model, effect selection, portable
effect calculations, explicit simulation-time handling, the resulting fixed
RGB frame, and the diagnostic state snapshot.

The same core sources must compile for both AVR/ATmega328P through PlatformIO
and native C++. The conceptual API is:

```text
normalized inputs + explicit timestamp
                  ↓
             AuroraRuntime
                  ↓
state snapshot + portable RGB frame
```

The core must not depend on Arduino, AVR registers, GPIO, interrupts, UART,
FastLED, SDL2, Dear ImGui, Qt/PySide, or operating-system wall-clock APIs. It
must never include files from `src/avr`.

### AVR platform layer

The AVR application and adapters own GPIO reads, input polarity, debounce, HDD
level sampling, raw strip-power safety, `millis()`, entropy
collection, FastLED configuration, `Rgb8` to `CRGB` conversion, the WS2812 DATA
pin safe state, `FastLED.show()`, and serial diagnostics.

Dependency direction is strictly:

```text
AVR application and adapters
             ↓
       portable C++ core
```

Raw physical strip-power loss is an electrical-safety signal and must disable
DATA driving immediately. Debounced logical strip-power presence is a separate
input to `AuroraRuntime` and the PC state machine. Output may resume only after
restoration is confirmed and a fresh portable frame has been rendered.

### Native simulator

The native simulator is a C++ application using SDL2 for the window, input,
timing, and platform integration; Dear ImGui for its minimal controls and state
display; and the SDL renderer backend for linear LED visualization.

It must call the same `AuroraRuntime` implementation used by the firmware and
must not reimplement the FSM, transition controller, Power LED classifier, HDD
model, or effects in another language.

```text
SDL2 + Dear ImGui simulator frontend
                 ↓
          portable C++ core
```

The simulator controls normalized virtual inputs and explicit simulation time,
then reads `AuroraSnapshot`, transition/state diagnostics, and the portable RGB
frame. SDL2 and Dear ImGui remain outside the core include and link interfaces.

### Existing Python simulator

The current Python/PySide simulator is a temporary workbench. It may remain
available during migration and may provide limited visual UI inspiration, but
it is not a reference implementation and must not be used for behavioral
parity, FSM, Power LED, HDD, or effects validation. It must not receive new
canonical firmware logic. Qt/PySide is not the native simulator frontend.

### Intended project layout

Reasonable future adjustments are allowed while preserving the boundaries:

```text
src/
  core/
    AuroraRuntime
    runtime types and installation geometry
    portable RGB types and math
    renderer
  effects/
    portable effect implementations
  avr/
    GPIO input sampling and debounce
    FastLED output and DATA-pin safety
    serial diagnostics
    hardware configuration
  main.cpp
    AVR entry point

native_simulator/
  CMakeLists.txt
  src/
    main.cpp
    simulator_app.cpp
    simulator_session.cpp
    led_view.cpp
  test/
    simulator_session_test.cpp

test/
  test_native/
    portable core and runtime tests
```

## Simulation-time contract

- `AuroraRuntime::step()` receives one normalized input snapshot for one
  explicit timestamp.
- Normalized digital inputs use `SignalState`: `Low`, `Rising`, `High`,
  `Falling`, or `Blinking`. Rising and Falling are one-update observations.
- Time comparisons use wraparound-safe unsigned `uint32_t` arithmetic.
- A normal simulator UI update advances `uint32_t` simulation time and invokes
  `AuroraRuntime::step()` at most once. There is no frontend fixed logic tick,
  substep loop, or catch-up accumulator.
- Speed scaling may keep only a fractional-millisecond remainder. A clamped
  wall-time delta prevents debugger stalls from producing unbounded jumps.
- Pause suppresses automatic time advancement and runtime calls. A manual step
  advances exactly `Config::FrameIntervalMs` with one runtime call.
- Internal frame scheduling, HDD smoothing, Aurora field updates, and
  transition timing remain responsibilities of the core.

The simulator update should conceptually follow:

```cpp
simulationTimeMs += scaledAndClampedWallDeltaMs;
runtime.step(virtualInputs, simulationTimeMs);  // at most once per UI update
drawImGui(runtime.snapshot(), runtime.ledFrame());
```

Do not add SDL2 or Dear ImGui dependencies to the portable core to implement
this contract.

## Implementation rules

- Use C++17 where supported by the AVR toolchain.
- No dynamic allocation, RTTI, exceptions, virtual interfaces, `std::function`,
  or Arduino `String`.
- Do not use blocking `delay()` in runtime code.
- Use explicit time with `millis()`/`micros()`-based scheduling and polling in
  the AVR layer. Reintroduce an HDD edge interrupt only if real-hardware
  measurements show that sampling loses visually significant activity.
- Keep hardware input normalization separate from state-machine logic.
- Buttons continue to connect directly to the motherboard. Firmware only
  observes them.
- Model HDD activity as a smoothed 0..128 value, not only as a boolean.
- Separate persistent PC state from temporary transition effects.
- Transition renderers have priority over ambient renderers.
- Core timings and effect parameters belong in `src/config.h`; pins, electrical
  polarity, current limits, debounce, and AVR diagnostic settings belong in
  `src/avr/avr_config.h`.
- Avoid hidden magic numbers.
- Preserve extension points for a future over-temperature indication.
- dot not run test until user asks about it

## Validation

Before finishing a task:

1. Read `docs/spec.md` before modifying behavior and `TASK.md` for the current
   milestone.
2. Run the relevant AVR, native PlatformIO, CMake, and simulator tests.
3. Fix compiler errors and warnings introduced by the change.
4. Report program and static SRAM usage.
5. Do not claim hardware behavior was validated without real hardware.
