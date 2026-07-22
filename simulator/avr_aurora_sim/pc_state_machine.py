from dataclasses import dataclass
from . import firmware_defaults as defaults
from .input_edges import InputEdges
from .power_led_tracker import PowerLedMode
from .state_types import PcState

@dataclass(frozen=True)
class PcStateInputs:
    strip_power_present: bool
    power_button: bool
    edges: InputEdges
    power_mode: PowerLedMode
    startup_transition_finished: bool

@dataclass
class PcStateConfig:
    forced_hold_ms: int = defaults.POWER_HOLD_FORCED_MS
    starting_timeout_ms: int = defaults.STARTING_TIMEOUT_MS
    shutdown_warning_timeout_ms: int = defaults.SHUTDOWN_WARNING_TIMEOUT_MS

@dataclass
class PcStateEvents:
    request_startup: bool = False
    request_shutdown: bool = False
    request_reset: bool = False
    request_forced_shutdown: bool = False
    cancel_startup: bool = False
    cancel_forced_shutdown: bool = False

class PcStateMachine:
    def __init__(self, config: PcStateConfig | None = None) -> None:
        self.config = config or PcStateConfig()
        self.state = PcState.OFF
        self.power_hold_start_ms = 0
        self.starting_since_ms = 0
        self.awaiting_shutdown_since_ms = 0
        self.tracking_hold = False
        self.forced_latched = False
        self.startup_transition_requested = False
        self.startup_transition_finished = False
        self.waiting_for_strip_power = False

    def reset(self) -> None:
        self.__init__(self.config)

    @property
    def power_hold_duration_ms(self) -> int:
        return 0

    def hold_duration(self, now_ms: int) -> int:
        return now_ms - self.power_hold_start_ms if self.tracking_hold else 0

    def starting_elapsed(self, now_ms: int) -> int:
        return now_ms - self.starting_since_ms if self.state is PcState.STARTING else 0

    def await_shutdown_elapsed(self, now_ms: int) -> int:
        return now_ms - self.awaiting_shutdown_since_ms if self.state is PcState.AWAIT_SHUTDOWN else 0

    def enter_off(self) -> None:
        self.state = PcState.OFF
        self.tracking_hold = False
        self.forced_latched = False
        self.startup_transition_requested = False
        self.startup_transition_finished = False
        self.waiting_for_strip_power = False

    def enter_starting(self, events: PcStateEvents, strip_power_present: bool, now_ms: int) -> None:
        self.state = PcState.STARTING
        self.starting_since_ms = now_ms
        self.tracking_hold = False
        self.forced_latched = False
        self.startup_transition_finished = False
        self.waiting_for_strip_power = not strip_power_present
        self.startup_transition_requested = strip_power_present
        events.request_startup = strip_power_present

    def leave_starting(self, events: PcStateEvents, next_state: PcState) -> None:
        if self.startup_transition_requested:
            events.cancel_startup = True
        self.state = next_state
        self.startup_transition_requested = False
        self.startup_transition_finished = False
        self.waiting_for_strip_power = False

    def enter_await_shutdown(self, now_ms: int) -> None:
        self.state = PcState.AWAIT_SHUTDOWN
        self.awaiting_shutdown_since_ms = now_ms

    def update(self, inputs: PcStateInputs, now_ms: int) -> PcStateEvents:
        events = PcStateEvents()
        if self.state is PcState.OFF:
            if inputs.edges.power_button_pressed:
                self.enter_starting(events, inputs.strip_power_present, now_ms)
            elif inputs.power_mode is PowerLedMode.BLINKING:
                self.state = PcState.SLEEPING
            elif inputs.power_mode is PowerLedMode.ON:
                self.state = PcState.RUNNING
        elif self.state is PcState.STARTING:
            if inputs.power_mode is PowerLedMode.BLINKING:
                self.leave_starting(events, PcState.SLEEPING)
            elif inputs.power_mode is PowerLedMode.OFF and now_ms - self.starting_since_ms >= self.config.starting_timeout_ms:
                self.leave_starting(events, PcState.OFF)
            elif not inputs.strip_power_present:
                if self.startup_transition_requested:
                    events.cancel_startup = True
                self.startup_transition_requested = False
                self.startup_transition_finished = False
                self.waiting_for_strip_power = True
            else:
                if self.waiting_for_strip_power or not self.startup_transition_requested:
                    self.waiting_for_strip_power = False
                    self.startup_transition_requested = True
                    self.startup_transition_finished = False
                    events.request_startup = True
                elif inputs.startup_transition_finished:
                    self.startup_transition_finished = True
                if self.startup_transition_finished and inputs.power_mode is PowerLedMode.ON:
                    self.leave_starting(events, PcState.RUNNING)
        elif self.state is PcState.RUNNING:
            if inputs.edges.reset_button_pressed:
                events.request_reset = True
            if inputs.edges.power_button_pressed:
                self.tracking_hold = True
                self.forced_latched = False
                self.power_hold_start_ms = now_ms
                events.request_forced_shutdown = True
            if self.tracking_hold and inputs.power_button and not self.forced_latched and now_ms - self.power_hold_start_ms >= self.config.forced_hold_ms:
                self.forced_latched = True
            if self.tracking_hold and inputs.edges.power_button_released:
                held_long_enough = self.forced_latched or now_ms - self.power_hold_start_ms >= self.config.forced_hold_ms
                self.tracking_hold = False
                self.forced_latched = held_long_enough
                self.enter_await_shutdown(now_ms)
                if not held_long_enough:
                    events.cancel_forced_shutdown = True
                    events.request_shutdown = True
            elif not self.tracking_hold:
                if inputs.power_mode is PowerLedMode.BLINKING:
                    self.state = PcState.SLEEPING
                elif inputs.power_mode is PowerLedMode.OFF:
                    self.enter_off()
        elif self.state is PcState.SLEEPING:
            if inputs.power_mode is PowerLedMode.OFF:
                self.enter_off()
            elif inputs.power_mode is PowerLedMode.ON:
                self.state = PcState.RUNNING
        elif self.state is PcState.AWAIT_SHUTDOWN:
            if inputs.power_mode is PowerLedMode.OFF:
                self.enter_off()
            elif inputs.power_mode is PowerLedMode.ON and now_ms - self.awaiting_shutdown_since_ms >= self.config.shutdown_warning_timeout_ms:
                self.state = PcState.WARN
        elif self.state is PcState.WARN:
            if inputs.power_mode is PowerLedMode.OFF:
                self.enter_off()
        return events
