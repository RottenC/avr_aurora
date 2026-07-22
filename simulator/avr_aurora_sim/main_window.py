from PySide6.QtCore import QTimer, Qt
from PySide6.QtWidgets import *
from .simulation import Simulation
from .hdd_generator import HddMode
from .led_canvas import LedCanvas

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__(); self.setWindowTitle("AVR Aurora simulator — AVR-like mode"); self.sim=Simulation(); self.paused=False; self.speed=1.0
        root=QWidget(); self.setCentralWidget(root); layout=QHBoxLayout(root)
        left=QVBoxLayout(); layout.addLayout(left); right=QVBoxLayout(); layout.addLayout(right,1)
        self.power=QPushButton("Power"); self.power.pressed.connect(lambda:self._set('power_button',True)); self.power.released.connect(lambda:self._set('power_button',False)); left.addWidget(self.power)
        self.reset=QPushButton("Reset"); self.reset.pressed.connect(lambda:self._set('reset_button',True)); self.reset.released.connect(lambda:self._set('reset_button',False)); left.addWidget(self.reset)
        for name,attr,checked in [("Power LED",'power_led',False),("HDD LED",'hdd_led',False),("Strip power",'strip_power',True)]:
            cb=QCheckBox(name); cb.setChecked(checked); cb.toggled.connect(lambda v,a=attr:self._set(a,v)); left.addWidget(cb)
        self.mode=QComboBox(); self.mode.addItems([m.value for m in HddMode]); self.mode.currentTextChanged.connect(self._mode); left.addWidget(QLabel("HDD mode")); left.addWidget(self.mode)
        self.seed=QSpinBox(); self.seed.setRange(0,2**31-1); self.seed.setValue(1); self.seed.valueChanged.connect(self._seed); left.addWidget(QLabel("Seed")); left.addWidget(self.seed)
        for label in ["Activity rate","Pulse duration","Burst size","Randomness"]: left.addWidget(QSpinBox()); left.itemAt(left.count()-1).widget().setPrefix(label+": ")
        self.pause=QPushButton("Pause"); self.pause.clicked.connect(self._pause); left.addWidget(self.pause)
        step=QPushButton("Step one frame"); step.clicked.connect(lambda:self._tick(True)); left.addWidget(step)
        restart=QPushButton("Restart simulation"); restart.clicked.connect(lambda:(self.sim.restart(),self._refresh())); left.addWidget(restart)
        self.speedbox=QComboBox(); self.speedbox.addItems(["0.25x","1x","4x"]); self.speedbox.setCurrentText("1x"); self.speedbox.currentTextChanged.connect(lambda t:setattr(self,'speed',float(t[:-1]))); left.addWidget(self.speedbox)
        self.fps=QSpinBox(); self.fps.setRange(1,200); self.fps.setValue(50); self.fps.valueChanged.connect(lambda v:setattr(self.sim.config,'frame_interval_ms',max(1,1000//v))); left.addWidget(self.fps)
        self.indices=QCheckBox("Show LED indices"); self.indices.toggled.connect(self._indices); left.addWidget(self.indices)
        self.strict=QCheckBox("Strict mode"); self.strict.toggled.connect(lambda v:setattr(self.sim.diagnostics,'strict',v)); left.addWidget(self.strict)
        clear=QPushButton("Clear diagnostics"); clear.clicked.connect(lambda:(self.sim.diagnostics.clear(),self._refresh())); left.addWidget(clear); left.addStretch()
        self.linear=LedCanvas(False); self.physical=LedCanvas(True); right.addWidget(QLabel("Linear 0..55")); right.addWidget(self.linear); right.addWidget(QLabel("Physical U-shaped layout")); right.addWidget(self.physical)
        self.status=QLabel(); self.status.setAlignment(Qt.AlignTop); right.addWidget(self.status)
        self.timer=QTimer(self); self.timer.timeout.connect(lambda:self._tick(False)); self.timer.start(20); self._refresh()
    def _set(self,a,v): setattr(self.sim.inputs,a,bool(v))
    def _mode(self,t): self.sim.generator.set_mode(HddMode(t))
    def _seed(self,v): self.sim.state.random_seed=v; self.sim.generator.reset(v)
    def _pause(self): self.paused=not self.paused; self.pause.setText("Resume" if self.paused else "Pause")
    def _indices(self,v): self.linear.show_indices=v; self.physical.show_indices=v; self.linear.update(); self.physical.update()
    def _tick(self,force):
        if self.paused and not force: return
        try: self.sim.step(int(self.sim.config.frame_interval_ms*self.speed)); self._refresh()
        except Exception as e: self.paused=True; QMessageBox.critical(self,"Strict diagnostic",str(e))
    def _refresh(self):
        leds=self.sim.led_frame.pixels if self.sim.inputs.strip_power else [(0,0,0)]*56; self.linear.set_leds(leds); self.physical.set_leds(leds)
        s=self.sim.state; i=self.sim.inputs; c=self.sim.diagnostics.counters
        self.power.setText(f"Power hold: {s.power_hold_ms/1000:.2f} / 4.00 s")
        self.status.setText(f"time={s.now_ms} ms\nframe={s.frame_number}\nFPS target={1000//self.sim.config.frame_interval_ms}\npc_state={s.pc_state.value}\ntransition={s.transition.value}\nraw Power LED={i.power_led}\nraw HDD LED={i.hdd_led}\nsmoothed HDD={s.hdd_activity}/128\npower hold={s.power_hold_ms} ms\nstrip power={i.strip_power}\nseed={s.random_seed}\nDiagnostics:\n"+'\n'.join(f"{k}: {v}" for k,v in sorted(c.items())))
