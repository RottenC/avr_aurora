#pragma once
#include <stdint.h>

struct HddActivityConfig {
  uint16_t updateMs;
  uint8_t edgeBoost;
  uint8_t activeRise;
  uint8_t inactiveDecay;
  uint8_t maximum;
};

class HddActivity {
public:
  explicit HddActivity(const HddActivityConfig &config) : config_(config) {}
  void reset() { value_ = 0; }
  void update(bool active, uint8_t edgeCount, uint32_t elapsedMs);
  uint8_t value() const { return value_; }
  uint16_t updateIntervalMs() const { return config_.updateMs; }
private:
  HddActivityConfig config_;
  uint8_t value_=0;
};
