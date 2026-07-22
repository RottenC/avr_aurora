from dataclasses import dataclass

@dataclass(frozen=True)
class InputEdges:
    power_button_pressed: bool = False
    power_button_released: bool = False
    reset_button_pressed: bool = False

class InputEdgeTracker:
    """Per-frame edge tracker; intentionally not an electrical debounce model."""
    def __init__(self) -> None:
        self.previous_power = False
        self.previous_reset = False

    def reset(self, power_button: bool = False, reset_button: bool = False) -> None:
        self.previous_power = power_button
        self.previous_reset = reset_button

    def update(self, power_button: bool, reset_button: bool) -> InputEdges:
        edges = InputEdges(
            power_button_pressed=(not self.previous_power) and power_button,
            power_button_released=self.previous_power and (not power_button),
            reset_button_pressed=(not self.previous_reset) and reset_button,
        )
        self.previous_power = power_button
        self.previous_reset = reset_button
        return edges
