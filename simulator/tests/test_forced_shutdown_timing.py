from avr_aurora_sim import firmware_defaults as defaults
from avr_aurora_sim.power_led_generator import PowerLedSourceMode
from avr_aurora_sim.state_types import PcState, Transition
from avr_aurora_sim.simulation import Simulation

def running_sim():
    sim = Simulation(); sim.power_led_generator.mode = PowerLedSourceMode.ON; sim.step(20); assert sim.pc_state_machine.state is PcState.RUNNING; return sim

def start_forced(sim):
    sim.inputs.power_button = True
    ctx, _ = sim.step(20)
    return ctx

def test_real_forced_progress_uses_visual_duration_but_controller_indefinite():
    sim = running_sim(); ctx = start_forced(sim)
    assert ctx.transition is Transition.FORCED_SHUTDOWN
    assert ctx.transition_duration_ms == defaults.POWER_HOLD_FORCED_MS
    assert ctx.transition_progress == 0
    sim.step(2000); ctx = sim.context()
    assert ctx.transition_progress == 127
    sim.step(2000); ctx = sim.context()
    assert ctx.transition_progress == 255
    sim.step(1000); ctx = sim.context()
    assert ctx.transition_progress == 255
    assert sim.effect_controller.current is Transition.FORCED_SHUTDOWN
    assert sim.effect_controller.duration(Transition.FORCED_SHUTDOWN) == 0

def test_short_release_switches_to_shutdown_context():
    sim = running_sim(); start_forced(sim); sim.step(100); sim.inputs.power_button = False; ctx, _ = sim.step(20)
    assert sim.effect_controller.current is Transition.SHUTDOWN
    assert ctx.transition is Transition.SHUTDOWN
    assert ctx.transition_duration_ms == defaults.SHUTDOWN_DURATION_MS
    assert ctx.transition_progress < 20

def test_release_exactly_at_threshold_remains_forced_with_full_progress():
    sim = running_sim(); start_forced(sim); sim.step(defaults.POWER_HOLD_FORCED_MS); sim.inputs.power_button = False; ctx, _ = sim.step(0)
    assert sim.pc_state_machine.forced_latched
    assert sim.effect_controller.current is Transition.FORCED_SHUTDOWN
    assert ctx.transition_progress == 255

def test_forced_preview_timing_and_no_state_mutation():
    sim = running_sim(); state = sim.pc_state_machine.state; current = sim.effect_controller.current
    sim.render_override = "Force ForcedShutdown"; sim.restart_preview(); ctx, _ = sim.step(0)
    assert ctx.preview_progress == 0
    sim.step(defaults.POWER_HOLD_FORCED_MS // 2); ctx = sim.context()
    assert ctx.preview_progress == 127
    sim.step(defaults.POWER_HOLD_FORCED_MS); ctx = sim.context()
    assert ctx.preview_progress == 255
    assert sim.pc_state_machine.state is state and sim.effect_controller.current is current

def test_forced_decision_equivalent_for_frame_sizes():
    def scenario(step):
        sim = running_sim(); sim.inputs.power_button = True; sim.step(step)
        for _ in range(defaults.POWER_HOLD_FORCED_MS // step): sim.step(step)
        ctx = sim.context(); return sim.pc_state_machine.forced_latched, ctx.transition_progress
    assert scenario(5) == scenario(20) == scenario(80) == (True, 255)
