from avr_aurora_sim.input_edges import InputEdgeTracker

def test_press_release_once_and_held_quiet():
    t = InputEdgeTracker()
    e = t.update(True, False); assert e.power_button_pressed and not e.power_button_released
    e = t.update(True, False); assert not e.power_button_pressed and not e.power_button_released
    e = t.update(False, False); assert e.power_button_released and not e.power_button_pressed
    e = t.update(False, True); assert e.reset_button_pressed
    e = t.update(False, True); assert not e.reset_button_pressed
