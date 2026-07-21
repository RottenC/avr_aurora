#pragma once
#include <Arduino.h>

enum class PowerLedMode : uint8_t { Off, On, Blinking };

class PowerLedTracker {
public:
  void update(bool active, uint32_t nowMs);
  PowerLedMode mode(uint32_t nowMs) const;
private:
  bool last_=false;
  uint32_t lastChangeMs_=0;
  uint32_t lastOnMs_=0;
  uint32_t lastValidBlinkEdgeMs_=0;
  uint8_t blinkEdges_=0;
};