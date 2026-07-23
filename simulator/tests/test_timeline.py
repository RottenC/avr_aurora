from avr_aurora_sim.timeline_model import TimelineHistory, TimelineSample


def sample(t, state="Running", transition="None"):
    return TimelineSample(t, False, "Off", False, 0, state, transition)


def test_timeline_retention_is_time_based_and_bounded():
    history = TimelineHistory(window_ms=15000, bucket_ms=20)
    for t in range(0, 60000, 5):
        history.add_sample(sample(t))
    latest_input_ms = 59995
    latest_stored_ms = history.samples[-1].now_ms
    assert 0 <= latest_input_ms - latest_stored_ms < history.bucket_ms
    assert history.samples[0].now_ms >= 59995 - 15000 - 20
    assert len(history.samples) <= 800


def test_timeline_keeps_immediate_state_changes_and_clears():
    history = TimelineHistory(window_ms=15000, bucket_ms=20)
    history.add_sample(sample(0, "Off")); history.add_sample(sample(5, "Running"))
    assert [s.pc_state for s in history.samples] == ["Off", "Running"]
    history.clear(); assert len(history.samples) == 0

def test_timeline_has_absolute_sample_bound_at_same_timestamp():
    history = TimelineHistory(window_ms=15000, bucket_ms=20, max_samples=32)
    states = ["Off", "Running"]
    for i in range(1000):
        history.add_sample(sample(100, states[i % 2]))
    assert len(history.samples) == 32
    assert history.samples[0].now_ms == 100


def test_timeline_model_has_no_pyside_dependency():
    import sys
    import avr_aurora_sim.timeline_model  # noqa: F401
    assert "PySide6.QtGui" not in sys.modules
    assert "PySide6.QtWidgets" not in sys.modules
