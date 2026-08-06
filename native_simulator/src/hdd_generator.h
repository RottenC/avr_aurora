#pragma once

#include <cstdint>

enum class HddSourceMode : uint8_t { Manual, RandomQuake };

struct HddGeneratorParams {
  int activityRate = 55;
  int pulseDurationMs = 80;
  int burstSize = 6;
  int randomness = 85;
};

struct HddUpdate {
  bool level = false;
  uint8_t activeEdges = 0;
};

class HddGenerator {
 public:
  void reset();
  HddUpdate update(uint32_t deltaMs, bool manualLevel);
  void setMode(HddSourceMode mode);
  void setSeed(uint32_t seed);
  void setParams(const HddGeneratorParams &params) { params_ = params; }

  HddSourceMode mode() const { return mode_; }
  uint32_t seed() const { return seed_; }
  const HddGeneratorParams &params() const { return params_; }
  bool level() const { return level_; }

 private:
  uint32_t nextRandom();
  int randomRange(int minimum, int maximum);
  bool nextLevel();
  uint32_t nextDuration(bool active);

  HddSourceMode mode_ = HddSourceMode::Manual;
  HddGeneratorParams params_{};
  uint32_t seed_ = 1;
  uint32_t randomState_ = 1;
  uint32_t remainingMs_ = 0;
  bool level_ = false;
  bool manualLevel_ = false;
};
