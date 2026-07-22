from abc import ABC, abstractmethod
from ..diagnostics import Diagnostics
from ..model import FrameContext, LedBuffer
class Effect(ABC):
    def reset(self, context: FrameContext) -> None: pass
    @abstractmethod
    def render(self, context: FrameContext, leds: LedBuffer, diagnostics: Diagnostics) -> None: ...
