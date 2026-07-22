from .model import InputState, SimulationState, SimulatorConfig, FrameContext, LedFrame
from .state_types import PcState, Transition
from .diagnostics import Diagnostics
from .avr_math import qadd8, qsub8, clamp_u8
from .hdd_generator import HddGenerator, HddMode
from .effects import PlaceholderEffect

class Simulation:
    def __init__(self):
        self.config=SimulatorConfig(); self.inputs=InputState(); self.state=SimulationState()
        self.diagnostics=Diagnostics(); self.generator=HddGenerator(self.state.random_seed); self.effect=PlaceholderEffect(); self.led_frame=LedFrame()
    def restart(self):
        seed=self.state.random_seed; strict=self.diagnostics.strict
        self.__init__(); self.state.random_seed=seed; self.generator.reset(seed); self.diagnostics.strict=strict
    def context(self, dt_ms:int=0):
        return FrameContext(self.state.now_ms,dt_ms,self.state.frame_number,self.state.pc_state,self.state.transition,self.inputs.power_button,self.inputs.reset_button,self.inputs.power_led,self.inputs.hdd_led,self.inputs.strip_power,self.state.hdd_activity)
    def step(self, dt_ms:int|None=None):
        dt_ms = self.config.frame_interval_ms if dt_ms is None else int(dt_ms)
        self.state.now_ms += dt_ms; self.state.frame_number += 1
        raw, edges = self.generator.update(dt_ms, self.inputs.hdd_led)
        self.inputs.hdd_led = raw
        self.state.power_hold_ms = self.state.power_hold_ms + dt_ms if self.inputs.power_button else 0
        self.state.pc_state = PcState.RUNNING if self.inputs.power_led or self.inputs.strip_power else PcState.OFF
        self.state.transition = Transition.RESET if self.inputs.reset_button else Transition.NONE
        ticks=max(1, dt_ms//self.config.hdd_update_ms)
        for _ in edges: self.state.hdd_activity=qadd8(self.state.hdd_activity,self.config.hdd_edge_boost,self.diagnostics,"hdd edge")
        if self.inputs.hdd_led: self.state.hdd_activity=qadd8(self.state.hdd_activity,ticks*self.config.hdd_active_rise,self.diagnostics,"hdd active")
        else: self.state.hdd_activity=qsub8(self.state.hdd_activity,ticks*self.config.hdd_inactive_decay,self.diagnostics,"hdd decay")
        self.state.hdd_activity=clamp_u8(self.state.hdd_activity,self.diagnostics,"hdd clamp")
        if self.state.hdd_activity>self.config.hdd_max: self.state.hdd_activity=self.config.hdd_max; self.diagnostics.record("clamped_value",("hdd",),self.config.hdd_max,"hdd max")
        self.diagnostics.set_context(self.state.frame_number,self.state.now_ms)
        ctx=self.context(dt_ms); pixels=[(0,0,0)]*self.config.led_count
        if self.inputs.strip_power: self.effect.render(ctx,pixels,self.diagnostics)
        self.led_frame.pixels=pixels
        if dt_ms > self.config.frame_interval_ms: self.diagnostics.record("slow_frame",(dt_ms,),self.config.frame_interval_ms,"target interval")
        return ctx, self.led_frame
