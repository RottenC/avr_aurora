import os
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
from PySide6.QtWidgets import QApplication, QScrollArea, QSplitter
from avr_aurora_sim.hdd_generator import HddMode
from avr_aurora_sim.main_window import MainWindow
from avr_aurora_sim.power_led_generator import PowerLedSourceMode


def test_main_window_offscreen_smoke():
    app = QApplication.instance() or QApplication([])
    window = MainWindow(); window.timer.stop(); window.diag_timer.stop(); window.show(); app.processEvents()
    assert window.findChild(QScrollArea) is not None
    assert window.findChild(QSplitter) is not None
    window._tick(True)
    window.power_source.setCurrentText(PowerLedSourceMode.BLINKING.value)
    window.power_blink_half.setValue(300)
    window.mode.setCurrentText(HddMode.HEAVY.value)
    window.render_mode.setCurrentText("Force ForcedShutdown")
    for i in range(window.sim.diagnostics.max_events + 2): window.sim.diagnostics.record("smoke", (i,), i)
    window._refresh_diagnostics(force=True)
    assert window.diag_table.rowCount() == window.sim.diagnostics.max_events
    window.resize(900, 700); app.processEvents(); window._tick(True); app.processEvents()
    assert "Rendered effect:" in window.status.text()
    assert "Preview override" in window.status.text()
    window.close(); app.processEvents()
