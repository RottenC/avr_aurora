#pragma once

#include <stdint.h>

enum class PcState : uint8_t { Off, Starting, Running, Sleeping, AwaitShutdown, Warn };
enum class TransitionEffect : uint8_t { None, Startup, Shutdown, ForcedShutdown, Reset };
enum class PowerLedMode : uint8_t { Off, On, Blinking };
