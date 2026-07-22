import pytest
from avr_aurora_sim.avr_math import clamp_u8, div_trunc, qadd8, qsub8, require_u8, scale8, u8, u16
from avr_aurora_sim.diagnostics import Diagnostics, StrictDiagnosticError

def test_wraps_and_counters():
    diagnostics = Diagnostics()
    assert u8(260, diagnostics) == 4
    assert u16(70000, diagnostics) == 4464
    assert diagnostics.counters["u8_wrap"] == 1
    assert diagnostics.counters["u16_wrap"] == 1

def test_scale8_uses_uint8_inputs_and_records_wrap():
    diagnostics = Diagnostics()
    assert scale8(255, 255, diagnostics) == 255
    assert scale8(255, 0, diagnostics) == 0
    assert scale8(128, 128, diagnostics) == 64
    assert scale8(300, 128, diagnostics, "scale") == 22
    assert diagnostics.counters["u8_wrap"] == 1

def test_qadd8_and_qsub8_use_uint8_inputs():
    diagnostics = Diagnostics()
    assert qadd8(300, 250, diagnostics, "add") == 255  # 44 + 250 saturates
    assert qsub8(300, 50, diagnostics, "sub") == 0      # 44 - 50 saturates
    assert diagnostics.counters["u8_wrap"] == 2
    assert diagnostics.counters["saturating_add"] == 1
    assert diagnostics.counters["saturating_sub"] == 1

def test_explicit_clamp_records_without_strict_raise():
    diagnostics = Diagnostics(strict=True)
    assert clamp_u8(300, diagnostics, "intentional") == 255
    assert diagnostics.counters["clamped_value"] == 1

def test_unexpected_validation_raises_in_strict_mode():
    diagnostics = Diagnostics(strict=True)
    with pytest.raises(StrictDiagnosticError):
        require_u8(300, diagnostics, "bad")

def test_truncating_signed_division_toward_zero():
    diagnostics = Diagnostics()
    assert div_trunc(-7, 3, diagnostics) == -2
    assert div_trunc(7, -3, diagnostics) == -2
    assert div_trunc(-7, -3, diagnostics) == 2
    assert diagnostics.counters["truncating_division"] == 3
