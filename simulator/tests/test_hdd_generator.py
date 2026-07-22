from avr_aurora_sim.hdd_generator import HddGenerator, HddMode, HddParams
from avr_aurora_sim.simulation import Simulation

def seq(seed, mode):
    generator = HddGenerator(seed, mode)
    return [generator.update(20, False) for _ in range(50)]

def test_deterministic_same_seed():
    assert seq(7, HddMode.RANDOM) == seq(7, HddMode.RANDOM)

def test_different_seeds():
    assert seq(7, HddMode.RANDOM) != seq(8, HddMode.RANDOM)

def test_manual_mode_off_to_on_counts_one_active_edge():
    generator = HddGenerator(1, HddMode.MANUAL)
    assert generator.update(20, False)[1] == []
    _, transitions = generator.update(20, True)
    assert sum(t.active_edge for t in transitions) == 1
    _, transitions = generator.update(20, True)
    assert sum(t.active_edge for t in transitions) == 0

def test_one_complete_pulse_has_one_counted_edge_and_falling_not_counted():
    generator = HddGenerator(1, HddMode.LIGHT, HddParams(activity_rate=100, pulse_duration_ms=10, burst_size=1, randomness=0))
    _, rising = generator.update(10, False)
    _, falling = generator.update(10, False)
    assert [(t.active, t.active_edge) for t in rising + falling] == [(True, True), (False, False)]
    assert sum(t.active_edge for t in rising + falling) == 1

def test_activity_modes_emit_active_edges():
    for mode in (HddMode.LIGHT, HddMode.MEDIUM, HddMode.HEAVY, HddMode.RANDOM):
        generator = HddGenerator(3, mode)
        assert any(any(t.active_edge for t in edges) for _, edges in (generator.update(20, False) for _ in range(200)))

def _activity_after(step_ms):
    sim = Simulation()
    sim.set_hdd_mode(HddMode.RANDOM)
    for _ in range(80 // step_ms):
        sim.step(step_ms)
    return sim.state.hdd_activity

def test_hdd_activity_equivalent_for_frame_sizes():
    assert _activity_after(80) == _activity_after(20) == _activity_after(5)

def test_hdd_activity_never_exceeds_max():
    sim = Simulation()
    sim.set_hdd_mode(HddMode.HEAVY)
    for _ in range(200):
        sim.step(80)
        assert 0 <= sim.state.hdd_activity <= sim.config.hdd_max

def test_pending_hdd_edge_cleared_by_restart():
    sim = Simulation()
    sim.inputs.manual_hdd_led = True
    sim.step(5)
    assert sim._pending_edge_count == 1
    sim.inputs.manual_hdd_led = False
    sim.restart()
    sim.step(10)
    assert sim.state.hdd_activity == 0


def test_manual_and_generated_hdd_state_are_separate():
    sim = Simulation()
    assert sim.inputs.manual_hdd_led is False
    sim.set_hdd_mode(HddMode.HEAVY)
    sim.generator.params.activity_rate = 100
    sim.step(20)
    assert sim.state.raw_hdd_led is True
    sim.set_hdd_mode(HddMode.MANUAL)
    sim.step(20)
    assert sim.inputs.manual_hdd_led is False
    assert sim.state.raw_hdd_led is False
