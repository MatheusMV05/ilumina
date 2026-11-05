#pragma once

#include <Arduino.h>

#include "PinMap.h"

enum class LightingMode : uint8_t {
  DAY_OFF,  // ambiente claro: luminárias apagadas
  ECONOMY,  // noite sem presença: intensidade reduzida
  ACTIVE,   // noite com presença (ou dentro do temporizador)
  SAFE,     // LDR ambiente ou sensores de presença indisponíveis
};

// Entradas da lógica de iluminação, já interpretadas pelo main
struct LightingInputs {
  bool ambientValid = false;
  int ambientAdc = 0;
  bool presenceDetected = false;
  bool presenceAvailable = false;  // ao menos um sensor de presença confirmado
};

// Controla os quatro LEDs amarelos por PWM, aplicando histerese,
// temporizador de presença e regras de modo seguro.
class LightingController {
 public:
  void begin();
  void update(const LightingInputs &inputs, uint32_t now);

  // Acesso direto para o teste visual do boot (ignora a lógica)
  void setZoneImmediate(uint8_t zone, uint8_t percent);
  void setAllImmediate(uint8_t percent);

  // Preparado para zonas independentes; na v1 todas recebem o mesmo alvo
  void setZoneTarget(uint8_t zone, uint8_t percent);

  bool isNight() const { return night; }
  bool presenceActive() const { return mode_ == LightingMode::ACTIVE; }
  LightingMode mode() const { return mode_; }
  uint8_t targetPercent() const { return zoneTarget[0]; }
  uint32_t holdRemainingMs(uint32_t now) const;
  uint8_t zonePercent(uint8_t zone) const { return static_cast<uint8_t>(zoneCurrent[zone] + 0.5f); }

  static const char *modeOledLabel(LightingMode m);
  static const char *modeTelemetryLabel(LightingMode m);

 private:
  void updateDayNight(int adc);
  void applyFade(uint32_t now);
  void writeZone(uint8_t zone, float percent);

  bool night = false;
  bool dayNightInitialized = false;
  bool hadPresence = false;
  uint32_t lastPresenceMs = 0;
  uint32_t lastFadeMs = 0;
  LightingMode mode_ = LightingMode::DAY_OFF;

  uint8_t zoneTarget[Pins::LAMP_COUNT] = {};
  float zoneCurrent[Pins::LAMP_COUNT] = {};
};
