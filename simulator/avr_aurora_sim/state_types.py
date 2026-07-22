from enum import Enum

class PcState(str, Enum):
    OFF = "Off"
    STARTING = "Starting"
    RUNNING = "Running"
    SLEEPING = "Sleeping"
    AWAIT_SHUTDOWN = "AwaitShutdown"
    WARN = "Warn"

class Transition(str, Enum):
    NONE = "None"
    STARTUP = "Startup"
    SHUTDOWN = "Shutdown"
    FORCED_SHUTDOWN = "ForcedShutdown"
    RESET = "Reset"
