from avr_aurora_sim.diagnostics import Diagnostics
from avr_aurora_sim.simulation import Simulation

def test_event_history_is_bounded_but_counters_continue():
    diagnostics = Diagnostics(max_events=3)
    for i in range(10): diagnostics.record("event", (i,), i)
    assert diagnostics.counters["event"] == 10 and len(diagnostics.events) == 3

def test_clear_removes_counters_and_events():
    diagnostics = Diagnostics(max_events=3); diagnostics.record("event"); diagnostics.clear()
    assert diagnostics.counters == {} and len(diagnostics.events) == 0

def test_simulation_events_have_current_frame_context():
    sim = Simulation(); sim.inputs.manual_hdd_led = True; sim.step(400)
    event = next(event for event in sim.diagnostics.events if event.operation == "clamped_value")
    assert event.frame_number == sim.state.frame_number
    assert event.now_ms == sim.state.now_ms
