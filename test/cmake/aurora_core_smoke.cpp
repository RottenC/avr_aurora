#include <aurora/aurora_runtime.h>

#include "config.h"

int main() {
  AuroraRuntime runtime(Config::runtimeConfig());
  if (!runtime.configValid() || runtime.ledCount() != Aurora::LedCount) {
    return 1;
  }

  runtime.reset(0x12345678UL, 0);
  AuroraInputFrame inputs;
  inputs.powerLed = SignalState::High;
  inputs.stripPowerPresent = true;
  runtime.step(inputs, 0);

  uint32_t frameHash = 0x811C9DC5UL;
  for (uint8_t index = 0; index < runtime.ledCount(); ++index) {
    const Aurora::Rgb8 &pixel = runtime.ledFrame()[index];
    frameHash = (frameHash ^ pixel.r) * 0x01000193UL;
    frameHash = (frameHash ^ pixel.g) * 0x01000193UL;
    frameHash = (frameHash ^ pixel.b) * 0x01000193UL;
  }
  return frameHash == 0 ? 2 : 0;
}
