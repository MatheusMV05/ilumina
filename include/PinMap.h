#pragma once

#include <Arduino.h>

// Mapa de pinos — placa NodeMCU-32S / ESP32 DevKit (módulo ESP-WROOM-32, 38 pinos).
// Conferir sempre pelo nome IMPRESSO na placa, não pela posição do desenho.
// Nenhum outro arquivo deve usar números de GPIO diretamente.
namespace Pins {

// OLED SSD1306 (I2C)
constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;

// DHT11
constexpr uint8_t DHT_DATA = 4;

// LDRs — divisor com 10 kΩ até o GND, no ADC1 (o ADC2 não funciona com Wi-Fi ligado)
constexpr uint8_t LDR_AMBIENT = 33;  // longe dos LEDs
constexpr uint8_t LDR_LOCAL = 32;    // LDR do poste, encostado no LED do poste 1

// HC-SR04 (alimentado pelo Vin, 5 V)
constexpr uint8_t ULTRASONIC_TRIG = 5;
constexpr uint8_t ULTRASONIC_ECHO = 18;  // via divisor 1 kΩ + 2 kΩ (ECHO é 5 V)

// KY-038 — sensor de som (alimentado em 3,3 V)
constexpr uint8_t SOUND_AO = 34;  // saída analógica; só entrada, sem pull interno
constexpr uint8_t SOUND_DO = 35;  // saída do comparador (limiar no trimpot); só entrada

// LEDs dos postes (luminárias), um por zona, via 220 Ω
constexpr uint8_t LAMP_COUNT = 4;
constexpr uint8_t LAMPS[LAMP_COUNT] = {25, 26, 27, 13};

// LED de status (firmware em execução), via 220 Ω
constexpr uint8_t STATUS_LED = 19;

// Não instalados nesta montagem (veja Config::IR_INSTALLED / MQ135_INSTALLED).
// Antes de habilitar, confirme que o pino escolhido está livre.
constexpr uint8_t IR_OUT = 23;
constexpr uint8_t MQ135_AO = 36;  // VP; ADC1, só entrada

}  // namespace Pins
