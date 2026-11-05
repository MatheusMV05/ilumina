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
// Roda numa tarefa própria no núcleo 0: requisições lentas não afetam a iluminação.
// Nenhuma rota aciona saídas (a especificação não permite comandos remotos na v1).
class DashboardServer {
 public:
  enum class RequestType : uint8_t { NONE, TEST, CONFIRM };

  void begin();
  // Chamado pelo loop principal com o JSON mais recente
  void setState(const char *json);
  // Retira o pedido pendente vindo do painel, se houver
  RequestType takeRequest(Component &component);

 private:
  static void taskEntry(void *arg);
  void handleRoot();
  void handleState();
  void handleRequest(RequestType type);

  WebServer server{Config::DASHBOARD_PORT};
  SemaphoreHandle_t lock = nullptr;
  bool running = false;

  volatile RequestType pendingType = RequestType::NONE;
  volatile uint8_t pendingComponent = 0;

  char state[Config::DASHBOARD_JSON_SIZE] = "{}";
};
