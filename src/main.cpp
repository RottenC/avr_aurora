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
PcStateMachine pc({Config::PowerHoldForcedMs, Config::StartingTimeoutMs});
EffectController effects;
LedOutput ledOutput;
SerialDebug debug;
uint32_t lastFrameMs = 0;
uint32_t lastHddUpdateMs = 0;
volatile uint8_t hddEdgeCounter = 0;

void hddEdgeIsr() {
  if (hddEdgeCounter < 255) ++hddEdgeCounter;
}

uint8_t consumeHddEdges() {
  noInterrupts();
  const uint8_t count = hddEdgeCounter;
  hddEdgeCounter = 0;
  interrupts();
  return count;
}

void setup() {
  inputs.begin();
  attachInterrupt(digitalPinToInterrupt(Config::HddLedPin), hddEdgeIsr, Config::HddLedActiveHigh ? RISING : FALLING);
  ledOutput.begin();
  debug.begin();
}

void loop() {
  const uint32_t now = millis();
  inputs.update(now);
  const NormalizedInputs &in = inputs.state();
  powerLed.update(in.powerLed, now);
  const PowerLedMode powerMode = powerLed.mode(now);
  if (now - lastHddUpdateMs >= Config::HddUpdateMs) {
    const uint16_t elapsed = static_cast<uint16_t>(now - lastHddUpdateMs);
    lastHddUpdateMs = now;
    hdd.update(in.hddLed, consumeHddEdges(), elapsed);
  }

  const PcStateInputs pcInputs = {in.stripPowerPresent, in.powerButton, inputs.powerButtonPressed(), inputs.powerButtonReleased(), inputs.resetButtonPressed(), powerMode, effects.consumeFinished()};
  const PcStateEvents pcEvents = pc.update(pcInputs, now);
  if (pcEvents.cancelStartupRequested) effects.cancel(TransitionEffect::Startup);
  if (pcEvents.forcedShutdownCancelRequested) effects.cancel(TransitionEffect::ForcedShutdown);
  if (pcEvents.startupRequested) effects.restart(TransitionEffect::Startup, now);
  if (pcEvents.shutdownRequested) effects.request(TransitionEffect::Shutdown, now);
  if (pcEvents.resetRequested) effects.request(TransitionEffect::Reset, now);
  if (pcEvents.forcedShutdownRequested) effects.request(TransitionEffect::ForcedShutdown, pc.powerHoldStartMs());
  effects.update(now);

  if (now - lastFrameMs >= Config::FrameIntervalMs) {
    lastFrameMs = now;
    ledOutput.render(pc.state(), effects.current(), effects.startedAt(), hdd.value(), in.stripPowerPresent, now);
  }
  debug.update(in, pc.state(), effects.current(), powerMode, hdd.value(), now);
}
