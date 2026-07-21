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
PowerLedTracker powerLed({Config::ShortPowerLedOffIgnoreMs,
                         Config::PowerLedBlinkMinHalfPeriodMs,
                         Config::PowerLedBlinkMaxHalfPeriodMs,
                         Config::PowerLedBlinkStaleMs,
                         Config::PowerLedBlinkEdgesRequired});
HddActivity hdd({Config::HddUpdateMs,
                 Config::HddEdgeBoost,
                 Config::HddActiveRise,
                 Config::HddInactiveDecay,
                 Config::HddMax});
PcStateMachine pc({Config::PowerHoldForcedMs, Config::StartingTimeoutMs});
EffectController effects({Config::StartupDurationMs,
                          Config::ShutdownDurationMs,
                          Config::ResetDurationMs});
LedOutput ledOutput;
SerialDebug debug;
uint32_t lastFrameMs = 0;
uint32_t lastHddUpdateMs = 0;

void setup() {
  inputs.begin();
  ledOutput.begin();
  debug.begin();
  lastHddUpdateMs = millis();
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
    hdd.update(in.hddLed, inputs.consumeHddEdges(), elapsed);
  }

  effects.update(now);
  const TransitionEffect finishedEffect = effects.consumeFinished();
  const PcStateInputs pcInputs = {
      in.stripPowerPresent,
      in.powerButton,
      inputs.powerButtonPressed(),
      inputs.powerButtonReleased(),
      inputs.resetButtonPressed(),
      powerMode,
      finishedEffect == TransitionEffect::Startup};
  const PcStateEvents events = pc.update(pcInputs, now);

  if (events.cancelStartup) effects.cancel(TransitionEffect::Startup);
  if (events.cancelForcedShutdown) effects.cancel(TransitionEffect::ForcedShutdown);
  if (events.requestStartup) effects.restart(TransitionEffect::Startup, now);
  if (events.requestShutdown) effects.request(TransitionEffect::Shutdown, now);
  if (events.requestReset) effects.request(TransitionEffect::Reset, now);
  if (events.requestForcedShutdown) {
    effects.request(TransitionEffect::ForcedShutdown, pc.powerHoldStartMs());
  }
  effects.reconcile(pc.state());

  if (now - lastFrameMs >= Config::FrameIntervalMs) {
    lastFrameMs = now;
    ledOutput.render(pc.state(), effects.current(), effects.startedAt(), hdd.value(), in.stripPowerPresent, now);
  }

  debug.update(in, pc.state(), effects.current(), powerMode, hdd.value(), now);
}
