#include "Components.h"

const char *componentName(Component c) {
  switch (c) {
    case Component::OLED: return "OLED";
    case Component::DHT11: return "DHT11";
    case Component::LDR_AMBIENT: return "LDR AMB.";
    case Component::LDR_LOCAL: return "LDR LOCAL";
    case Component::ULTRASONIC: return "HC-SR04";
    case Component::IR: return "IR";
    case Component::MIC: return "KY-038";
    case Component::MQ135: return "MQ-135";
    case Component::LEDS: return "LEDS";
    case Component::STATUS_LED: return "LED STATUS";
    case Component::WIFI: return "WIFI";
    case Component::MQTT: return "MQTT";
    default: return "?";
  }
}

const char *componentCode(Component c) {
  switch (c) {
    case Component::OLED: return "OLED";
    case Component::DHT11: return "DHT11";
    case Component::LDR_AMBIENT: return "LDR_AMBIENT";
    case Component::LDR_LOCAL: return "LDR_LOCAL";
    case Component::ULTRASONIC: return "ULTRASONIC";
    case Component::IR: return "IR";
    case Component::MIC: return "MIC";
    case Component::MQ135: return "MQ135";
    case Component::LEDS: return "LEDS";
    case Component::STATUS_LED: return "STATUS_LED";
    case Component::WIFI: return "WIFI";
    case Component::MQTT: return "MQTT";
    default: return "UNKNOWN";
  }
}

bool componentFromCode(const char *code, Component &out) {
  for (uint8_t i = 0; i < COMPONENT_COUNT; i++) {
    Component c = static_cast<Component>(i);
    if (strcmp(code, componentCode(c)) == 0) {
      out = c;
      return true;
    }
  }
  return false;
}
