#pragma once

#include <stdint.h>

#include "../core/rgb8.h"

namespace Tune {

enum class CommandKind : uint8_t {
  SetColor,
};

enum class ParseError : uint8_t {
  None,
  Empty,
  UnknownCommand,
  MissingArgument,
  InvalidColor,
  InvalidBrightness,
  TrailingInput,
};

struct Command {
  CommandKind kind = CommandKind::SetColor;
  Aurora::Rgb8 color{};
  uint8_t brightness = 0;
};

struct ParseResult {
  Command command{};
  ParseError error = ParseError::None;

  bool ok() const { return error == ParseError::None; }
};

ParseResult parseCommand(const char *line);

}  // namespace Tune
