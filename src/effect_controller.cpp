#include "effect_controller.h"
#include "config.h"
void EffectController::request(TransitionEffect effect, uint32_t nowMs){ current_=effect; startedAt_=nowMs; }
void EffectController::update(PcStateMachine &pc, bool stripPowerPresent, uint32_t nowMs){
  if (!stripPowerPresent && current_ != TransitionEffect::ForcedShutdown) return;
  const uint32_t age = nowMs - startedAt_;
  if (current_ == TransitionEffect::Startup && age >= Config::StartupDurationMs) { current_=TransitionEffect::None; pc.markStartupComplete(); }
  else if (current_ == TransitionEffect::Shutdown && age >= Config::ShutdownDurationMs) { current_=TransitionEffect::None; pc.markShutdownComplete(); }
  else if (current_ == TransitionEffect::Reset && age >= Config::ResetDurationMs) current_=TransitionEffect::None;
}
const __FlashStringHelper *transitionName(TransitionEffect effect){
  switch(effect){case TransitionEffect::None:return F("None");case TransitionEffect::Startup:return F("Startup");case TransitionEffect::Shutdown:return F("Shutdown");case TransitionEffect::ForcedShutdown:return F("ForcedShutdown");case TransitionEffect::Reset:return F("Reset");}
  return F("?");
}
