#include "serial_debug.h"
#include "config.h"
#include "power_led_tracker.h"
void SerialDebug::begin(){ Serial.begin(Config::SerialBaud); }
void SerialDebug::update(const NormalizedInputs &in, PcState state, TransitionEffect transition, PowerLedMode powerMode, uint8_t hdd, uint32_t nowMs){
  if (nowMs - lastMs_ < Config::DebugIntervalMs) return; lastMs_=nowMs;
  Serial.print(F("pwrLed=")); Serial.print(in.powerLed); Serial.print(F(" hddLed=")); Serial.print(in.hddLed);
  Serial.print(F(" pwrBtn=")); Serial.print(in.powerButton); Serial.print(F(" rstBtn=")); Serial.print(in.resetButton);
  Serial.print(F(" strip=")); Serial.print(in.stripPowerPresent); Serial.print(F(" pwrMode=")); Serial.print(static_cast<uint8_t>(powerMode));
  Serial.print(F(" state=")); Serial.print(pcStateName(state)); Serial.print(F(" transition=")); Serial.print(transitionName(transition));
  Serial.print(F(" hdd=")); Serial.println(hdd);
}
