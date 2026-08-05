#include <Arduino.h>

#include "avr/avr_config.h"
#include "avr/avr_inputs.h"
#include "avr/avr_led_driver.h"
#include "avr/avr_serial_debug.h"
#include "config.h"
#include "core/aurora_runtime.h"
#include "effects/aurora_field.h"

namespace {

uint32_t collectAuroraSeed() {
  pinMode(AvrConfig::AuroraEntropyPin, INPUT);
  digitalWrite(AvrConfig::AuroraEntropyPin, LOW);

  uint32_t seed = micros() ^ Config::AuroraZeroSeedFallback;
  for (uint8_t sampleIndex = 0; sampleIndex < 32; ++sampleIndex) {
    const uint32_t sample = analogRead(AvrConfig::AuroraEntropyPin);
    seed ^= sample << (sampleIndex & 0x0F);
    seed ^= micros() + 0x9E3779B9UL + sampleIndex;
    seed = Aurora::xorshift32(seed);
  }
  return seed == 0 ? Config::AuroraZeroSeedFallback : seed;
}

AvrInputs inputs;
AvrLedDriver ledDriver;
AvrSerialDebug debug;
AuroraRuntime runtime(Config::runtimeConfig());

}  // namespace

void setup() {
  inputs.begin();
  ledDriver.begin();
  debug.begin();
  runtime.reset(collectAuroraSeed(), millis());
}

void loop() {
  const uint32_t nowMs = millis();
  inputs.update(nowMs);
  const AuroraInputFrame &inputFrame = inputs.frame();
  runtime.step(inputFrame, nowMs);

  const AuroraSnapshot &snapshot = runtime.snapshot();
  ledDriver.output(runtime.ledFrame(), runtime.ledCount(),
                   inputFrame.stripPowerPresent, snapshot.frameChanged);
  debug.update(inputFrame, snapshot, nowMs);
}
