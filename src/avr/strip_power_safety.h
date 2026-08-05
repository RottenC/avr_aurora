#pragma once

#include <stdint.h>

struct StripPowerSafetyResult {
  bool safeStateRequired = true;
  bool outputAllowed = false;
  bool requestFrameUpdate = false;
};

// AVR output policy kept free of Arduino APIs so it can be tested natively.
// Raw loss is authoritative immediately. Restoration must remain raw-present
// for restoreConfirmMs and also have debounced logical confirmation.
class StripPowerSafety {
 public:
  explicit StripPowerSafety(uint16_t restoreConfirmMs)
      : restoreConfirmMs_(restoreConfirmMs) {}

  void reset(bool rawPresent, bool logicalPresent, uint32_t nowMs);
  StripPowerSafetyResult update(bool rawPresent, bool logicalPresent,
                                uint32_t nowMs);

 private:
  uint16_t restoreConfirmMs_;
  uint32_t rawPresentSinceMs_ = 0;
  bool rawWasPresent_ = false;
  bool outputAllowed_ = false;
  bool restorePending_ = true;
};
