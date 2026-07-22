from dataclasses import dataclass
from enum import Enum
import random

class HddMode(str, Enum):
    MANUAL="Manual"; LIGHT="Light"; MEDIUM="Medium"; HEAVY="Heavy"; RANDOM="Random / Quake"

@dataclass
class HddParams:
    activity_rate:int=35; pulse_duration_ms:int=60; burst_size:int=3; randomness:int=30

PRESETS={
    HddMode.LIGHT:HddParams(12,45,1,20), HddMode.MEDIUM:HddParams(45,70,4,35),
    HddMode.HEAVY:HddParams(85,90,8,25), HddMode.RANDOM:HddParams(55,80,6,85), HddMode.MANUAL:HddParams(0,50,1,0)
}

class HddGenerator:
    def __init__(self, seed:int=1, mode:HddMode=HddMode.MANUAL, params:HddParams|None=None):
        self.seed=seed; self.rng=random.Random(seed); self.mode=mode; self.params=params or PRESETS[mode]; self.raw=False; self.remaining_ms=0
    def reset(self, seed:int|None=None):
        if seed is not None: self.seed=seed
        self.rng=random.Random(self.seed); self.raw=False; self.remaining_ms=0
    def set_mode(self, mode:HddMode):
        self.mode=mode; self.params=PRESETS[mode]; self.reset()
    def update(self, dt_ms:int, manual:bool=False):
        if self.mode is HddMode.MANUAL: return manual, []
        edges=[]; elapsed=0
        while elapsed < dt_ms:
            if self.remaining_ms <= 0:
                old=self.raw
                if self.mode is HddMode.HEAVY: self.raw = self.rng.randrange(100) < self.params.activity_rate
                elif self.mode is HddMode.RANDOM: self.raw = self.rng.randrange(100) < max(5,min(95,self.params.activity_rate+self.rng.randrange(-self.params.randomness,self.params.randomness+1)))
                else: self.raw = (not self.raw) if self.rng.randrange(100) < self.params.activity_rate else False
                base=self.params.pulse_duration_ms if self.raw else self.params.pulse_duration_ms*self.params.burst_size
                jitter=(base*self.params.randomness)//100
                self.remaining_ms=max(5, base + (self.rng.randrange(-jitter,jitter+1) if jitter else 0))
                if self.raw != old: edges.append((elapsed,self.raw))
            step=min(dt_ms-elapsed,self.remaining_ms); elapsed+=step; self.remaining_ms-=step
        return self.raw, edges
