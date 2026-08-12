# Current task: shared-field animation modes

Generalize Aurora animation control so startup, ambient, and reset evolve one
continuous `Aurora::Field` owned by `AuroraRuntime`.

## Required deliverables

1. Add a compact `AnimationMode` enum and a mode switch inside the portable
   runtime update path.
2. Move field and RGB-frame ownership into `AuroraRuntime`.
3. Preserve the field, ignition pool, fixed-step accumulator, and PRNG across
   every animation-mode and PC-state transition.
4. Implement Startup as a field update that spreads, flashes, and settles into
   Ambient without clearing the field.
5. Implement Reset as a red wave that modifies the same field and returns to
   Ambient without clearing it.
6. Keep persistent `PcState` separate from temporary visual transitions.
7. Keep `TransitionEffect` and transition timing available in
   `AuroraSnapshot` for AVR and simulator diagnostics.
8. Remove the standalone effect controller and renderer-owned field lifecycle.

## Constraints

- Only `AuroraRuntime::reset()` may call `Aurora::Field::reset()`.
- Keep a single `Aurora::Field` and one fixed 56-element RGB frame.
- Do not allocate a transition frame or per-mode field.
- Preserve raw strip-power DATA-pin safety in the AVR layer.
- Use wraparound-safe `uint32_t` time arithmetic and no blocking calls.
- Keep dynamic allocation, RTTI, exceptions, virtual interfaces, and Arduino
  `String` out of the portable core.

## Acceptance criteria

- Startup completion plus Power LED confirmation still enters `Running`.
- Reset keeps persistent state at `Running` and repeated Reset restarts only
  the Reset animation clock.
- Reset and Ambient advance the same field PRNG for the same seed and timeline.
- Strip-power loss produces black output without erasing the logical state or
  Aurora field.
- Transition priority and forced-shutdown timing remain unchanged.
- Native PlatformIO, CMake core, simulator-session, and AVR builds pass without
  new warnings.
- Final results report flash and static SRAM usage; visual tuning and electrical
  behavior remain subject to simulator and real-hardware validation.
