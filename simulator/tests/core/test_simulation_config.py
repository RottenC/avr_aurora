from avr_aurora_sim.model import SimulatorConfig
from avr_aurora_sim.simulation import Simulation


def test_custom_config_propagates_to_subsystems_and_restart():
    config = SimulatorConfig(frame_interval_ms=25, target_fps=40, power_hold_forced_ms=1234, hdd_update_ms=7, startup_duration_ms=111, shutdown_duration_ms=222, reset_duration_ms=333, short_power_led_off_ignore_ms=44, power_led_blink_min_half_period_ms=55, power_led_blink_max_half_period_ms=66, power_led_blink_stale_ms=77, power_led_blink_edges_required=3, starting_timeout_ms=444, shutdown_warning_timeout_ms=555)
    sim = Simulation(config)
    assert sim.config.frame_interval_ms == 25
    assert sim.power_led_tracker.config.short_off_ignore_ms == 44
    assert sim.power_led_tracker.config.blink_edges_required == 3
    assert sim.pc_state_machine.config.forced_hold_ms == 1234
    assert sim.pc_state_machine.config.starting_timeout_ms == 444
    assert sim.effect_controller.config.startup_duration_ms == 111
    assert sim.effect_controller.config.shutdown_duration_ms == 222
    assert sim.effect_controller.config.reset_duration_ms == 333
    sim.restart()
    assert sim.config.frame_interval_ms == 25
    assert sim.pc_state_machine.config.shutdown_warning_timeout_ms == 555
from avr_aurora_sim.effects.placeholder import ForcedShutdownPlaceholder
from avr_aurora_sim.model import LedBuffer
from avr_aurora_sim.diagnostics import Diagnostics
from avr_aurora_sim.state_types import Transition


def test_custom_forced_timing_hdd_max_and_led_count():
    config = SimulatorConfig(power_hold_forced_ms=1000, forced_flash_at_ms=400, hdd_max=64, led_count=20)
    sim = Simulation(config)
    assert len(sim.led_buffer) == 20
    sim.render_override = "Force ForcedShutdown"; sim.restart_preview(); sim.step(500)
    ctx = sim.state.last_render_context
    assert ctx.transition is Transition.FORCED_SHUTDOWN
    assert ctx.transition_duration_ms == 1000
    assert ctx.forced_flash_at_ms == 400
    assert ctx.transition_progress == 127
    buffer = LedBuffer(Diagnostics(), count=20)
    ForcedShutdownPlaceholder().render(ctx, buffer, Diagnostics())
    assert len(buffer.to_list()) == 20
