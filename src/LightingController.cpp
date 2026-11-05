#include "LightingController.h"

#include "AppConfig.h"

namespace {
constexpr uint32_t PWM_MAX = (1u << Config::LIGHT_PWM_RESOLUTION_BITS) - 1;
}

void LightingController::begin() {
  // Canal LEDC = índice da zona (0 a 3)
  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) {
    ledcSetup(zone, Config::LIGHT_PWM_FREQ_HZ, Config::LIGHT_PWM_RESOLUTION_BITS);
    ledcAttachPin(Pins::LAMPS[zone], zone);
    writeZone(zone, 0);
  }
}

void LightingController::update(const LightingInputs &inputs, uint32_t now) {
  if (inputs.presenceDetected) {
    hadPresence = true;
    lastPresenceMs = now;
  }
  bool presenceHeld = hadPresence && (now - lastPresenceMs) < Config::PRESENCE_HOLD_MS;

  if (!inputs.ambientValid) {
    // Sem LDR ambiente não dá para saber se é dia: nunca assumir dia
    mode_ = LightingMode::SAFE;
  } else {
    updateDayNight(inputs.ambientAdc);
    if (!night) {
      mode_ = LightingMode::DAY_OFF;
    } else if (!inputs.presenceAvailable) {
      mode_ = LightingMode::SAFE;
    } else if (presenceHeld) {
      mode_ = LightingMode::ACTIVE;
    } else {
      mode_ = LightingMode::ECONOMY;
    }
  }

  uint8_t target;
  switch (mode_) {
    case LightingMode::DAY_OFF: target = Config::LIGHT_OFF_PERCENT; break;
    case LightingMode::ECONOMY: target = Config::LIGHT_STANDBY_PERCENT; break;
    case LightingMode::ACTIVE: target = Config::LIGHT_ACTIVE_PERCENT; break;
    default: target = Config::LIGHT_SAFE_PERCENT; break;
  }
  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) {
    setZoneTarget(zone, target);
  }

  applyFade(now);
}

void LightingController::updateDayNight(int adc) {
  if (!dayNightInitialized) {
    // Primeira leitura: decide pelo ponto médio entre os limites
    night = adc < (Config::DAY_THRESHOLD + Config::NIGHT_THRESHOLD) / 2;
    dayNightInitialized = true;
    return;
  }
  // Histerese: entre os dois limites o estado anterior é mantido
  if (adc > Config::DAY_THRESHOLD) {
    night = false;
  } else if (adc < Config::NIGHT_THRESHOLD) {
    night = true;
  }
}

void LightingController::setZoneTarget(uint8_t zone, uint8_t percent) {
  if (zone < Pins::LAMP_COUNT) zoneTarget[zone] = min<uint8_t>(percent, 100);
}

void LightingController::applyFade(uint32_t now) {
  uint32_t elapsed = lastFadeMs == 0 ? 0 : now - lastFadeMs;
  lastFadeMs = now;

  float maxStep = Config::LIGHT_FADE_MS_PER_PERCENT == 0
                      ? 100.0f
                      : static_cast<float>(elapsed) / Config::LIGHT_FADE_MS_PER_PERCENT;

  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) {
    float diff = zoneTarget[zone] - zoneCurrent[zone];
    if (diff == 0) continue;
    if (fabsf(diff) <= maxStep) {
      zoneCurrent[zone] = zoneTarget[zone];
    } else {
      zoneCurrent[zone] += diff > 0 ? maxStep : -maxStep;
    }
    writeZone(zone, zoneCurrent[zone]);
  }
}

uint32_t LightingController::holdRemainingMs(uint32_t now) const {
  if (!hadPresence) return 0;
  uint32_t elapsed = now - lastPresenceMs;
  return elapsed < Config::PRESENCE_HOLD_MS ? Config::PRESENCE_HOLD_MS - elapsed : 0;
}

void LightingController::setZoneImmediate(uint8_t zone, uint8_t percent) {
  if (zone >= Pins::LAMP_COUNT) return;
  zoneTarget[zone] = min<uint8_t>(percent, 100);
  zoneCurrent[zone] = zoneTarget[zone];
  writeZone(zone, zoneCurrent[zone]);
}

void LightingController::setAllImmediate(uint8_t percent) {
  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) {
    setZoneImmediate(zone, percent);
  }
}

void LightingController::writeZone(uint8_t zone, float percent) {
  ledcWrite(zone, static_cast<uint32_t>(percent * PWM_MAX / 100.0f + 0.5f));
}

const char *LightingController::modeOledLabel(LightingMode m) {
  switch (m) {
    case LightingMode::DAY_OFF: return "DESLIGADO";
    case LightingMode::ECONOMY: return "ECONOMIA";
    case LightingMode::ACTIVE: return "PRESENCA";
    default: return "SEGURO";
  }
}

const char *LightingController::modeTelemetryLabel(LightingMode m) {
  switch (m) {
    case LightingMode::DAY_OFF: return "DAY_OFF";
    case LightingMode::ECONOMY: return "ECONOMY";
    case LightingMode::ACTIVE: return "ACTIVE";
    default: return "SAFE";
  }
}
