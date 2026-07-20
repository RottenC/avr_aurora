#pragma once
#include <Arduino.h>
#include "inputs.h"
#include "pc_state.h"
#include "effect_controller.h"
class SerialDebug { public: void begin(); void update(const NormalizedInputs &in, PcState state, TransitionEffect transition, PowerLedMode powerMode, uint8_t hdd, uint32_t nowMs); private: uint32_t lastMs_=0; };
