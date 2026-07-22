import pytest
from avr_aurora_sim.avr_math import u8,u16,qadd8,qsub8,scale8,clamp_u8
from avr_aurora_sim.diagnostics import Diagnostics, StrictDiagnosticError

def test_wraps_and_counters():
    d=Diagnostics(); assert u8(260,d)==4; assert u16(70000,d)==4464
    assert d.counters['u8_wrap']==1 and d.counters['u16_wrap']==1

def test_saturating_and_scale8():
    d=Diagnostics(); assert qadd8(250,20,d)==255; assert qsub8(5,9,d)==0; assert scale8(128,128,d)==64
    assert d.counters['saturating_add']==1 and d.counters['saturating_sub']==1

def test_strict_invalid_value():
    d=Diagnostics(strict=True)
    with pytest.raises(StrictDiagnosticError): clamp_u8(300,d,'bad')
