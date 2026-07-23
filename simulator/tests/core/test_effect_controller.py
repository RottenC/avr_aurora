from avr_aurora_sim.effect_controller import EffectController
from avr_aurora_sim.state_types import PcState, Transition

def test_priority_and_repeated_request():
    c = EffectController(); c.request(Transition.STARTUP, 10); assert c.current is Transition.STARTUP
    c.request(Transition.STARTUP, 20); assert c.started_at_ms == 10
    c.request(Transition.RESET, 30); assert c.current is Transition.RESET
    c.request(Transition.STARTUP, 40); assert c.current is Transition.RESET

def test_restart_completion_and_reconcile():
    c = EffectController(); c.request(Transition.RESET, 0); c.restart(Transition.RESET, 100); assert c.started_at_ms == 100
    c.update(999); assert c.consume_finished() is Transition.NONE
    c.update(1000); assert c.consume_finished() is Transition.RESET
    c.request(Transition.STARTUP, 0); c.reconcile(PcState.RUNNING); assert c.current is Transition.NONE

def test_forced_lifetime():
    c = EffectController(); c.request(Transition.FORCED_SHUTDOWN, 0); c.update(999999); assert c.current is Transition.FORCED_SHUTDOWN
    c.reconcile(PcState.OFF); assert c.current is Transition.NONE
