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

LampProbe LightingController::probeLamp(uint8_t zone) {
  constexpr uint32_t SETTLE_US = 150;
  LampProbe probe;
  if (zone >= Pins::LAMP_COUNT) return probe;
  uint8_t pin = Pins::LAMPS[zone];
  ledcDetachPin(pin);
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(SETTLE_US);
  probe.highWithPullUp = digitalRead(pin);
  pinMode(pin, INPUT_PULLDOWN);
  delayMicroseconds(SETTLE_US);
  probe.highWithPullDown = digitalRead(pin);
  pinMode(pin, OUTPUT);
  ledcAttachPin(pin, zone);
  writeZone(zone, zoneCurrent[zone]);
  return probe;
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
    case LightingMode::ECONOMY: target = economyPercent(inputs.ambientAdc); break;
    case LightingMode::ACTIVE: target = Config::LIGHT_ACTIVE_PERCENT; break;
    default: target = Config::LIGHT_SAFE_PERCENT; break;
  }
  autoTarget_ = target;
  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) {
    switch (control_[zone]) {
      case ZoneControl::OFF: setZoneTarget(zone, 0); break;
      case ZoneControl::ON: setZoneTarget(zone, manual_[zone]); break;
      default: setZoneTarget(zone, target); break;
    }
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

uint8_t LightingController::economyPercent(int adc) {
  // Entre NIGHT_THRESHOLD e DAY_THRESHOLD (histerese, luz já acesa) fica no mínimo do anoitecer
  if (adc >= Config::NIGHT_THRESHOLD) return Config::LIGHT_DUSK_PERCENT;
  if (adc <= Config::DARK_THRESHOLD) return Config::LIGHT_NIGHT_PERCENT;
  // Quanto mais escuro, mais forte: interpolação linear entre os dois níveis
  float darkness = static_cast<float>(Config::NIGHT_THRESHOLD - adc) /
                   (Config::NIGHT_THRESHOLD - Config::DARK_THRESHOLD);
  return static_cast<uint8_t>(Config::LIGHT_DUSK_PERCENT +
                              darkness * (Config::LIGHT_NIGHT_PERCENT - Config::LIGHT_DUSK_PERCENT) + 0.5f);
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

void LightingController::setZoneControl(uint8_t zone, ZoneControl control, uint8_t percent) {
  if (zone >= Pins::LAMP_COUNT) return;
  control_[zone] = control;
  if (control == ZoneControl::ON) manual_[zone] = constrain(percent, 1, 100);
}

bool LightingController::anyManual() const {
  for (ZoneControl c : control_) {
    if (c != ZoneControl::AUTO) return true;
  }
  return false;
}

const char *LightingController::controlLabel(ZoneControl c) {
  switch (c) {
    case ZoneControl::OFF: return "OFF";
    case ZoneControl::ON: return "ON";
    default: return "AUTO";
  }
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
