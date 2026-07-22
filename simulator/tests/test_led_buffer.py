import pytest
from avr_aurora_sim.diagnostics import Diagnostics, StrictDiagnosticError
from avr_aurora_sim.model import LED_COUNT, LedBuffer

def test_valid_writes_and_length():
    buffer = LedBuffer(Diagnostics())
    buffer[0] = (1, 2, 3)
    assert len(buffer) == LED_COUNT
    assert buffer[0] == (1, 2, 3)
    assert len(buffer.to_list()) == LED_COUNT

def test_invalid_index_records_and_non_strict_safe():
    diagnostics = Diagnostics()
    buffer = LedBuffer(diagnostics)
    buffer[99] = (1, 2, 3)
    assert diagnostics.counters["invalid_led_index"] == 1
    assert len(buffer.to_list()) == LED_COUNT

def test_invalid_rgb_records_and_clamps_non_strict():
    diagnostics = Diagnostics()
    buffer = LedBuffer(diagnostics)
    buffer[0] = (300, -1, 4)
    assert diagnostics.counters["invalid_rgb_write"] == 1
    assert buffer[0] == (255, 0, 4)

def test_strict_raises_for_index_and_rgb():
    diagnostics = Diagnostics(strict=True)
    buffer = LedBuffer(diagnostics)
    with pytest.raises(StrictDiagnosticError):
        buffer[99] = (1, 2, 3)
    with pytest.raises(StrictDiagnosticError):
        buffer[0] = (1, 2, 300)
