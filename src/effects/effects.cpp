#include "effects.h"
#include "../config.h"

void renderBlack(CRGB *leds, uint8_t count){ fill_solid(leds, count, CRGB::Black); }

void renderAurora(CRGB *leds, uint8_t count, uint32_t nowMs, uint8_t hddActivity){
  const uint8_t normalizedActivity = qadd8(hddActivity, hddActivity);
  const uint8_t speed = Config::AuroraBaseSpeed + (Config::AuroraHddAffectsSpeed ? hddActivity / 4 : 0);
  const uint8_t bright = qadd8(Config::AuroraBaseBrightness,
      Config::AuroraHddAffectsBrightness ? scale8(normalizedActivity, Config::AuroraHddBrightnessBoost) : 0);
  for (uint8_t i=0;i<count;++i){
    const uint8_t wave = sin8(i * 9 + (nowMs / 16) * speed / 16);
    leds[i] = CHSV(96 + wave / 5, 190, scale8(wave, bright));
  }
}

void renderSleep(CRGB *leds, uint8_t count, uint32_t nowMs){
  renderBlack(leds, count);
  const uint8_t p1 = (nowMs / 1700) % count;
  const uint8_t p2 = (p1 + 29) % count;
  const uint8_t b = beatsin8(4, 4, Config::SleepPointBrightness);
  leds[p1] = CHSV(160, 180, b);
  leds[p2] = CHSV(176, 160, b / 2);
}

void renderTransition(CRGB *leds, uint8_t count, TransitionEffect effect, uint32_t startedAt, uint32_t nowMs){
  renderBlack(leds, count);
  const uint32_t age = nowMs - startedAt;
  if (effect == TransitionEffect::Startup) {
    const uint8_t lit = static_cast<uint8_t>((static_cast<uint32_t>(count) * age) / Config::StartupDurationMs);
    for (uint8_t i=0;i<count && i<=lit;++i) leds[i] = CHSV(120 + i, 220, Config::TransitionBrightness);
  } else if (effect == TransitionEffect::Shutdown || effect == TransitionEffect::Reset) {
    const uint16_t duration = effect == TransitionEffect::Reset ? Config::ResetDurationMs : Config::ShutdownDurationMs;
    const uint8_t origin = Config::ShutdownOriginMin + ((startedAt >> 2) % (Config::ShutdownOriginMax - Config::ShutdownOriginMin + 1));
    const uint8_t radius = static_cast<uint8_t>((static_cast<uint32_t>(count) * age) / duration);
    const CRGB color = effect == TransitionEffect::Reset ? CRGB::Red : CRGB::White;
    for (uint8_t i=0;i<count;++i) if (abs(static_cast<int>(i)-static_cast<int>(origin)) == radius) leds[i] = color;
  } else if (effect == TransitionEffect::ForcedShutdown) {
    const uint32_t boundedAge = min(age, static_cast<uint32_t>(Config::PowerHoldForcedMs));
    const uint8_t b = boundedAge < Config::ForcedFlashAtMs
      ? static_cast<uint8_t>(map(boundedAge, 0, Config::ForcedFlashAtMs, 10, 160))
      : static_cast<uint8_t>(map(boundedAge, Config::ForcedFlashAtMs, Config::PowerHoldForcedMs, 160, 0));
    fill_solid(leds, count, CHSV(0, 255, b));
  }
}