from avr_aurora_sim.power_led_generator import PowerLedGenerator, PowerLedSourceMode
from avr_aurora_sim.power_led_tracker import PowerLedMode, PowerLedTracker
from avr_aurora_sim.simulation import Simulation

def drive_tracker(half_period, step_ms, total_ms):
    gen = PowerLedGenerator(PowerLedSourceMode.BLINKING, half_period)
    tracker = PowerLedTracker()
    now = 0
    for _ in range(total_ms // step_ms):
        start = now; now += step_ms
        raw, transitions = gen.update(step_ms, False)
        for transition in transitions:
            tracker.update(transition.active, start + transition.offset_ms)
        tracker.update(raw, now)
    return raw, tracker.mode(now), tracker.blink_edges

def test_blinking_large_step_returns_all_boundaries():
    gen = PowerLedGenerator(PowerLedSourceMode.BLINKING, 150)
    raw, transitions = gen.update(400, False)
    assert raw is False
    assert [(t.offset_ms, t.active) for t in transitions] == [(150, True), (300, False)]

def test_boundary_belongs_to_current_step_without_duplicate():
    gen = PowerLedGenerator(PowerLedSourceMode.BLINKING, 150)
    _, first = gen.update(150, False)
    _, second = gen.update(150, False)
    assert [(t.offset_ms, t.active) for t in first] == [(150, True)]
    assert [(t.offset_ms, t.active) for t in second] == [(150, False)]

def test_manual_off_on_modes_emit_only_changes():
    gen = PowerLedGenerator(PowerLedSourceMode.MANUAL)
    assert gen.update(10, False)[1] == []
    assert [(t.offset_ms, t.active) for t in gen.update(10, True)[1]] == [(0, True)]
    assert gen.update(10, True)[1] == []
    gen.mode = PowerLedSourceMode.OFF
    assert [(t.offset_ms, t.active) for t in gen.update(10, True)[1]] == [(0, False)]

def test_power_led_classification_frame_size_independent():
    for half in (150, 151, 500):
        results = {step: drive_tracker(half, step, 12000) for step in (5, 20, 80, 400)}
        assert len(set(results.values())) == 1, (half, results)

def test_valid_fast_slow_and_stale_classification():
    assert drive_tracker(150, 400, 12000)[1] is PowerLedMode.BLINKING
    assert drive_tracker(100, 20, 2000)[1] is not PowerLedMode.BLINKING
    assert drive_tracker(5000, 400, 20000)[1] is not PowerLedMode.BLINKING
    raw, mode, _ = drive_tracker(150, 400, 12000)
    assert mode is PowerLedMode.BLINKING and raw in (True, False)

def test_simulation_large_power_led_step_matches_small_steps():
    def run(step):
        sim = Simulation(); sim.power_led_generator.mode = PowerLedSourceMode.BLINKING; sim.power_led_generator.half_period_ms = 150
        for _ in range(12000 // step): sim.step(step)
        return sim.state.raw_power_led, sim.state.power_led_mode, sim.power_led_tracker.blink_edges
    assert len({run(step) for step in (5, 20, 80, 400)}) == 1

def transition_list(gen, dt=5, manual=False):
    raw, transitions = gen.update(dt, manual)
    return raw, [(t.offset_ms, t.active) for t in transitions]


def test_source_mode_transitions_are_timestamped_at_zero():
    gen = PowerLedGenerator(PowerLedSourceMode.OFF)
    gen.update(10, False)
    gen.set_mode(PowerLedSourceMode.BLINKING)
    assert transition_list(gen) == (False, [])

    gen = PowerLedGenerator(PowerLedSourceMode.ON); gen.update(10, False)
    gen.set_mode(PowerLedSourceMode.BLINKING)
    assert transition_list(gen) == (False, [(0, False)])

    gen = PowerLedGenerator(PowerLedSourceMode.MANUAL); gen.update(10, True)
    gen.set_mode(PowerLedSourceMode.BLINKING)
    assert transition_list(gen) == (False, [(0, False)])

    gen = PowerLedGenerator(PowerLedSourceMode.BLINKING, 5); gen.update(5, False)
    gen.set_mode(PowerLedSourceMode.ON)
    assert transition_list(gen) == (True, [])

    gen = PowerLedGenerator(PowerLedSourceMode.BLINKING, 5); gen.update(5, False)
    gen.set_mode(PowerLedSourceMode.OFF)
    assert transition_list(gen) == (False, [(0, False)])

    gen = PowerLedGenerator(PowerLedSourceMode.BLINKING, 20); gen.update(5, False)
    gen.set_mode(PowerLedSourceMode.MANUAL)
    assert transition_list(gen, manual=True) == (True, [(0, True)])
    gen.set_mode(PowerLedSourceMode.BLINKING); gen.update(5, False); gen.set_mode(PowerLedSourceMode.MANUAL)
    assert transition_list(gen, manual=False) == (False, [])


def test_same_power_mode_does_not_restart_phase_and_period_change_is_deterministic():
    gen = PowerLedGenerator(PowerLedSourceMode.BLINKING, 150)
    gen.update(100, False)
    gen.set_mode(PowerLedSourceMode.BLINKING)
    raw, transitions = gen.update(50, False)
    assert raw is True and [(t.offset_ms, t.active) for t in transitions] == [(50, True)]
    gen.set_half_period_ms(200)
    assert transition_list(gen, 10) == (False, [(0, False)])


def test_simulation_blinking_first_frame_initializes_tracker_at_start():
    sim = Simulation(); sim.set_power_led_mode(PowerLedSourceMode.BLINKING); sim.set_power_led_half_period_ms(150)
    sim.step(400)
    assert sim.power_led_tracker.blink_edges == 2
    assert sim.power_led_tracker.last_change_ms == 300


def test_power_mode_change_equivalent_across_frame_sizes():
    def scenario(step):
        sim = Simulation(); sim.set_power_led_mode(PowerLedSourceMode.ON); sim.step(20)
        sim.set_power_led_mode(PowerLedSourceMode.BLINKING)
        for _ in range(400 // step): sim.step(step)
        return sim.state.raw_power_led, sim.state.power_led_mode, sim.power_led_tracker.blink_edges, sim.power_led_tracker.last_change_ms
    assert len({scenario(step) for step in (5, 20, 80, 400)}) == 1
