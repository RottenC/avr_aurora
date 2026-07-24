from avr_aurora_sim.hdd_generator import HddMode
from avr_aurora_sim.effects import AuroraFieldEffect
from avr_aurora_sim.power_led_generator import PowerLedSourceMode
from avr_aurora_sim.state_types import PcState, Transition
from avr_aurora_sim.simulation import Simulation

def run_step(sim, ms=20): sim.step(ms); return sim

def test_cold_boot_and_reset():
    sim = Simulation(); sim.inputs.power_button = True; run_step(sim); assert sim.pc_state_machine.state is PcState.STARTING and sim.effect_controller.current is Transition.STARTUP
    sim.inputs.power_button = False; sim.power_led_generator.mode = PowerLedSourceMode.ON; sim.step(2200); sim.step(20)
    assert sim.pc_state_machine.state is PcState.RUNNING
    sim.inputs.reset_button = True; sim.step(20); assert sim.effect_controller.current is Transition.RESET

def test_short_shutdown_and_blackout_when_off():
    sim = Simulation(); sim.power_led_generator.mode = PowerLedSourceMode.ON; sim.step(4000); assert sim.pc_state_machine.state is PcState.RUNNING
    sim.inputs.power_button = True; sim.step(20); sim.inputs.power_button = False; sim.step(100)
    assert sim.pc_state_machine.state is PcState.AWAIT_SHUTDOWN and sim.effect_controller.current is Transition.SHUTDOWN
    sim.power_led_generator.mode = PowerLedSourceMode.OFF; sim.step(4000); assert sim.pc_state_machine.state is PcState.OFF
    assert all(pixel == (0,0,0) for pixel in sim.led_buffer.to_list())

def test_forced_shutdown_sleep_wake_and_strip_loss_startup():
    sim = Simulation(); sim.power_led_generator.mode = PowerLedSourceMode.ON; sim.step(20)
    sim.inputs.power_button = True; sim.step(20); sim.step(4000); sim.inputs.power_button = False; sim.step(20)
    assert sim.pc_state_machine.forced_latched and sim.pc_state_machine.state is PcState.AWAIT_SHUTDOWN
    sim = Simulation(); sim.power_led_generator.mode = PowerLedSourceMode.BLINKING; sim.power_led_generator.half_period_ms = 150
    for _ in range(10): sim.step(150)
    assert sim.pc_state_machine.state is PcState.SLEEPING
    sim.power_led_generator.mode = PowerLedSourceMode.ON; sim.step(4000); assert sim.pc_state_machine.state is PcState.RUNNING
    sim = Simulation(); sim.inputs.strip_power = False; sim.inputs.power_button = True; sim.step(20); assert sim.effect_controller.current is Transition.NONE
    sim.inputs.strip_power = True; sim.inputs.power_button = False; sim.step(20); assert sim.effect_controller.current is Transition.STARTUP

def test_preview_override_does_not_mutate_control_plane():
    sim = Simulation(); sim.power_led_generator.mode = PowerLedSourceMode.ON; sim.step(20)
    state = sim.pc_state_machine.state; current = sim.effect_controller.current
    sim.render_override = "Force Shutdown"; sim.restart_preview(); sim.step(20)
    assert sim.pc_state_machine.state is state and sim.effect_controller.current is current
    assert sim.context().preview_progress >= 0

def test_control_plane_frame_size_equivalence_for_simple_inputs():
    def scenario(step):
        sim = Simulation(); sim.power_led_generator.mode = PowerLedSourceMode.ON
        for _ in range(100 // step): sim.step(step)
        return sim.pc_state_machine.state, sim.effect_controller.current
    assert scenario(5) == scenario(20)

def test_restart_releases_momentary_buttons_without_synthetic_events():
    sim = Simulation(); sim.inputs.power_button = True; sim.inputs.reset_button = True
    sim.restart()
    assert not sim.inputs.power_button and not sim.inputs.reset_button
    sim.step(20)
    assert sim.effect_controller.current is Transition.NONE
    assert sim.pc_state_machine.state is PcState.OFF


def test_aurora_selection_preview_strip_power_and_off_integration():
    sim = Simulation()
    assert isinstance(sim.effects["Aurora"], AuroraFieldEffect)

    sim.power_led_generator.mode = PowerLedSourceMode.ON
    sim.step(20)
    assert sim.pc_state_machine.state is PcState.RUNNING
    assert sim._active_effect_key == "Aurora"

    control_state = sim.pc_state_machine.state
    control_transition = sim.effect_controller.current
    sim.render_override = "Force Aurora"
    sim.restart_preview()
    sim.step(20)
    assert sim._active_effect_key == "Aurora"
    assert sim.pc_state_machine.state is control_state
    assert sim.effect_controller.current is control_transition

    sim.inputs.strip_power = False
    sim.step(20)
    assert sim._active_effect_key is None
    assert sim.led_buffer.to_list() == [(0, 0, 0)] * sim.config.led_count

    off = Simulation()
    off.step(20)
    assert off._active_effect_key == "Off"
    assert off.led_buffer.to_list() == [(0, 0, 0)] * off.config.led_count


def test_aurora_preview_restart_restores_deterministic_initial_state():
    sim = Simulation()
    sim.render_override = "Force Aurora"
    sim.restart_preview()
    aurora = sim.effects["Aurora"]
    initial = (
        aurora.brightness,
        aurora.color_progress,
        aurora.ticks_until_next_spawn,
        aurora.prng_state,
        aurora.fixed_step_accumulator_ms,
    )
    for _ in range(50):
        sim.step(20)
    assert aurora.brightness != initial[0]

    sim.restart_preview()
    assert (
        aurora.brightness,
        aurora.color_progress,
        aurora.ticks_until_next_spawn,
        aurora.prng_state,
        aurora.fixed_step_accumulator_ms,
    ) == initial
