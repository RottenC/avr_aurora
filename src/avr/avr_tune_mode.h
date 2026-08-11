#pragma once

#ifdef TuneMode

#include <Arduino.h>

#include "../core/geometry.h"
#include "../core/rgb8.h"
#include "../tune/tune_command.h"
#include "avr_inputs.h"
#include "avr_led_driver.h"

class AvrTuneMode {
 public:
  void begin();
  void update();

 private:
  static constexpr uint8_t CommandBufferSize = 48;

  bool readCommands();
  bool finishLine();
  void apply(const Tune::Command &command);
  void rebuildFrame();
  void printError(Tune::ParseError error) const;

  AvrInputs inputs_;
  AvrLedDriver ledDriver_;
  Aurora::Rgb8 frame_[Aurora::LedCount]{};
  Aurora::Rgb8 color_{};
  char commandBuffer_[CommandBufferSize]{};
  uint8_t commandLength_ = 0;
  uint8_t brightness_ = 0;
  bool discardingLine_ = false;
};

#endif
