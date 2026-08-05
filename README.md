# AVR Aurora

WS2812B lighting controller for a PC case, based on an Arduino Pro Mini 5 V / ATmega328P.

## Shared C++ runtime

Firmware behavior and rendering live in the hardware-independent
`AuroraRuntime` under `src/core`. The runtime accepts normalized input frames
plus an explicit `uint32_t` timestamp and exposes a read-only state snapshot
and fixed 56-pixel `Aurora::Rgb8` frame.

The AVR entry point only samples GPIO, supplies `millis()`, writes the portable
frame through the FastLED adapter, and prints diagnostics. The PlatformIO
`native` environment compiles the same runtime and drives it end to end in
`test/test_native/test_runtime.cpp`; this is the integration point for a future
desktop UI.

## OpenAI Codex

The repository contains the implementation context:

- `AGENTS.md` — constraints and coding rules;
- `docs/spec.md` — hardware and firmware behavior;
- `TASK.md` — the current implementation milestone.

Start a Codex task with:

```text
Implement the current milestone from TASK.md.
Read and follow AGENTS.md and docs/spec.md before changing code.
Run pio run, fix all build errors, and report flash/SRAM usage plus hardware-only validation points.
```

Codex should replace the generated PlatformIO example in `src/main.cpp` and keep the result buildable for `pro16MHzatmega328`.
