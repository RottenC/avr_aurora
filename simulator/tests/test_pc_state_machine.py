from avr_aurora_sim.input_edges import InputEdges
from avr_aurora_sim.pc_state_machine import PcStateInputs, PcStateMachine
from avr_aurora_sim.power_led_tracker import PowerLedMode
from avr_aurora_sim.state_types import PcState

def inp(power=PowerLedMode.OFF, strip=True, button=False, press=False, release=False, reset=False, startup_done=False):
    return PcStateInputs(strip, button, InputEdges(press, release, reset), power, startup_done)

def test_initial_reconciliation_running_and_sleeping():
    m = PcStateMachine(); m.update(inp(PowerLedMode.ON), 0); assert m.state is PcState.RUNNING
    m = PcStateMachine(); m.update(inp(PowerLedMode.BLINKING), 0); assert m.state is PcState.SLEEPING

def test_cold_startup_delayed_strip_and_loss():
    m = PcStateMachine(); e = m.update(inp(strip=False, press=True), 0); assert m.state is PcState.STARTING and not e.request_startup
    e = m.update(inp(strip=True), 10); assert e.request_startup
    e = m.update(inp(strip=False), 20); assert e.cancel_startup

def test_startup_complete_timeout_and_sleep():
    m = PcStateMachine(); m.update(inp(press=True), 0); e = m.update(inp(PowerLedMode.ON, startup_done=True), 1); assert m.state is PcState.RUNNING and e.cancel_startup
    m = PcStateMachine(); m.update(inp(press=True), 0); m.update(inp(PowerLedMode.OFF), 30000); assert m.state is PcState.OFF
    m = PcStateMachine(); m.update(inp(press=True), 0); m.update(inp(PowerLedMode.BLINKING), 10); assert m.state is PcState.SLEEPING

def test_sleep_wake_reset_shutdown_forced_warn():
    m = PcStateMachine(); m.update(inp(PowerLedMode.BLINKING), 0); m.update(inp(PowerLedMode.ON), 1); assert m.state is PcState.RUNNING
    e = m.update(inp(PowerLedMode.ON, reset=True), 2); assert e.request_reset
    e = m.update(inp(PowerLedMode.ON, button=True, press=True), 3); assert e.request_forced_shutdown
    e = m.update(inp(PowerLedMode.ON, release=True), 100); assert e.cancel_forced_shutdown and e.request_shutdown and m.state is PcState.AWAIT_SHUTDOWN
    m.update(inp(PowerLedMode.OFF), 101); assert m.state is PcState.OFF
    m.update(inp(PowerLedMode.ON), 200); m.update(inp(PowerLedMode.ON, button=True, press=True), 201); m.update(inp(PowerLedMode.ON, button=True), 4201); e = m.update(inp(PowerLedMode.ON, release=True), 4201); assert not e.request_shutdown and m.forced_latched
    m.update(inp(PowerLedMode.ON), 124202); assert m.state is PcState.WARN
