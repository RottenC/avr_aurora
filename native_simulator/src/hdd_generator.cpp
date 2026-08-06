#include "hdd_generator.h"

#include <algorithm>
#include <climits>

void HddGenerator::reset() {
  randomState_ = seed_ == 0 ? 0x6d2b79f5U : seed_;
  remainingMs_ = 0;
  level_ = false;
  manualLevel_ = false;
}

void HddGenerator::setMode(HddSourceMode mode) {
  if (mode == mode_) return;
  mode_ = mode;
  reset();
}

void HddGenerator::setSeed(uint32_t seed) {
  seed_ = seed;
  reset();
}

uint32_t HddGenerator::nextRandom() {
  uint32_t value = randomState_;
  value ^= value << 13U;
  value ^= value >> 17U;
  value ^= value << 5U;
  randomState_ = value;
  return value;
}

int HddGenerator::randomRange(int minimum, int maximum) {
  const uint32_t width = static_cast<uint32_t>(maximum - minimum + 1);
  return minimum + static_cast<int>(nextRandom() % width);
}

bool HddGenerator::nextLevel() {
  const int randomness = std::max(0, std::min(100, params_.randomness));
  const int activity = std::max(5, std::min(95, params_.activityRate +
      randomRange(-randomness, randomness)));
  return randomRange(0, 99) < activity;
}

uint32_t HddGenerator::nextDuration(bool active) {
  const int pulse = std::max(5, std::min(5000, params_.pulseDurationMs));
  const int burst = std::max(1, std::min(100, params_.burstSize));
  const int base = active ? pulse : pulse * burst;
  const int jitter = base * std::max(0, std::min(100, params_.randomness)) / 100;
  return static_cast<uint32_t>(std::max(5, base + randomRange(-jitter, jitter)));
}

HddUpdate HddGenerator::update(uint32_t deltaMs, bool manualLevel) {
  HddUpdate result{};
  if (mode_ == HddSourceMode::Manual) {
    result.activeEdges = static_cast<uint8_t>(!manualLevel_ && manualLevel);
    manualLevel_ = manualLevel;
    level_ = manualLevel;
    result.level = level_;
    return result;
  }

  uint32_t elapsed = 0;
  while (elapsed < deltaMs) {
    if (remainingMs_ == 0) {
      const bool previous = level_;
      level_ = nextLevel();
      remainingMs_ = nextDuration(level_);
      if (!previous && level_ && result.activeEdges < UINT8_MAX) {
        ++result.activeEdges;
      }
    }
    const uint32_t step = std::min(deltaMs - elapsed, remainingMs_);
    elapsed += step;
    remainingMs_ -= step;
  }
  result.level = level_;
  return result;
}
