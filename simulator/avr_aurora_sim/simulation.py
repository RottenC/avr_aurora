from time import perf_counter_ns
from . import firmware_defaults as defaults
from .diagnostics import Diagnostics
from .avr_math import clamp_u8, div_trunc
from .effect_controller import EffectController, EffectControllerConfig
from .effects import AuroraPlaceholder, ForcedShutdownPlaceholder, OffEffect, ResetPlaceholder, ShutdownPlaceholder, SleepPlaceholder, StartupPlaceholder, WarnPlaceholder
from .hdd_generator import HddGenerator, HddMode, HddParams, HddTransition
from .input_edges import InputEdgeTracker
from .model import FrameContext, InputState, LedBuffer, SimulationState, SimulatorConfig
from .pc_state_machine import PcStateConfig, PcStateInputs, PcStateMachine
from .power_led_generator import PowerLedGenerator, PowerLedSourceMode, PowerLedTransition
from .power_led_tracker import PowerLedTracker, PowerLedMode, PowerLedTrackerConfig
from .state_types import PcState, Transition

class Simulation:
    def __init__(self, config: SimulatorConfig | None = None):
        self.config = config or SimulatorConfig()
        self.inputs = InputState()
        self.state = SimulationState()
        self.diagnostics = Diagnostics()
        self.generator = HddGenerator(self.state.random_seed)
        self.power_led_generator = PowerLedGenerator()
        self.power_led_tracker = PowerLedTracker(self._power_led_tracker_config())
        self.input_edges = InputEdgeTracker()
        self.pc_state_machine = PcStateMachine(self._pc_state_config())
        self.effect_controller = EffectController(self._effect_controller_config())
        self.effects = {
            "Off": OffEffect(), "Aurora": AuroraPlaceholder(), "Startup": StartupPlaceholder(), "Shutdown": ShutdownPlaceholder(),
            "Reset": ResetPlaceholder(), "ForcedShutdown": ForcedShutdownPlaceholder(), "Sleep": SleepPlaceholder(), "Warn": WarnPlaceholder(),
        }
        self.led_buffer = LedBuffer(self.diagnostics, self.config.led_count)
        self.last_frame_elapsed_ms = 0.0
        self._pending_edge_count = 0
        self.render_override = "Auto"

    def restart(self) -> None:
        seed = self.state.random_seed
        strict = self.diagnostics.strict
        mode = self.generator.mode
        params = HddParams(**vars(self.generator.params))
        pwr_mode = self.power_led_generator.mode
        pwr_half = self.power_led_generator.half_period_ms
        inputs = InputState(**vars(self.inputs))
        config = SimulatorConfig(**vars(self.config))
        max_events = self.diagnostics.max_events
        override = self.render_override
        self.__init__(config)
        self.inputs = inputs; self.state = SimulationState(random_seed=seed)
        self.diagnostics = Diagnostics(strict=strict, max_events=max_events)
        self.generator = HddGenerator(seed, mode, params)
        self.power_led_generator = PowerLedGenerator(pwr_mode, pwr_half)
        self.power_led_tracker = PowerLedTracker(self._power_led_tracker_config())
        self.pc_state_machine = PcStateMachine(self._pc_state_config())
        self.effect_controller = EffectController(self._effect_controller_config())
        self.led_buffer = LedBuffer(self.diagnostics, self.config.led_count)
        self._pending_edge_count = 0
        self.render_override = override

    def _power_led_tracker_config(self) -> PowerLedTrackerConfig:
        return PowerLedTrackerConfig(self.config.short_power_led_off_ignore_ms, self.config.power_led_blink_min_half_period_ms, self.config.power_led_blink_max_half_period_ms, self.config.power_led_blink_stale_ms, self.config.power_led_blink_edges_required)

    def _pc_state_config(self) -> PcStateConfig:
        return PcStateConfig(self.config.power_hold_forced_ms, self.config.starting_timeout_ms, self.config.shutdown_warning_timeout_ms)

    def _effect_controller_config(self) -> EffectControllerConfig:
        return EffectControllerConfig(self.config.startup_duration_ms, self.config.shutdown_duration_ms, self.config.reset_duration_ms)

    def context(self, dt_ms: int = 0, transition: Transition | None = None, started_at: int | None = None, progress: int | None = None) -> FrameContext:
        current = self.effect_controller.current if transition is None else transition
        start = self.effect_controller.started_at_ms if started_at is None else started_at
        elapsed = self.state.now_ms - start if current is not Transition.NONE else 0
        duration = self._visual_duration(current)
        prog = self._visual_progress8(current, elapsed, duration) if progress is None else progress
        return FrameContext(
            self.state.now_ms, dt_ms, self.state.frame_number, self.pc_state_machine.state, current,
            self.inputs.power_button, self.inputs.reset_button, self.inputs.manual_power_led, self.inputs.manual_hdd_led,
            self.state.raw_power_led, self.state.power_led_mode, self.state.raw_hdd_led, self.inputs.strip_power, self.state.hdd_activity,
            start, elapsed, duration, prog, self.preview_elapsed_ms(), self.preview_duration_ms(), self.preview_progress8()
        )

    def set_hdd_mode(self, mode: HddMode) -> None:
        self.generator.set_mode(mode)

    def set_hdd_params(self, params: HddParams) -> None:
        self.generator.set_params(params)

    def set_power_led_mode(self, mode: PowerLedSourceMode) -> None:
        self.power_led_generator.set_mode(mode)

    def set_power_led_half_period_ms(self, value: int) -> None:
        self.power_led_generator.set_half_period_ms(value)

    def restart_preview(self) -> None:
        self.state.preview_started_at_ms = self.state.now_ms

    def preview_duration_ms(self) -> int:
        effect = self._override_transition()
        if effect is Transition.STARTUP: return self.config.startup_duration_ms
        if effect is Transition.SHUTDOWN: return self.config.shutdown_duration_ms
        if effect is Transition.RESET: return self.config.reset_duration_ms
        if effect is Transition.FORCED_SHUTDOWN: return self.config.power_hold_forced_ms
        return 0

    def preview_elapsed_ms(self) -> int:
        return self.state.now_ms - self.state.preview_started_at_ms if self.render_override != "Auto" else 0

    def preview_progress8(self) -> int:
        duration = self.preview_duration_ms()
        if not duration: return 0
        return clamp_u8(div_trunc(min(self.preview_elapsed_ms(), duration) * 255, duration), self.diagnostics, "preview progress")

    def step(self, dt_ms: int | None = None, real_budget_ms: int | None = None):
        frame_start_ns = perf_counter_ns()
        dt_ms = self.config.frame_interval_ms if dt_ms is None else int(dt_ms)
        frame_start_ms = self.state.now_ms
        self.state.now_ms += dt_ms
        self.state.frame_number += 1
        self.diagnostics.set_context(self.state.frame_number, self.state.now_ms)

        frame_start_power_led = self.power_led_generator.raw
        final_power_led, power_transitions = self.power_led_generator.update(dt_ms, self.inputs.manual_power_led)
        self._update_power_led_tracker(frame_start_ms, dt_ms, frame_start_power_led, final_power_led, power_transitions)
        start_raw_hdd = self.state.raw_hdd_led
        raw_hdd, transitions = self.generator.update(dt_ms, self.inputs.manual_hdd_led)
        edges = self.input_edges.update(self.inputs.power_button, self.inputs.reset_button)
        self._update_hdd_activity(dt_ms, start_raw_hdd, transitions)
        self.state.raw_hdd_led = raw_hdd

        self.effect_controller.update(self.state.now_ms)
        finished = self.effect_controller.consume_finished()
        pc_inputs = PcStateInputs(self.inputs.strip_power, self.inputs.power_button, edges, self.state.power_led_mode, finished is Transition.STARTUP)
        events = self.pc_state_machine.update(pc_inputs, self.state.now_ms)
        self.state.pc_state = self.pc_state_machine.state
        self.state.power_hold_ms = self.pc_state_machine.hold_duration(self.state.now_ms)
        if events.cancel_startup: self.effect_controller.cancel(Transition.STARTUP)
        if events.cancel_forced_shutdown: self.effect_controller.cancel(Transition.FORCED_SHUTDOWN)
        if events.request_startup: self.effect_controller.request(Transition.STARTUP, self.state.now_ms)
        if events.request_reset: self.effect_controller.request(Transition.RESET, self.state.now_ms)
        if events.request_forced_shutdown: self.effect_controller.request(Transition.FORCED_SHUTDOWN, self.state.now_ms)
        if events.request_shutdown: self.effect_controller.request(Transition.SHUTDOWN, self.state.now_ms)
        self.effect_controller.reconcile(self.pc_state_machine.state)
        self.state.transition = self.effect_controller.current

        ctx = self.context(dt_ms)
        self.state.last_context = ctx
        self._render(ctx)
        elapsed_ns = perf_counter_ns() - frame_start_ns
        self.last_frame_elapsed_ms = elapsed_ns / 1_000_000
        budget_ms = self.config.frame_interval_ms if real_budget_ms is None else real_budget_ms
        if elapsed_ns > budget_ms * 1_000_000:
            self.diagnostics.record("slow_frame", (self.last_frame_elapsed_ms, budget_ms), None, "measured frame time")
        return ctx, self.led_buffer

    def _visual_duration(self, transition: Transition) -> int:
        if transition is Transition.FORCED_SHUTDOWN:
            return self.config.power_hold_forced_ms
        return self.effect_controller.duration(transition)

    def _visual_progress8(self, transition: Transition, elapsed_ms: int, duration_ms: int) -> int:
        if transition is Transition.NONE or duration_ms <= 0:
            return 0
        return clamp_u8(div_trunc(min(elapsed_ms, duration_ms) * 255, duration_ms, self.diagnostics, "transition visual progress"), self.diagnostics, "transition visual progress clamp")

    def _update_power_led_tracker(self, frame_start_ms: int, dt_ms: int, start_raw: bool, final_raw: bool, transitions: list[PowerLedTransition]) -> None:
        self.power_led_tracker.update(start_raw, frame_start_ms)
        for transition in transitions:
            self.power_led_tracker.update(transition.active, frame_start_ms + transition.offset_ms)
        self.power_led_tracker.update(final_raw, frame_start_ms + dt_ms)
        self.state.raw_power_led = final_raw
        self.state.power_led_mode = self.power_led_tracker.mode(self.state.now_ms)

    def _render(self, ctx: FrameContext) -> None:
        self.led_buffer.clear()
        if not self.inputs.strip_power:
            return
        key = self._selected_effect_key(ctx)
        effect_ctx = ctx if self.render_override == "Auto" else self._preview_context(ctx)
        self.effects[key].render(effect_ctx, self.led_buffer, self.diagnostics)

    def _selected_effect_key(self, ctx: FrameContext) -> str:
        if self.render_override != "Auto":
            return self.render_override.replace("Force ", "")
        if ctx.transition is Transition.STARTUP: return "Startup"
        if ctx.transition is Transition.SHUTDOWN: return "Shutdown"
        if ctx.transition is Transition.RESET: return "Reset"
        if ctx.transition is Transition.FORCED_SHUTDOWN: return "ForcedShutdown"
        if ctx.pc_state is PcState.SLEEPING: return "Sleep"
        if ctx.pc_state is PcState.WARN: return "Warn"
        if ctx.pc_state is PcState.RUNNING: return "Aurora"
        return "Off"

    def _override_transition(self) -> Transition:
        return {"Force Startup": Transition.STARTUP, "Force Shutdown": Transition.SHUTDOWN, "Force Reset": Transition.RESET, "Force ForcedShutdown": Transition.FORCED_SHUTDOWN}.get(self.render_override, Transition.NONE)

    def _preview_context(self, ctx: FrameContext) -> FrameContext:
        transition = self._override_transition()
        return self.context(ctx.dt_ms, transition, self.state.preview_started_at_ms, self.preview_progress8())

    def _update_hdd_activity(self, dt_ms: int, start_raw: bool, transitions: list[HddTransition]) -> None:
        self.state.active_edges_this_frame = 0
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
                    self.state.active_edges_this_frame += 1
            self.state.hdd_pending_ms += 1
            if self.state.hdd_pending_ms >= self.config.hdd_update_ms:
                self._apply_hdd_tick(active, edge_count, self.config.hdd_update_ms)
                self.state.hdd_pending_ms -= self.config.hdd_update_ms
                edge_count = 0
        if edge_count:
            self._pending_edge_count += edge_count

    def _apply_hdd_tick(self, active: bool, edge_count: int, elapsed_ms: int) -> None:
        edge_count += self._pending_edge_count
        self._pending_edge_count = 0
        next_value = int(self.state.hdd_activity) + edge_count * self.config.hdd_edge_boost
        ticks = elapsed_ms // self.config.hdd_update_ms
        delta = ticks * (self.config.hdd_active_rise if active else self.config.hdd_inactive_decay)
        next_value = next_value + delta if active else max(0, next_value - delta)
        if next_value > self.config.hdd_max:
            self.diagnostics.record("clamped_value", ("hdd_activity", next_value), self.config.hdd_max, "hdd max")
            next_value = self.config.hdd_max
        self.state.hdd_activity = clamp_u8(next_value, self.diagnostics, "hdd activity storage")
