#include "DashboardServer.h"

#include "DashboardPage.h"
#include "SensorValidator.h"

namespace {
constexpr uint32_t TASK_STACK_BYTES = 8192;
constexpr uint32_t TASK_PERIOD_MS = 5;
constexpr BaseType_t WEB_CORE = 0;

char responseBuffer[Config::DASHBOARD_JSON_SIZE];
}  // namespace

void DashboardServer::begin() {
  if (!DASHBOARD_ENABLED) return;

  lock = xSemaphoreCreateMutex();
  server.on("/", HTTP_GET, [this] { handleRoot(); });
  server.on("/api/state", HTTP_GET, [this] { handleState(); });
  server.on("/api/test", HTTP_POST, [this] { handleRequest(RequestType::TEST); });
  server.on("/api/confirm", HTTP_POST, [this] { handleRequest(RequestType::CONFIRM); });
  server.onNotFound([this] { server.send(404, "text/plain", "Nao encontrado"); });
  server.begin();
  running = true;

  xTaskCreatePinnedToCore(taskEntry, "web", TASK_STACK_BYTES, this, 1, nullptr, WEB_CORE);
}

void DashboardServer::taskEntry(void *arg) {
  auto *self = static_cast<DashboardServer *>(arg);
  for (;;) {
    self->server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD_MS));
  }
}

void DashboardServer::setState(const char *json) {
  if (!running) return;
  xSemaphoreTake(lock, portMAX_DELAY);
  strlcpy(state, json, sizeof(state));
  xSemaphoreGive(lock);
}

DashboardServer::RequestType DashboardServer::takeRequest(Component &component) {
  if (!running) return RequestType::NONE;
  xSemaphoreTake(lock, portMAX_DELAY);
  RequestType type = pendingType;
  component = static_cast<Component>(pendingComponent);
  pendingType = RequestType::NONE;
  xSemaphoreGive(lock);
  return type;
}

void DashboardServer::handleRoot() {
  // Página já comprimida no build (web/scripts/embed.mjs); todo navegador atual aceita gzip
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char *>(DASHBOARD_HTML_GZ),
                DASHBOARD_HTML_GZ_LEN);
}

void DashboardServer::handleState() {
  xSemaphoreTake(lock, portMAX_DELAY);
  strlcpy(responseBuffer, state, sizeof(responseBuffer));
  xSemaphoreGive(lock);

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", responseBuffer);
}

void DashboardServer::handleRequest(RequestType type) {
  Component component;
  if (!componentFromCode(server.arg("c").c_str(), component)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"componente desconhecido\"}");
    return;
  }
  bool allowed = type == RequestType::TEST ? SensorValidator::testable(component)
                                           : SensorValidator::confirmable(component);
  if (!allowed) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"acao nao disponivel\"}");
    return;
  }

  xSemaphoreTake(lock, portMAX_DELAY);
  pendingType = type;
  pendingComponent = componentIndex(component);
  xSemaphoreGive(lock);
  server.send(202, "application/json", "{\"ok\":true}");
}
