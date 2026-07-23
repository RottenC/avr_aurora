import os
import subprocess
import sys


def test_core_imports_do_not_load_qt_modules():
    code = """
import sys
from avr_aurora_sim.simulation import Simulation
from avr_aurora_sim.model import SimulatorConfig
from avr_aurora_sim.timeline_model import TimelineHistory
from avr_aurora_sim.diagnostics import Diagnostics
sim = Simulation()
ctx, leds = sim.step(20)
assert ctx.pc_state.value
assert len(leds.to_list()) == sim.config.led_count
for name in ('PySide6', 'PySide6.QtCore', 'PySide6.QtGui', 'PySide6.QtWidgets'):
    assert name not in sys.modules, name
"""
    env = os.environ.copy()
    env["PYTHONPATH"] = "simulator"
    subprocess.run([sys.executable, "-c", code], check=True, env=env)
