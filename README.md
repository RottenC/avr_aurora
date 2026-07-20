# AVR Aurora

WS2812B lighting controller for a PC case, based on an Arduino Pro Mini 5 V / ATmega328P.

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
