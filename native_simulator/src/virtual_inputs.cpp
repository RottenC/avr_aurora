#include "virtual_inputs.h"

#include <algorithm>
#include <climits>

#include "config.h"

void VirtualInputs::resetTransientState() {
  eventHead_ = 0;
  eventCount_ = 0;
  pendingHddEdges_ = 0;
  requestedPowerButton_ = false;
  requestedResetButton_ = false;
  deliveredPowerButton_ = false;
  deliveredResetButton_ = false;
}

void VirtualInputs::enqueue(ButtonEvent event) {
  if (eventCount_ == events_.size()) return;
  events_[(eventHead_ + eventCount_) % events_.size()] = event;
  ++eventCount_;
}

void VirtualInputs::pressPowerButton() {
  if (requestedPowerButton_) return;
  requestedPowerButton_ = true;
  enqueue(ButtonEvent::PowerPress);
}
void VirtualInputs::releasePowerButton() {
  if (!requestedPowerButton_) return;
  requestedPowerButton_ = false;
  enqueue(ButtonEvent::PowerRelease);
}
void VirtualInputs::pressResetButton() {
  if (requestedResetButton_) return;
  requestedResetButton_ = true;
  enqueue(ButtonEvent::ResetPress);
}
void VirtualInputs::releaseResetButton() {
  if (!requestedResetButton_) return;
  requestedResetButton_ = false;
  enqueue(ButtonEvent::ResetRelease);
}

void VirtualInputs::addHddActiveEdges(uint8_t count) {
  pendingHddEdges_ = static_cast<uint16_t>(std::min<unsigned>(
      UINT8_MAX, static_cast<unsigned>(pendingHddEdges_) + count));
}

uint8_t VirtualInputs::pendingHddActiveEdges() const {
  return static_cast<uint8_t>(pendingHddEdges_);
}

AuroraInputFrame VirtualInputs::buildFrame(bool powerLed, bool hddLed,
                                           uint8_t generatedHddEdges) {
  AuroraInputFrame frame{};
  frame.powerLed = powerLed ? SignalState::High : SignalState::Low;
  frame.hddLed = hddLed ? SignalState::High : SignalState::Low;
  const uint8_t edges = static_cast<uint8_t>(std::min<unsigned>(
      UINT8_MAX, pendingHddEdges_ + generatedHddEdges));
  pendingHddEdges_ = 0;
  // The portable core expresses sampled HDD activity as a Q8.8 contribution.
  frame.hddContribution = static_cast<int16_t>(std::min<unsigned>(
      INT16_MAX,
      static_cast<unsigned>(edges) * (Config::HddEdgeBoost << 8U)));

  if (eventCount_ != 0) {
    const ButtonEvent event = events_[eventHead_];
    eventHead_ = static_cast<uint8_t>((eventHead_ + 1) % events_.size());
    --eventCount_;
    switch (event) {
      case ButtonEvent::PowerPress:
        deliveredPowerButton_ = true;
        frame.powerButton = SignalState::Rising;
        break;
      case ButtonEvent::PowerRelease:
        deliveredPowerButton_ = false;
        frame.powerButton = SignalState::Falling;
        break;
      case ButtonEvent::ResetPress:
        deliveredResetButton_ = true;
        frame.resetButton = SignalState::Rising;
        break;
      case ButtonEvent::ResetRelease:
        deliveredResetButton_ = false;
        frame.resetButton = SignalState::Falling;
        break;
    }
  }
  if (frame.powerButton == SignalState::Low && deliveredPowerButton_) {
    frame.powerButton = SignalState::High;
  }
  if (frame.resetButton == SignalState::Low && deliveredResetButton_) {
    frame.resetButton = SignalState::High;
  }
  frame.stripPowerPresent = stripPowerPresent_;
  return frame;
}
