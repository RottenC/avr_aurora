#pragma once
#include <Arduino.h>

struct NormalizedInputs {
  bool powerLed;
  bool hddLed;
  bool powerButton;
  bool resetButton;
  bool stripPowerPresent;
  bool debugButton;
};

class Inputs {
public:
  void begin();
  void update(uint32_t nowMs);
  const NormalizedInputs &state() const { return stable_; }
  bool powerButtonPressed() const { return powerButtonPressed_; }
  bool powerButtonReleased() const { return powerButtonReleased_; }
  bool resetButtonPressed() const { return resetButtonPressed_; }
  bool debugButtonPressed() const { return debugButtonPressed_; }
  uint8_t consumeHddEdges();
private:
  struct Debounced { bool stable=false; bool candidate=false; uint32_t changedAt=0; };
  void updateOne(Debounced &d, bool raw, uint32_t nowMs, bool &pressed, bool &released);
  uint32_t lastPollMs_=0;
  Debounced powerLed_, powerButton_, resetButton_, stripPower_, debugButton_;
  NormalizedInputs stable_{};
  bool powerButtonPressed_=false, powerButtonReleased_=false, resetButtonPressed_=false, debugButtonPressed_=false;
};
