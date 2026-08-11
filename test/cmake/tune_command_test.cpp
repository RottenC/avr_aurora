#include "tune/tune_command.h"

namespace {

bool parsesSetColor() {
  const Tune::ParseResult result =
      Tune::parseCommand("set_color #1aB2c3 0x80");
  return result.ok() && result.command.kind == Tune::CommandKind::SetColor &&
         result.command.color.r == 0x1A && result.command.color.g == 0xB2 &&
         result.command.color.b == 0xC3 && result.command.brightness == 0x80;
}

bool acceptsWhitespaceAndBrightnessBounds() {
  const Tune::ParseResult low =
      Tune::parseCommand("\tset_color\t#000000\t0X0  ");
  const Tune::ParseResult high =
      Tune::parseCommand("set_color #FFFFFF 0xff");
  return low.ok() && low.command.brightness == 0 && high.ok() &&
         high.command.brightness == 0xFF;
}

bool rejectsMalformedCommands() {
  return Tune::parseCommand("").error == Tune::ParseError::Empty &&
         Tune::parseCommand("unknown #FFFFFF 0x10").error ==
             Tune::ParseError::UnknownCommand &&
         Tune::parseCommand("set_color").error ==
             Tune::ParseError::UnknownCommand &&
         Tune::parseCommand("set_color #FFFF 0x10").error ==
             Tune::ParseError::InvalidColor &&
         Tune::parseCommand("set_color #FFFFFF 10").error ==
             Tune::ParseError::InvalidBrightness &&
         Tune::parseCommand("set_color #FFFFFF 0x100").error ==
             Tune::ParseError::InvalidBrightness &&
         Tune::parseCommand("set_color #FFFFFF 0x10 extra").error ==
             Tune::ParseError::TrailingInput;
}

}  // namespace

int main() {
  if (!parsesSetColor()) return 1;
  if (!acceptsWhitespaceAndBrightnessBounds()) return 2;
  if (!rejectsMalformedCommands()) return 3;
  return 0;
}
