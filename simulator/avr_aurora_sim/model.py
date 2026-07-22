from dataclasses import dataclass, field
from .state_types import PcState, Transition
LED_COUNT=56
RGB=tuple[int,int,int]

@dataclass
class SimulatorConfig:
    led_count:int=LED_COUNT; frame_interval_ms:int=20; target_fps:int=50
    power_hold_forced_ms:int=4000; hdd_update_ms:int=10
    hdd_edge_boost:int=20; hdd_active_rise:int=3; hdd_inactive_decay:int=2; hdd_max:int=128

@dataclass
class InputState:
    power_button:bool=False; reset_button:bool=False; power_led:bool=False; hdd_led:bool=False; strip_power:bool=True

@dataclass
class FrameContext:
    now_ms:int; dt_ms:int; frame_number:int; pc_state:PcState; transition:Transition
    power_button:bool; reset_button:bool; power_led:bool; hdd_led:bool; strip_power:bool; hdd_activity:int

@dataclass
class LedFrame:
    pixels:list[RGB]=field(default_factory=lambda:[(0,0,0)]*LED_COUNT)
    def set(self,index:int,rgb:RGB,diagnostics=None,label:str="") -> None:
        if not 0 <= index < LED_COUNT:
            if diagnostics: diagnostics.record("invalid_led_index",(index,),None,label,True)
            return
        if any(c<0 or c>255 for c in rgb):
            if diagnostics: diagnostics.record("invalid_rgb_write",(index,*rgb),rgb,label,True)
            rgb=tuple(max(0,min(255,c)) for c in rgb) # type: ignore
        self.pixels[index]=rgb

@dataclass
class SimulationState:
    now_ms:int=0; frame_number:int=0; pc_state:PcState=PcState.OFF; transition:Transition=Transition.NONE
    hdd_activity:int=0; power_hold_ms:int=0; random_seed:int=1
