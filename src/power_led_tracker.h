#pragma once
#include <stdint.h>
#include "state_types.h"

struct PowerLedTrackerConfig {
  uint32_t shortOffIgnoreMs;
  uint32_t blinkMinHalfPeriodMs;
  uint32_t blinkMaxHalfPeriodMs;
  uint32_t blinkStaleMs;
  uint8_t blinkEdgesRequired;
};

class PowerLedTracker {
public:
  explicit PowerLedTracker(const PowerLedTrackerConfig &config) : config_(config) {}
  void update(bool active, uint32_t nowMs);
  PowerLedMode mode(uint32_t nowMs) const;
private:
  PowerLedTrackerConfig config_;
  bool last_=false;
  bool initialized_=false;
  bool seenOn_=false;
  uint32_t lastChangeMs_=0;
  uint32_t lastOnMs_=0;
  uint32_t lastValidBlinkEdgeMs_=0;
  uint8_t blinkEdges_=0;
};
