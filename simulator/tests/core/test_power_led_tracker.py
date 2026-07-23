from avr_aurora_sim.power_led_tracker import PowerLedMode, PowerLedTracker, PowerLedTrackerConfig

def tracker(): return PowerLedTracker(PowerLedTrackerConfig(100, 10, 100, 50, 4))

def test_steady_off_and_on():
    t = tracker(); t.update(False, 0); assert t.mode(200) is PowerLedMode.OFF
    t = tracker(); t.update(True, 0); assert t.mode(200) is PowerLedMode.ON

def test_short_off_gap_remains_on():
    t = tracker(); t.update(True, 0); t.update(False, 10); assert t.mode(50) is PowerLedMode.ON; assert t.mode(200) is PowerLedMode.OFF

def test_valid_blinking_detection_and_stale():
    t = tracker(); active = False
    for now in (0, 20, 40, 60, 80):
        active = not active; t.update(active, now)
    assert t.mode(90) is PowerLedMode.BLINKING
    assert t.mode(200) is PowerLedMode.ON

def test_invalid_blink_frequencies_rejected():
    for half_period in (5, 150):
        t = tracker(); active = False
        for i in range(5): active = not active; t.update(active, i * half_period)
        assert t.mode(5 * half_period) is not PowerLedMode.BLINKING
