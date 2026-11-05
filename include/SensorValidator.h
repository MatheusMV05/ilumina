#pragma once

#include <Arduino.h>

#include "Components.h"
#include "SensorManager.h"

// Testes práticos guiados: o usuário provoca um estímulo físico (cobrir o LDR,
// bater palmas...) e o firmware confirma que o sensor reagiu. Só observa
// sensores — nenhum teste aciona saídas.
class SensorValidator {
 public:
  enum class State : uint8_t { IDLE, RUNNING, PASSED, FAILED };

  struct Result {
    State state = State::IDLE;
    uint32_t startedMs = 0;
    uint32_t finishedMs = 0;
    float baseline = 0;  // valor no início do teste
    float best = 0;      // maior reação observada (mesma unidade do critério)
    float target = 0;    // reação necessária para passar
  };

  static bool testable(Component c);
  static bool confirmable(Component c);  // confirmação visual pelo usuário (LEDs e LED de status)
  static uint32_t windowMs(Component c);

  // Retorna false se o componente não tiver teste ou outro teste estiver rodando
  bool start(Component c, const SensorReadings &r, uint32_t now);
  void confirm(Component c, uint32_t now);
  void update(const SensorReadings &r, uint32_t now);

  const Result &result(Component c) const { return results[componentIndex(c)]; }
  bool passed(Component c) const { return result(c).state == State::PASSED; }
  bool failed(Component c) const { return result(c).state == State::FAILED; }
  uint32_t remainingMs(Component c, uint32_t now) const;
  static const char *stateCode(State s);

 private:
  void finish(Component c, bool ok, uint32_t now);

  Result results[COMPONENT_COUNT];
  int8_t active = -1;

  // Bases extras de testes com duas grandezas ou contagem
  float baselineHumidity = 0;
  uint16_t baselineIrChanges = 0;
  uint16_t baselineSoundEvents = 0;
};
