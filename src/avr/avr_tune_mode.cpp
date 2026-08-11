#include "avr_tune_mode.h"

#ifdef TuneMode

#include "avr_config.h"

namespace {

uint8_t scaleChannel(uint8_t channel, uint8_t brightness) {
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(channel) * brightness + 127U) / 255U);
}

char hexDigit(uint8_t value) {
  value &= 0x0F;
  return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
}

void printHexByte(uint8_t value) {
  Serial.print(hexDigit(value >> 4));
  Serial.print(hexDigit(value));
}

}  // namespace

void AvrTuneMode::begin() {
  inputs_.begin();
  ledDriver_.begin();
  Serial.begin(AvrConfig::SerialBaud);
  rebuildFrame();
  Serial.println(F("TuneMode ready"));
  Serial.println(F("set_color #RRGGBB 0x00..0xFF"));
}

void AvrTuneMode::update() {
  const uint32_t nowMs = millis();
  inputs_.update(nowMs);
  const bool powerRestored = ledDriver_.updatePowerState(
      inputs_.rawStripPowerPresent(), inputs_.frame().stripPowerPresent,
      nowMs);
  const bool frameChanged = readCommands();
  ledDriver_.output(frame_, Aurora::LedCount,
                    frameChanged || powerRestored);
}

bool AvrTuneMode::readCommands() {
  bool frameChanged = false;
  while (Serial.available() > 0) {
    const int input = Serial.read();
    if (input < 0) break;
    const char value = static_cast<char>(input);
    if (value == '\r') continue;
    if (value == '\n') {
      frameChanged |= finishLine();
      continue;
    }
    if (discardingLine_) continue;
    if (commandLength_ + 1U >= CommandBufferSize) {
      discardingLine_ = true;
      continue;
    }
    commandBuffer_[commandLength_++] = value;
  }
  return frameChanged;
}

bool AvrTuneMode::finishLine() {
  if (discardingLine_) {
    Serial.println(F("error: command_too_long"));
    commandLength_ = 0;
    discardingLine_ = false;
    return false;
  }

  commandBuffer_[commandLength_] = '\0';
  commandLength_ = 0;
  const Tune::ParseResult result = Tune::parseCommand(commandBuffer_);
  if (result.error == Tune::ParseError::Empty) return false;
  if (!result.ok()) {
    printError(result.error);
    return false;
  }

  apply(result.command);
  return true;
}

void AvrTuneMode::apply(const Tune::Command &command) {
  switch (command.kind) {
    case Tune::CommandKind::SetColor:
      color_ = command.color;
      brightness_ = command.brightness;
      rebuildFrame();
      Serial.print(F("ok set_color #"));
      printHexByte(color_.r);
      printHexByte(color_.g);
      printHexByte(color_.b);
      Serial.print(F(" 0x"));
      printHexByte(brightness_);
      Serial.println();
      break;
  }
}

void AvrTuneMode::rebuildFrame() {
  const Aurora::Rgb8 output{
      scaleChannel(color_.r, brightness_),
      scaleChannel(color_.g, brightness_),
      scaleChannel(color_.b, brightness_),
  };
  for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
    frame_[index] = output;
  }
}

void AvrTuneMode::printError(Tune::ParseError error) const {
  Serial.print(F("error: "));
  switch (error) {
    case Tune::ParseError::None:
      Serial.println(F("none"));
      return;
    case Tune::ParseError::Empty:
      Serial.println(F("empty"));
      return;
    case Tune::ParseError::UnknownCommand:
      Serial.println(F("unknown_command"));
      return;
    case Tune::ParseError::MissingArgument:
      Serial.println(F("missing_argument"));
      return;
    case Tune::ParseError::InvalidColor:
      Serial.println(F("invalid_color"));
      return;
    case Tune::ParseError::InvalidBrightness:
      Serial.println(F("invalid_brightness"));
      return;
    case Tune::ParseError::TrailingInput:
      Serial.println(F("trailing_input"));
      return;
  }
}

#endif
