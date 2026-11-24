#pragma once

#include <Arduino.h>

class SensorManager;
class LightingController;

// Ferramentas de calibração pelo Serial Monitor (115200). Envie uma letra:
//   h  ajuda
//   c  liga/desliga o fluxo CSV (linhas "CAL,...", 4 por segundo) com os dados crus
//   p  relatório de ligação: pull dos LDRs e teste elétrico dos 4 LEDs
//   o  teste óptico: acende cada LED e mede quanto cada LDR sobe
//   s  varredura dos pinos: mostra o que está ligado em cada GPIO (acha fio no pino errado)
//   j  JSON do dashboard (tratado no main)
// Nada aqui muda a lógica de iluminação; o teste óptico só pausa o loop por ~10 s.
class Calibration {
 public:
  // Devolve true se o comando era de calibração
  bool handle(char command, SensorManager &sensors, LightingController &lighting);
  void update(uint32_t now, const SensorManager &sensors, LightingController &lighting);

 private:
  void printHelp() const;
  void printProbeReport(const SensorManager &sensors, LightingController &lighting) const;
  void runOpticalTest(LightingController &lighting) const;
  void scanPins() const;

  bool streaming = false;
  uint32_t lastRowMs = 0;
};
