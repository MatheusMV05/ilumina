#pragma once

#include <Arduino.h>

#include "LightingController.h"
#include "PinMap.h"

enum class LampStatus : uint8_t {
  UNKNOWN,  // ainda não verificado, ou sem como medir (LDR ambiente desligado)
  OK,       // o LED puxou corrente quando aceso
  MISSING,  // aceso não mudou nada: LED ausente, invertido, queimado ou fio solto
};

// Verifica se cada LED está de fato no circuito.
//
// O pino de saída não sente a carga, e o teste com pull-up/pull-down não separa LED de
// fio solto (a tensão do LED cai na faixa indefinida da entrada digital). O que funciona
// nesta montagem: um LED aceso puxa corrente pelo GND/3V3 compartilhado e desloca a
// leitura do LDR ambiente (+25 a +111 ADC medidos). Um LED ausente não desloca nada.
//
// A verificação pisca um LED por vez (~0,6 s cada) e compara a resposta de cada um com
// a do LED que respondeu mais forte, porque o tamanho do desvio muda com a luz ambiente.
class LampMonitor {
 public:
  // Bloqueia ~2,5 s. Devolve os LEDs ao brilho em que estavam.
  void check(LightingController &lighting, bool ambientLdrConnected, uint32_t now);

  LampStatus status(uint8_t zone) const { return zone < Pins::LAMP_COUNT ? status_[zone] : LampStatus::UNKNOWN; }
  int delta(uint8_t zone) const { return zone < Pins::LAMP_COUNT ? delta_[zone] : 0; }
  bool checked() const { return checkedAtMs != 0; }
  uint32_t lastCheckMs() const { return checkedAtMs; }
  uint8_t missingCount() const;
  // Nenhum LED mudou a leitura: não dá para separar "todos soltos" de "sem sinal para medir"
  bool noSignal() const { return noSignal_; }

  static const char *statusLabel(LampStatus s);

 private:
  LampStatus status_[Pins::LAMP_COUNT] = {};
  int delta_[Pins::LAMP_COUNT] = {};
  uint32_t checkedAtMs = 0;
  bool noSignal_ = false;
};
