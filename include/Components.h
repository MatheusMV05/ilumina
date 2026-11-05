#pragma once

#include <Arduino.h>

enum class Component : uint8_t {
  OLED,
  DHT11,
  LDR_AMBIENT,
  LDR_LOCAL,
  ULTRASONIC,
  IR,
  MIC,
  MQ135,
  LEDS,
  STATUS_LED,
  WIFI,
  MQTT,
  COUNT,
};

constexpr uint8_t COMPONENT_COUNT = static_cast<uint8_t>(Component::COUNT);

inline uint8_t componentIndex(Component c) { return static_cast<uint8_t>(c); }

// Nome curto para OLED e Serial
const char *componentName(Component c);
// Identificador estável usado no JSON e na API do dashboard
const char *componentCode(Component c);
// Retorna false se o código não existir
bool componentFromCode(const char *code, Component &out);
