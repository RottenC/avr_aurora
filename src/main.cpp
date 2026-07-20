#include <Arduino.h>
#include "config.h"
#include "inputs.h"
#include "power_led_tracker.h"
#include "hdd_activity.h"
#include "pc_state.h"
#include "effect_controller.h"
#include "led_output.h"
#include "serial_debug.h"

Inputs inputs;
PowerLedTracker powerLed;
HddActivity hdd;
PcStateMachine pc;
EffectController effects;
LedOutput ledOutput;
SerialDebug debug;
uint32_t lastFrameMs = 0;

void setup() {
  inputs.begin();
  ledOutput.begin();
  debug.begin();
}

void loop() {
  const uint32_t now = millis();
  inputs.update(now);
  const NormalizedInputs &in = inputs.state();
  powerLed.update(in.powerLed, now);
  const PowerLedMode powerMode = powerLed.mode(now);
  hdd.update(in.hddLed);
  pc.update(in, inputs.powerButtonPressed(), inputs.powerButtonReleased(), inputs.resetButtonPressed(), powerMode, now);
  if ((pc.startupRequested() || (pc.state() == PcState::Starting && !effects.active())) && in.stripPowerPresent) effects.request(TransitionEffect::Startup, now);
  if (pc.shutdownRequested()) effects.request(TransitionEffect::Shutdown, now);
  if (pc.resetRequested()) effects.request(TransitionEffect::Reset, now);
  if (pc.forcedPreview()) effects.request(TransitionEffect::ForcedShutdown, pc.powerHoldStartMs());
  effects.update(pc, in.stripPowerPresent, now);
  if (now - lastFrameMs >= Config::FrameIntervalMs) {
    lastFrameMs = now;
    ledOutput.render(pc.state(), effects.current(), effects.startedAt(), hdd.value(), in.stripPowerPresent, now);
  }
  debug.update(in, pc.state(), effects.current(), powerMode, hdd.value(), now);
}
