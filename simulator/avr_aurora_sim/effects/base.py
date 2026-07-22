from abc import ABC, abstractmethod
from ..model import FrameContext, RGB
from ..diagnostics import Diagnostics
class Effect(ABC):
    def reset(self, context:FrameContext)->None: pass
    @abstractmethod
    def render(self, context:FrameContext, leds:list[RGB], diagnostics:Diagnostics)->None: ...
