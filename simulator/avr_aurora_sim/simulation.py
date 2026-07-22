from time import perf_counter_ns
from .model import InputState, SimulationState, SimulatorConfig, FrameContext, LedBuffer
from .state_types import PcState, Transition
from .diagnostics import Diagnostics
from .avr_math import clamp_u8
from .hdd_generator import HddGenerator, HddMode, HddParams, HddTransition
from .effects import PlaceholderEffect

class Simulation:
    def __init__(self):
        self.config = SimulatorConfig()
        self.inputs = InputState()
        self.state = SimulationState()
        self.diagnostics = Diagnostics()
        self.generator = HddGenerator(self.state.random_seed)
        self.effect = PlaceholderEffect()
        self.led_buffer = LedBuffer(self.diagnostics, self.config.led_count)
        self.last_frame_elapsed_ms = 0.0

    def restart(self) -> None:
        seed = self.state.random_seed
        strict = self.diagnostics.strict
        mode = self.generator.mode
        params = HddParams(**vars(self.generator.params))
        inputs = InputState(**vars(self.inputs))
        config = SimulatorConfig(**vars(self.config))
        max_events = self.diagnostics.max_events
        self.config = config
        self.inputs = inputs
        self.state = SimulationState(random_seed=seed)
        self.diagnostics = Diagnostics(strict=strict, max_events=max_events)
        self.generator = HddGenerator(seed, mode, params)
        self.effect = PlaceholderEffect()
        self.led_buffer = LedBuffer(self.diagnostics, self.config.led_count)
        self.last_frame_elapsed_ms = 0.0

    def context(self, dt_ms: int = 0) -> FrameContext:
        return FrameContext(self.state.now_ms, dt_ms, self.state.frame_number, self.state.pc_state, self.state.transition, self.inputs.power_button, self.inputs.reset_button, self.inputs.power_led, self.inputs.hdd_led, self.inputs.strip_power, self.state.hdd_activity)

    def set_hdd_mode(self, mode: HddMode) -> None:
        self.generator.set_mode(mode)

    def set_hdd_params(self, params: HddParams) -> None:
        self.generator.set_params(params)

    def step(self, dt_ms: int | None = None, real_budget_ms: int | None = None):
        frame_start_ns = perf_counter_ns()
        dt_ms = self.config.frame_interval_ms if dt_ms is None else int(dt_ms)
        self.state.now_ms += dt_ms
        self.state.frame_number += 1
        self.diagnostics.set_context(self.state.frame_number, self.state.now_ms)

        start_raw = self.generator.raw
        raw, transitions = self.generator.update(dt_ms, self.inputs.hdd_led)
        self._update_hdd_activity(dt_ms, start_raw, transitions)
        self.inputs.hdd_led = raw

        self.state.power_hold_ms = self.state.power_hold_ms + dt_ms if self.inputs.power_button else 0
        self.state.pc_state = PcState.RUNNING if self.inputs.power_led else PcState.OFF
        self.state.transition = Transition.RESET if self.inputs.reset_button else Transition.NONE

        ctx = self.context(dt_ms)
        self.led_buffer.clear()
        if self.inputs.strip_power:
            self.effect.render(ctx, self.led_buffer, self.diagnostics)

        elapsed_ns = perf_counter_ns() - frame_start_ns
        self.last_frame_elapsed_ms = elapsed_ns / 1_000_000
        budget_ms = self.config.frame_interval_ms if real_budget_ms is None else real_budget_ms
        if elapsed_ns > budget_ms * 1_000_000:
            self.diagnostics.record("slow_frame", (self.last_frame_elapsed_ms, budget_ms), None, "measured frame time")
        return ctx, self.led_buffer

    def _update_hdd_activity(self, dt_ms: int, start_raw: bool, transitions: list[HddTransition]) -> None:
        active = start_raw
        edge_count = 0
        by_offset: dict[int, list[HddTransition]] = {}
        for transition in transitions:
            by_offset.setdefault(transition.offset_ms, []).append(transition)
        for offset in range(dt_ms):
            for transition in by_offset.get(offset, []):
                active = transition.active
                if transition.active_edge:
                    edge_count += 1
            self.state.hdd_pending_ms += 1
            if self.state.hdd_pending_ms >= self.config.hdd_update_ms:
                self._apply_hdd_tick(active, edge_count, self.config.hdd_update_ms)
                self.state.hdd_pending_ms -= self.config.hdd_update_ms
                edge_count = 0
        if edge_count:
            self._pending_edge_count = getattr(self, "_pending_edge_count", 0) + edge_count
        elif not hasattr(self, "_pending_edge_count"):
            self._pending_edge_count = 0

    def _apply_hdd_tick(self, active: bool, edge_count: int, elapsed_ms: int) -> None:
        edge_count += getattr(self, "_pending_edge_count", 0)
        self._pending_edge_count = 0
        next_value = int(self.state.hdd_activity)
        next_value += edge_count * self.config.hdd_edge_boost
        ticks = elapsed_ms // self.config.hdd_update_ms
        delta = ticks * (self.config.hdd_active_rise if active else self.config.hdd_inactive_decay)
        if active:
            next_value += delta
        else:
            next_value = max(0, next_value - delta)
        if next_value > self.config.hdd_max:
            self.diagnostics.record("clamped_value", ("hdd_activity", next_value), self.config.hdd_max, "hdd max")
            next_value = self.config.hdd_max
        self.state.hdd_activity = clamp_u8(next_value, self.diagnostics, "hdd activity storage")
