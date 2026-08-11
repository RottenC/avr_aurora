#include "tune_command.h"

namespace Tune {
namespace {

bool isSpace(char value) { return value == ' ' || value == '\t'; }

void skipSpaces(const char *&cursor) {
  while (isSpace(*cursor)) ++cursor;
}

uint8_t hexDigit(char value) {
  if (value >= '0' && value <= '9') {
    return static_cast<uint8_t>(value - '0');
  }
  if (value >= 'a' && value <= 'f') {
    return static_cast<uint8_t>(value - 'a' + 10);
  }
  if (value >= 'A' && value <= 'F') {
    return static_cast<uint8_t>(value - 'A' + 10);
  }
  return 0xFF;
}

bool consumeKeyword(const char *&cursor, const char *keyword) {
  const char *candidate = cursor;
  while (*keyword != '\0' && *candidate == *keyword) {
    ++candidate;
    ++keyword;
  }
  if (*keyword != '\0' || !isSpace(*candidate)) return false;
  cursor = candidate;
  return true;
}

bool parseColor(const char *&cursor, Aurora::Rgb8 &color) {
  if (*cursor != '#') return false;
  ++cursor;

  uint8_t digits[6];
  for (uint8_t index = 0; index < 6; ++index) {
    digits[index] = hexDigit(*cursor);
    if (digits[index] == 0xFF) return false;
    ++cursor;
  }
  if (!isSpace(*cursor)) return false;

  color.r = static_cast<uint8_t>((digits[0] << 4) | digits[1]);
  color.g = static_cast<uint8_t>((digits[2] << 4) | digits[3]);
  color.b = static_cast<uint8_t>((digits[4] << 4) | digits[5]);
  return true;
}

bool parseBrightness(const char *&cursor, uint8_t &brightness) {
  if (cursor[0] != '0' || (cursor[1] != 'x' && cursor[1] != 'X')) {
    return false;
  }
  cursor += 2;

  const uint8_t firstDigit = hexDigit(*cursor);
  if (firstDigit == 0xFF) return false;

  uint16_t value = 0;
  do {
    value = static_cast<uint16_t>(value * 16U + hexDigit(*cursor));
    if (value > 0xFF) return false;
    ++cursor;
  } while (hexDigit(*cursor) != 0xFF);

  brightness = static_cast<uint8_t>(value);
  return true;
}

}  // namespace

ParseResult parseCommand(const char *line) {
  ParseResult result;
  if (line == nullptr) {
    result.error = ParseError::Empty;
    return result;
  }

  const char *cursor = line;
  skipSpaces(cursor);
  if (*cursor == '\0') {
    result.error = ParseError::Empty;
    return result;
  }
  if (!consumeKeyword(cursor, "set_color")) {
    result.error = ParseError::UnknownCommand;
    return result;
  }

  skipSpaces(cursor);
  if (*cursor == '\0') {
    result.error = ParseError::MissingArgument;
    return result;
  }
  if (!parseColor(cursor, result.command.color)) {
    result.error = ParseError::InvalidColor;
    return result;
  }

  skipSpaces(cursor);
  if (*cursor == '\0') {
    result.error = ParseError::MissingArgument;
    return result;
  }
  if (!parseBrightness(cursor, result.command.brightness)) {
    result.error = ParseError::InvalidBrightness;
    return result;
  }

  skipSpaces(cursor);
  if (*cursor != '\0') {
    result.error = ParseError::TrailingInput;
    return result;
  }
  return result;
}

}  // namespace Tune
