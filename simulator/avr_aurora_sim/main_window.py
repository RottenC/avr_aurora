from PySide6.QtCore import QSignalBlocker, QTimer, Qt
from PySide6.QtWidgets import *
from .hdd_generator import HddMode, HddParams
from .led_canvas import LedCanvas
from .simulation import Simulation

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AVR Aurora simulator — AVR-like mode")
        self.sim = Simulation()
        self.paused = False
        self.speed = 1.0
        root = QWidget(); self.setCentralWidget(root); layout = QHBoxLayout(root)
        left = QVBoxLayout(); layout.addLayout(left); right = QVBoxLayout(); layout.addLayout(right, 1)

        self.power = QPushButton(); self.power.pressed.connect(lambda: self._set_input("power_button", True)); self.power.released.connect(lambda: self._set_input("power_button", False)); left.addWidget(self.power)
        self.reset = QPushButton("Reset"); self.reset.pressed.connect(lambda: self._set_input("reset_button", True)); self.reset.released.connect(lambda: self._set_input("reset_button", False)); left.addWidget(self.reset)
        self.power_led = QCheckBox("Power LED"); self.power_led.toggled.connect(lambda value: self._set_input("power_led", value)); left.addWidget(self.power_led)
        self.hdd_led = QCheckBox("HDD LED"); self.hdd_led.toggled.connect(lambda value: self._set_input("hdd_led", value)); left.addWidget(self.hdd_led)
        self.strip_power = QCheckBox("Strip power"); self.strip_power.setChecked(True); self.strip_power.toggled.connect(lambda value: self._set_input("strip_power", value)); left.addWidget(self.strip_power)

        self.mode = QComboBox(); self.mode.addItems([mode.value for mode in HddMode]); self.mode.currentTextChanged.connect(self._mode_changed); left.addWidget(QLabel("HDD mode")); left.addWidget(self.mode)
        self.seed = QSpinBox(); self.seed.setRange(0, 2**31 - 1); self.seed.setValue(1); self.seed.valueChanged.connect(self._seed_changed); left.addWidget(QLabel("Seed")); left.addWidget(self.seed)
        self.activity_rate = self._param_spin("Activity rate", 0, 100, "%", left)
        self.pulse_duration = self._param_spin("Pulse duration", 5, 5000, " ms", left)
        self.burst_size = self._param_spin("Burst size", 1, 100, " pulses", left)
        self.randomness = self._param_spin("Randomness", 0, 100, "%", left)

        self.pause = QPushButton("Pause"); self.pause.clicked.connect(self._pause); left.addWidget(self.pause)
        step = QPushButton("Step one frame"); step.clicked.connect(lambda: self._tick(True)); left.addWidget(step)
        restart = QPushButton("Restart simulation"); restart.clicked.connect(self._restart); left.addWidget(restart)
        self.speedbox = QComboBox(); self.speedbox.addItems(["0.25x", "1x", "4x"]); self.speedbox.setCurrentText("1x"); self.speedbox.currentTextChanged.connect(lambda text: setattr(self, "speed", float(text[:-1]))); left.addWidget(self.speedbox)
        self.fps = QSpinBox(); self.fps.setRange(1, 200); self.fps.setValue(50); self.fps.setSuffix(" FPS"); self.fps.valueChanged.connect(self._fps_changed); left.addWidget(self.fps)
        self.indices = QCheckBox("Show LED indices"); self.indices.toggled.connect(self._indices); left.addWidget(self.indices)
        self.strict = QCheckBox("Strict mode"); self.strict.toggled.connect(lambda value: setattr(self.sim.diagnostics, "strict", value)); left.addWidget(self.strict)
        clear = QPushButton("Clear diagnostics"); clear.clicked.connect(lambda: (self.sim.diagnostics.clear(), self._refresh())); left.addWidget(clear); left.addStretch()

        self.linear = LedCanvas(False); self.physical = LedCanvas(True)
        right.addWidget(QLabel("Linear 0..55")); right.addWidget(self.linear)
        right.addWidget(QLabel("Physical U-shaped layout")); right.addWidget(self.physical)
        self.status = QLabel(); self.status.setAlignment(Qt.AlignTop); right.addWidget(self.status)
        self.timer = QTimer(self); self.timer.timeout.connect(lambda: self._tick(False)); self.timer.start(self.sim.config.frame_interval_ms)
        self._sync_controls_from_model(); self._refresh()

    def _param_spin(self, name, minimum, maximum, suffix, layout):
        spin = QSpinBox(); spin.setRange(minimum, maximum); spin.setPrefix(name + ": "); spin.setSuffix(suffix); spin.valueChanged.connect(self._params_changed); layout.addWidget(spin); return spin
    def _set_input(self, attr, value): setattr(self.sim.inputs, attr, bool(value))
    def _mode_changed(self, text):
        self.sim.set_hdd_mode(HddMode(text)); self._sync_param_controls()
    def _seed_changed(self, value): self.sim.state.random_seed = value; self.sim.generator.reset(value)
    def _params_changed(self):
        self.sim.set_hdd_params(HddParams(self.activity_rate.value(), self.pulse_duration.value(), self.burst_size.value(), self.randomness.value()))
    def _fps_changed(self, fps):
        interval = max(1, round(1000 / fps))
        self.sim.config.target_fps = fps; self.sim.config.frame_interval_ms = interval; self.timer.setInterval(interval)
    def _pause(self): self.paused = not self.paused; self.pause.setText("Resume" if self.paused else "Pause")
    def _indices(self, value): self.linear.show_indices = value; self.physical.show_indices = value; self.linear.update(); self.physical.update()
    def _restart(self): self.sim.restart(); self._sync_controls_from_model(); self._refresh()
    def _tick(self, force):
        if self.paused and not force: return
        try:
            self.sim.step(round(self.sim.config.frame_interval_ms * self.speed), self.sim.config.frame_interval_ms)
            self._refresh()
        except Exception as error:
            self.paused = True; self.pause.setText("Resume"); QMessageBox.critical(self, "Strict diagnostic", str(error))
    def _sync_param_controls(self):
        params = self.sim.generator.params
        for spin, value in [(self.activity_rate, params.activity_rate), (self.pulse_duration, params.pulse_duration_ms), (self.burst_size, params.burst_size), (self.randomness, params.randomness)]:
            blocker = QSignalBlocker(spin); spin.setValue(value); del blocker
    def _sync_controls_from_model(self):
        self._sync_param_controls(); self.fps.setValue(self.sim.config.target_fps); self.timer.setInterval(self.sim.config.frame_interval_ms)
        for widget, value in [(self.power_led, self.sim.inputs.power_led), (self.hdd_led, self.sim.inputs.hdd_led), (self.strip_power, self.sim.inputs.strip_power), (self.strict, self.sim.diagnostics.strict)]:
            blocker = QSignalBlocker(widget); widget.setChecked(value); del blocker
    def _refresh(self):
        leds = self.sim.led_buffer.to_list() if self.sim.inputs.strip_power else [(0, 0, 0)] * 56
        self.linear.set_leds(leds); self.physical.set_leds(leds)
        state = self.sim.state; inputs = self.sim.inputs; counters = self.sim.diagnostics.counters
        self.power.setText(f"Power hold: {state.power_hold_ms / 1000:.2f} / 4.00 s")
        lines = [
            f"Simulated time: {state.now_ms} ms",
            f"Frame: {state.frame_number}",
            f"Target FPS: {self.sim.config.target_fps} ({self.sim.config.frame_interval_ms} ms)",
            f"PC state: {state.pc_state.value}",
            f"Transition: {state.transition.value}",
            f"Raw Power LED: {inputs.power_led}",
            f"Raw HDD LED: {inputs.hdd_led}",
            f"Smoothed HDD activity: {state.hdd_activity}/128",
            f"Power hold duration: {state.power_hold_ms} ms",
            f"Strip power: {inputs.strip_power}",
            f"Current random seed: {state.random_seed}",
            "Diagnostics:",
        ]
        lines.extend(f"  {key}: {value}" for key, value in sorted(counters.items()))
        self.status.setText("\n".join(lines))
