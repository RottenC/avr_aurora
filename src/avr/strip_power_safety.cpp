#include "strip_power_safety.h"

void StripPowerSafety::reset(bool rawPresent, bool logicalPresent,
                             uint32_t nowMs) {
  (void)logicalPresent;
  rawPresentSinceMs_ = nowMs;
  rawWasPresent_ = rawPresent;
  outputAllowed_ = false;
  restorePending_ = true;
}

StripPowerSafetyResult StripPowerSafety::update(bool rawPresent,
                                                bool logicalPresent,
                                                uint32_t nowMs) {
  StripPowerSafetyResult result;

  if (!rawPresent) {
    rawWasPresent_ = false;
    outputAllowed_ = false;
    restorePending_ = true;
    return result;
  }

  if (!rawWasPresent_) {
    rawWasPresent_ = true;
    rawPresentSinceMs_ = nowMs;
    outputAllowed_ = false;
    restorePending_ = true;
  }

  if (!logicalPresent) {
    outputAllowed_ = false;
    restorePending_ = true;
    return result;
  }

  if (restorePending_ &&
      nowMs - rawPresentSinceMs_ >= restoreConfirmMs_) {
    restorePending_ = false;
    outputAllowed_ = true;
    result.requestFrameUpdate = true;
  }

  result.outputAllowed = outputAllowed_;
  result.safeStateRequired = !outputAllowed_;
  return result;
}
