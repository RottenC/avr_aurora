#pragma once

#include <array>
#include <cstdint>

#include <aurora/aurora_runtime.h>

class VirtualInputs {
 public:
  void resetTransientState();
  void setManualPowerLed(bool active) { manualPowerLed_ = active; }
  void setManualHddLed(bool active) { manualHddLed_ = active; }
  void setStripPowerPresent(bool present) { stripPowerPresent_ = present; }
  void pressPowerButton();
  void releasePowerButton();
  void pressResetButton();
  void releaseResetButton();
  void addHddActiveEdges(uint8_t count = 1);

  AuroraInputFrame buildFrame(bool powerLed, bool hddLed,
                              uint8_t generatedHddEdges);

  bool manualPowerLed() const { return manualPowerLed_; }
  bool manualHddLed() const { return manualHddLed_; }
  bool powerButtonHeld() const { return requestedPowerButton_; }
  bool resetButtonHeld() const { return requestedResetButton_; }
  bool stripPowerPresent() const { return stripPowerPresent_; }
  uint8_t pendingHddActiveEdges() const;

 private:
  enum class ButtonEvent : uint8_t { PowerPress, PowerRelease, ResetPress, ResetRelease };
  void enqueue(ButtonEvent event);

  std::array<ButtonEvent, 8> events_{};
  uint8_t eventHead_ = 0;
  uint8_t eventCount_ = 0;
  uint16_t pendingHddEdges_ = 0;
  bool manualPowerLed_ = false;
  bool manualHddLed_ = false;
  bool requestedPowerButton_ = false;
  bool requestedResetButton_ = false;
  bool deliveredPowerButton_ = false;
  bool deliveredResetButton_ = false;
  bool stripPowerPresent_ = true;
};
