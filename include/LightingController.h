#pragma once

#include <Arduino.h>

#include "PinMap.h"

enum class LightingMode : uint8_t {
  DAY_OFF,  // ambiente claro: luminárias apagadas
  ECONOMY,  // escuro sem movimento: brilho proporcional ao escuro (anoitecer < noite < máximo)
  ACTIVE,   // noite com presença (ou dentro do temporizador)
  SAFE,     // LDR ambiente ou sensores de presença indisponíveis
};

// Leitura do pino de um LED como entrada, com cada pull interno ligado.
// LED + 220 Ω até o GND puxam a linha para baixo contra o pull-up; fio solto não.
struct LampProbe {
  bool highWithPullUp = false;
  bool highWithPullDown = false;
};

// Comando de cada luminária vindo do painel. AUTO segue a lógica (dia/noite/presença);
// OFF e ON fixam a saída até alguém voltar para AUTO. Não é gravado: após um reinício
// tudo volta para AUTO, para um comando esquecido não deixar a rua apagada.
enum class ZoneControl : uint8_t { AUTO, OFF, ON };

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

  // Solta o PWM do LED por menos de 1 ms para ler o pino e devolve o brilho em seguida
  LampProbe probeLamp(uint8_t zone);

  // Preparado para zonas independentes; na v1 todas recebem o mesmo alvo
  void setZoneTarget(uint8_t zone, uint8_t percent);

  void setZoneControl(uint8_t zone, ZoneControl control, uint8_t manualPercent);
  ZoneControl zoneControl(uint8_t zone) const { return control_[zone]; }
  uint8_t manualPercent(uint8_t zone) const { return manual_[zone]; }
  bool anyManual() const;
  // Brilho que a lógica automática daria agora (o que AUTO faria)
  uint8_t autoPercent() const { return autoTarget_; }
  static const char *controlLabel(ZoneControl c);

  bool isNight() const { return night; }
  bool presenceActive() const { return mode_ == LightingMode::ACTIVE; }
  LightingMode mode() const { return mode_; }
  uint8_t targetPercent() const { return autoTarget_; }
  uint32_t holdRemainingMs(uint32_t now) const;
  uint8_t zonePercent(uint8_t zone) const { return static_cast<uint8_t>(zoneCurrent[zone] + 0.5f); }

  static const char *modeOledLabel(LightingMode m);
  static const char *modeTelemetryLabel(LightingMode m);

 private:
  void updateDayNight(int adc);
  static uint8_t economyPercent(int adc);
  void applyFade(uint32_t now);
  void writeZone(uint8_t zone, float percent);

  bool night = false;
  bool dayNightInitialized = false;
  bool hadPresence = false;
  uint32_t lastPresenceMs = 0;
  uint32_t lastFadeMs = 0;
  LightingMode mode_ = LightingMode::DAY_OFF;

  uint8_t zoneTarget[Pins::LAMP_COUNT] = {};
  ZoneControl control_[Pins::LAMP_COUNT] = {};
  uint8_t manual_[Pins::LAMP_COUNT] = {100, 100, 100, 100};
  uint8_t autoTarget_ = 0;
  float zoneCurrent[Pins::LAMP_COUNT] = {};
};
