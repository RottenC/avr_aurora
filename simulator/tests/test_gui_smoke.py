import os
import pytest
pytest.importorskip("PySide6")
from PySide6.QtWidgets import QApplication
from avr_aurora_sim.hdd_generator import HddMode
from avr_aurora_sim.main_window import MainWindow
from avr_aurora_sim.power_led_generator import PowerLedSourceMode


def test_main_window_offscreen_smoke():
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    app = QApplication.instance() or QApplication([])
    window = MainWindow(); window.timer.stop(); window.diag_timer.stop()
    window._tick(True)
    window._power_source_changed(PowerLedSourceMode.BLINKING.value)
    window._mode_changed(HddMode.HEAVY.value)
    window._render_mode_changed("Force ForcedShutdown")
    window._refresh(); window._refresh_diagnostics(force=True)
    window.close()
