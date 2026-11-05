#pragma once

#include <Arduino.h>

#include "Components.h"
#include "SensorManager.h"
#include "SensorValidator.h"

class LightingController;
class DisplayManager;
class ConnectivityManager;

enum class Health : uint8_t {
  OK,             // ligado e funcionando (comunicação ou teste confirmados)
  WARNING,        // responde de forma suspeita, ou não reagiu ao teste prático
  FAILURE,        // não conectado ou erro de comunicação confirmado
  WARMING_UP,     // sensor em estabilização
  NOT_TESTED,     // depende de ação do usuário
  NOT_AVAILABLE,  // não disponível nesta configuração
  VISUAL_CHECK,   // comando enviado; confirmação só visual
};

enum class SystemSummary : uint8_t { NORMAL, ALERTS, SAFE_MODE };

// Mantém o estado de saúde de cada componente a partir de três evidências:
// conexão física, coerência do sinal e teste prático. Executa o diagnóstico de boot.
class Diagnostics {
 public:
  void runBootSequence(SensorManager &sensors, LightingController &lighting,
                       DisplayManager &display, ConnectivityManager &net,
                       const SensorValidator &validator, bool oledFound, uint8_t oledAddress);

  // Recalcula todos os estados com as evidências atuais
  void evaluate(const SensorReadings &readings, const ConnectivityManager &net,
                const SensorValidator &validator, uint32_t now);

  Health health(Component c) const { return entries[componentIndex(c)].health; }
  const char *note(Component c) const { return entries[componentIndex(c)].note; }
  const char *oledLabel(Component c) const;
  static const char *componentName(Component c) { return ::componentName(c); }
  static const char *healthSerialLabel(Health h);
  static const char *healthCode(Health h);
  static const char *summaryLabel(SystemSummary s);
  static const char *summaryCode(SystemSummary s);

  SystemSummary summary() const;
  bool hasAlerts() const;
  bool isAlert(Component c) const;

  // Sensor de presença só é considerado utilizável após funcionamento confirmado
  bool ultrasonicUsable() const { return health(Component::ULTRASONIC) == Health::OK; }
  bool irUsable() const { return health(Component::IR) == Health::OK; }
  bool presenceAvailable() const { return ultrasonicUsable() || irUsable(); }

  void printReport() const;
  int lamp1Delta() const { return lamp1OpticalDelta; }

 private:
  struct Entry {
    Health health = Health::NOT_TESTED;
    char note[48] = "";
  };

  void set(Component c, Health h, const char *noteFormat = "", ...);
  void evaluateSensor(Component c, const LinkStatus &link, const SensorValidator &validator,
                      Health connectedHealth, const char *connectedNote);
  void testLeds(SensorManager &sensors, LightingController &lighting, DisplayManager &display);
  void waitForEvidence(SensorManager &sensors, DisplayManager &display);
  void promptUltrasonic(SensorManager &sensors, DisplayManager &display);
  void promptIr(SensorManager &sensors, DisplayManager &display);
  void waitForNetwork(ConnectivityManager &net, DisplayManager &display);

  Entry entries[COMPONENT_COUNT];
  bool oledOk = false;
  uint8_t oledAddr = 0;
  bool logChanges = false;    // durante o boot o relatório sai de uma vez no final
  int lamp1OpticalDelta = 0;  // variação do LDR do poste com o LED 1 aceso no boot
};
