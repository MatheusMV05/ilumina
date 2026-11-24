#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "AppConfig.h"
#include "Components.h"

// Dashboard web servido pelo próprio ESP32.
//   GET  /                     página do painel (HTML/JS embutidos, sem internet)
//   GET  /api/state            estado completo em JSON
//   POST /api/test?c=CODIGO    inicia o teste prático de um sensor (só observa)
//   POST /api/confirm?c=CODIGO usuário confirma que viu os LEDs acenderem
//   GET  /api/street           estatísticas da rua por hora do dia
//   POST /api/time?ms=&tz=     painel informa a hora (epoch em ms) e o fuso (minutos)
//   POST /api/street/reset     zera as estatísticas da rua
//   POST /api/lamp?z=1..4&mode=auto|off|on&pct=N  comando de uma luminária
//   POST /api/lamps/check      verifica se cada LED está no circuito (pisca um por vez)
// Roda numa tarefa própria no núcleo 0: requisições lentas não afetam a iluminação.
// /api/lamp é a única rota que aciona saídas; o firmware volta tudo para AUTO ao reiniciar.
class DashboardServer {
 public:
  enum class RequestType : uint8_t { NONE, TEST, CONFIRM };

  void begin();
  // Chamado pelo loop principal com o JSON mais recente
  void setState(const char *json);
  // Retira o pedido pendente vindo do painel, se houver
  RequestType takeRequest(Component &component);

  void setStreet(const char *json);
  // Hora enviada pelo painel, se chegou uma nova desde a última chamada
  bool takeClock(int64_t &epochMs, int16_t &tzMinutes);
  bool takeStreetReset();

  struct LampCommand {
    bool pending = false;
    uint8_t control = 0;  // ZoneControl
    uint8_t percent = 100;
  };
  // Copia e limpa os comandos de luminária pendentes (um por zona)
  void takeLampCommands(LampCommand out[], uint8_t count);
  bool takeLampCheck();

 private:
  static void taskEntry(void *arg);
  void handleRoot();
  void handleState();
  void handleRequest(RequestType type);
  void handleStreet();
  void handleTime();
  void handleStreetReset();
  void handleLamp();
  void handleLampCheck();

  WebServer server{Config::DASHBOARD_PORT};
  SemaphoreHandle_t lock = nullptr;
  bool running = false;

  volatile RequestType pendingType = RequestType::NONE;
  volatile uint8_t pendingComponent = 0;

  char state[Config::DASHBOARD_JSON_SIZE] = "{}";
  char street[Config::STREET_JSON_SIZE] = "{}";
  bool clockPending = false;
  int64_t clockEpochMs = 0;
  int16_t clockTzMinutes = 0;
  bool streetResetPending = false;
  LampCommand lampCommands[4];
  bool lampCheckPending = false;
};
