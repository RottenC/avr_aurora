from PySide6.QtCore import QSignalBlocker, QTimer, Qt
from PySide6.QtWidgets import *
from .hdd_generator import HddMode, HddParams
from .led_canvas import LedCanvas
from .power_led_generator import PowerLedSourceMode
from .simulation import Simulation
from .timeline import TimelineSample, TimelineWidget

RENDER_MODES = ["Auto", "Force Aurora", "Force Startup", "Force Shutdown", "Force Reset", "Force ForcedShutdown", "Force Sleep", "Force Warn", "Force Off"]

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__(); self.setWindowTitle("AVR Aurora simulator — AVR-like mode")
        self.sim = Simulation(); self.paused = False; self.speed = 1.0; self._diag_revision = -1; self._diag_filter = ""
        splitter = QSplitter(Qt.Horizontal); self.setCentralWidget(splitter)
        controls = QWidget(); left = QVBoxLayout(controls)
        scroll = QScrollArea(); scroll.setWidgetResizable(True); scroll.setWidget(controls); splitter.addWidget(scroll)
        right_widget = QWidget(); right = QVBoxLayout(right_widget); splitter.addWidget(right_widget); splitter.setStretchFactor(1, 1)

        self.power = QPushButton(); self.power.pressed.connect(lambda: self._set_input("power_button", True)); self.power.released.connect(lambda: self._set_input("power_button", False)); left.addWidget(self.power)
        self.reset = QPushButton("Reset"); self.reset.pressed.connect(lambda: self._set_input("reset_button", True)); self.reset.released.connect(lambda: self._set_input("reset_button", False)); left.addWidget(self.reset)
        self.power_led = QCheckBox("Manual Power LED"); self.power_led.toggled.connect(lambda value: self._set_input("manual_power_led", value)); left.addWidget(self.power_led)
        self.power_source = QComboBox(); self.power_source.addItems([m.value for m in PowerLedSourceMode]); self.power_source.currentTextChanged.connect(self._power_source_changed); left.addWidget(QLabel("Power LED source")); left.addWidget(self.power_source)
        self.power_blink_half = QSpinBox(); self.power_blink_half.setRange(100, 5000); self.power_blink_half.setSuffix(" ms half-period"); self.power_blink_half.setValue(500); self.power_blink_half.valueChanged.connect(self._power_half_changed); left.addWidget(self.power_blink_half)
        self.raw_power = QLabel("Raw Power LED: False"); left.addWidget(self.raw_power)
        self.hdd_led = QCheckBox("Manual HDD LED"); self.hdd_led.toggled.connect(lambda value: self._set_input("manual_hdd_led", value)); left.addWidget(self.hdd_led)
        self.raw_hdd = QLabel("Raw HDD LED: False"); left.addWidget(self.raw_hdd)
        self.strip_power = QCheckBox("Strip power"); self.strip_power.setChecked(True); self.strip_power.toggled.connect(lambda value: self._set_input("strip_power", value)); left.addWidget(self.strip_power)
        self.mode = QComboBox(); self.mode.addItems([mode.value for mode in HddMode]); self.mode.currentTextChanged.connect(self._mode_changed); left.addWidget(QLabel("HDD mode")); left.addWidget(self.mode)
        self.seed = QSpinBox(); self.seed.setRange(0, 2**31 - 1); self.seed.setValue(1); self.seed.valueChanged.connect(self._seed_changed); left.addWidget(QLabel("Seed")); left.addWidget(self.seed)
        self.activity_rate = self._param_spin("Activity rate", 0, 100, "%", left); self.pulse_duration = self._param_spin("Pulse duration", 5, 5000, " ms", left); self.burst_size = self._param_spin("Burst size", 1, 100, " pulses", left); self.randomness = self._param_spin("Randomness", 0, 100, "%", left)
        self.render_mode = QComboBox(); self.render_mode.addItems(RENDER_MODES); self.render_mode.currentTextChanged.connect(self._render_mode_changed); left.addWidget(QLabel("Render mode")); left.addWidget(self.render_mode)
        preview = QPushButton("Restart preview"); preview.clicked.connect(lambda: (self.sim.restart_preview(), self._refresh())); left.addWidget(preview)
        self.pause = QPushButton("Pause"); self.pause.clicked.connect(self._pause); left.addWidget(self.pause)
        step = QPushButton("Step one frame"); step.clicked.connect(lambda: self._tick(True)); left.addWidget(step)
        restart = QPushButton("Restart simulation"); restart.clicked.connect(self._restart); left.addWidget(restart)
        self.speedbox = QComboBox(); self.speedbox.addItems(["0.25x", "1x", "4x"]); self.speedbox.setCurrentText("1x"); self.speedbox.currentTextChanged.connect(lambda text: setattr(self, "speed", float(text[:-1]))); left.addWidget(self.speedbox)
        self.fps = QSpinBox(); self.fps.setRange(1, 200); self.fps.setValue(50); self.fps.setSuffix(" FPS"); self.fps.valueChanged.connect(self._fps_changed); left.addWidget(self.fps)
        self.indices = QCheckBox("Show LED indices"); self.indices.toggled.connect(self._indices); left.addWidget(self.indices)
        self.strict = QCheckBox("Strict mode"); self.strict.toggled.connect(lambda value: setattr(self.sim.diagnostics, "strict", value)); left.addWidget(self.strict)
        self.filter = QLineEdit(); self.filter.setPlaceholderText("Diagnostic operation filter"); self.filter.textChanged.connect(self._filter_changed); left.addWidget(self.filter)
        clear = QPushButton("Clear diagnostics"); clear.clicked.connect(self._clear_diagnostics); left.addWidget(clear); left.addStretch()

        views = QSplitter(Qt.Vertical); right.addWidget(views)
        led_box = QWidget(); led_layout = QVBoxLayout(led_box); self.linear = LedCanvas(False); self.physical = LedCanvas(True); led_layout.addWidget(QLabel("Linear 0..55")); led_layout.addWidget(self.linear); led_layout.addWidget(QLabel("Physical U-shaped layout")); led_layout.addWidget(self.physical); views.addWidget(led_box)
        self.timeline = TimelineWidget(); views.addWidget(self.timeline)
        lower = QSplitter(Qt.Vertical); self.status = QLabel(); self.status.setAlignment(Qt.AlignTop); lower.addWidget(self.status); self.diag_table = QTableWidget(0, 6); self.diag_table.setHorizontalHeaderLabels(["time", "frame", "operation", "label", "inputs", "result"]); lower.addWidget(self.diag_table); views.addWidget(lower)

        self.timer = QTimer(self); self.timer.timeout.connect(lambda: self._tick(False)); self.timer.start(self.sim.config.frame_interval_ms)
        self.diag_timer = QTimer(self); self.diag_timer.timeout.connect(self._refresh_diagnostics_if_needed); self.diag_timer.start(200)
        self._sync_controls_from_model(); self._refresh(); self._refresh_diagnostics(force=True)

    def _param_spin(self, name, minimum, maximum, suffix, layout):
        spin = QSpinBox(); spin.setRange(minimum, maximum); spin.setPrefix(name + ": "); spin.setSuffix(suffix); spin.valueChanged.connect(self._params_changed); layout.addWidget(spin); return spin
    def _set_input(self, attr, value): setattr(self.sim.inputs, attr, bool(value))
    def _power_source_changed(self, text): self.sim.set_power_led_mode(PowerLedSourceMode(text)); self.power_led.setEnabled(PowerLedSourceMode(text) is PowerLedSourceMode.MANUAL)
    def _power_half_changed(self, value): self.sim.set_power_led_half_period_ms(value)
    def _mode_changed(self, text): self.sim.set_hdd_mode(HddMode(text)); self._sync_param_controls(); self.hdd_led.setEnabled(HddMode(text) is HddMode.MANUAL)
    def _seed_changed(self, value): self.sim.state.random_seed = value; self.sim.generator.reset(value)
    def _params_changed(self): self.sim.set_hdd_params(HddParams(self.activity_rate.value(), self.pulse_duration.value(), self.burst_size.value(), self.randomness.value()))
    def _render_mode_changed(self, text): self.sim.render_override = text; self.sim.restart_preview()
    def _fps_changed(self, fps): interval = max(1, round(1000 / fps)); self.sim.config.target_fps = fps; self.sim.config.frame_interval_ms = interval; self.timer.setInterval(interval)
    def _pause(self): self.paused = not self.paused; self.pause.setText("Resume" if self.paused else "Pause")
    def _indices(self, value): self.linear.show_indices = value; self.physical.show_indices = value; self.linear.update(); self.physical.update()
    def _restart(self): self.sim.restart(); self.timeline.clear(); self._sync_controls_from_model(); self._refresh(); self._refresh_diagnostics(force=True)
    def _tick(self, force):
        if self.paused and not force: return
        try:
            ctx, _ = self.sim.step(round(self.sim.config.frame_interval_ms * self.speed), self.sim.config.frame_interval_ms)
            self.timeline.add_sample(TimelineSample(ctx.now_ms, ctx.raw_power_led, ctx.power_led_mode.value, ctx.raw_hdd_led, ctx.hdd_activity, ctx.pc_state.value, ctx.transition.value)); self._refresh()
        except Exception as error:
            self.paused = True; self.pause.setText("Resume"); QMessageBox.critical(self, "Strict diagnostic", str(error)); self._refresh_diagnostics(force=True)
    def _sync_param_controls(self):
        params = self.sim.generator.params
        for spin, value in [(self.activity_rate, params.activity_rate), (self.pulse_duration, params.pulse_duration_ms), (self.burst_size, params.burst_size), (self.randomness, params.randomness)]: blocker = QSignalBlocker(spin); spin.setValue(value); del blocker
    def _sync_controls_from_model(self):
        self._sync_param_controls(); self.fps.setValue(self.sim.config.target_fps); self.timer.setInterval(self.sim.config.frame_interval_ms)
        for widget, value in [(self.power_led, self.sim.inputs.manual_power_led), (self.hdd_led, self.sim.inputs.manual_hdd_led), (self.strip_power, self.sim.inputs.strip_power), (self.strict, self.sim.diagnostics.strict)]: blocker = QSignalBlocker(widget); widget.setChecked(value); del blocker
        self.hdd_led.setEnabled(self.sim.generator.mode is HddMode.MANUAL); self.power_led.setEnabled(self.sim.power_led_generator.mode is PowerLedSourceMode.MANUAL)
    def _refresh(self):
        leds = self.sim.led_buffer.to_list() if self.sim.inputs.strip_power else [(0, 0, 0)] * self.sim.config.led_count; self.linear.set_leds(leds); self.physical.set_leds(leds)
        s = self.sim.state; i = self.sim.inputs; pc = self.sim.pc_state_machine; ec = self.sim.effect_controller; control_ctx = s.last_control_context or self.sim.context(); render_ctx = s.last_render_context or control_ctx
        self.raw_power.setText(f"Raw Power LED: {s.raw_power_led}"); self.raw_hdd.setText(f"Raw HDD LED: {s.raw_hdd_led}"); self.power.setText(f"Power hold: {pc.hold_duration(s.now_ms)/1000:.2f} / {self.sim.config.power_hold_forced_ms/1000:.2f} s")
        controller_lifetime = "indefinite" if ec.duration() == 0 and ec.current.value != "None" else f"{ec.duration()} ms"
        source = "Auto" if self.sim.render_override == "Auto" else "Preview override"
        lines = [f"Simulated time: {s.now_ms} ms", f"Frame: {s.frame_number}", f"Target FPS: {self.sim.config.target_fps} ({self.sim.config.frame_interval_ms} ms)", f"Power LED source: {self.sim.power_led_generator.mode.value}", f"Raw Power LED: {s.raw_power_led}", f"Classified Power LED: {s.power_led_mode.value}", "Control plane:", f"  PC state: {control_ctx.pc_state.value}", f"  controller transition: {control_ctx.transition.value}", f"  controller lifetime: {controller_lifetime}", "Rendered effect:", f"  effect/transition: {render_ctx.transition.value}", f"  elapsed: {render_ctx.transition_elapsed_ms} ms", f"  duration: {render_ctx.transition_duration_ms} ms", f"  progress: {render_ctx.transition_progress}", f"  source: {source}", f"Manual Power LED: {i.manual_power_led}", f"Manual HDD LED: {i.manual_hdd_led}", f"Raw HDD LED: {s.raw_hdd_led}", f"Active HDD edges this frame: {s.active_edges_this_frame}", f"Pending active edges: {self.sim._pending_edge_count}", f"Pending HDD milliseconds: {s.hdd_pending_ms}", f"Smoothed HDD activity: {s.hdd_activity}/{self.sim.config.hdd_max}", f"Power hold duration: {pc.hold_duration(s.now_ms)} ms", f"Forced shutdown latched: {pc.forced_latched}", f"Starting elapsed: {pc.starting_elapsed(s.now_ms)} ms", f"Await-shutdown elapsed: {pc.await_shutdown_elapsed(s.now_ms)} ms", f"Strip power: {i.strip_power}", f"Current random seed: {s.random_seed}", f"Preview elapsed/duration/progress: {self.sim.preview_elapsed_ms()} / {self.sim.preview_duration_ms()} ms / {self.sim.preview_progress8()}", "Diagnostics:"]
        lines.extend(f"  {key}: {value}" for key, value in sorted(self.sim.diagnostics.counters.items())); self.status.setText("\n".join(lines))
    def _filter_changed(self, text): self._diag_filter = text; self._refresh_diagnostics(force=True)
    def _clear_diagnostics(self): self.sim.diagnostics.clear(); self._diag_revision = -1; self.diag_table.setRowCount(0); self._refresh()
    def _refresh_diagnostics_if_needed(self): self._refresh_diagnostics(force=False)
    def _refresh_diagnostics(self, force=False):
        revision = self.sim.diagnostics.revision
        if not force and revision == self._diag_revision and self.filter.text() == self._diag_filter: return
        self._diag_revision = revision; self._diag_filter = self.filter.text() if hasattr(self, "filter") else ""
        events = [e for e in reversed(self.sim.diagnostics.events) if not self._diag_filter or self._diag_filter in e.operation]
        self.diag_table.setRowCount(len(events))
        for row, event in enumerate(events):
            for col, value in enumerate([event.now_ms, event.frame_number, event.operation, event.label, event.inputs, event.result]): self.diag_table.setItem(row, col, QTableWidgetItem(str(value)))
        if events: self.diag_table.selectRow(0)
