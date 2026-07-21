#pragma once

// FastLED intentionally uses several GNU/AVR extensions that trigger
// project-level -Wpedantic and one AVR-only constant-conversion warning.
// Suppress those diagnostics only while parsing the dependency headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverflow"
#include <FastLED.h>
#pragma GCC diagnostic pop
