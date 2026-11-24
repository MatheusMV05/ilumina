#include "DashboardServer.h"

#include "DashboardPage.h"
#include "SensorValidator.h"

namespace {
constexpr uint32_t TASK_STACK_BYTES = 8192;
constexpr uint32_t TASK_PERIOD_MS = 5;
constexpr BaseType_t WEB_CORE = 0;

char responseBuffer[Config::DASHBOARD_JSON_SIZE];
char streetBuffer[Config::STREET_JSON_SIZE];
// Hora do painel só é aceita se for depois de 2024 e o fuso estiver entre -14h e +14h
constexpr int64_t MIN_CLOCK_MS = 1704067200000LL;
}  // namespace

void DashboardServer::begin() {
  if (!DASHBOARD_ENABLED) return;

  lock = xSemaphoreCreateMutex();
  server.on("/", HTTP_GET, [this] { handleRoot(); });
  server.on("/api/state", HTTP_GET, [this] { handleState(); });
  server.on("/api/test", HTTP_POST, [this] { handleRequest(RequestType::TEST); });
  server.on("/api/confirm", HTTP_POST, [this] { handleRequest(RequestType::CONFIRM); });
  server.on("/api/street", HTTP_GET, [this] { handleStreet(); });
  server.on("/api/time", HTTP_POST, [this] { handleTime(); });
  server.on("/api/street/reset", HTTP_POST, [this] { handleStreetReset(); });
  server.on("/api/lamp", HTTP_POST, [this] { handleLamp(); });
  server.on("/api/lamps/check", HTTP_POST, [this] { handleLampCheck(); });
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

void DashboardServer::setStreet(const char *json) {
  if (!running) return;
  xSemaphoreTake(lock, portMAX_DELAY);
  strlcpy(street, json, sizeof(street));
  xSemaphoreGive(lock);
}

bool DashboardServer::takeClock(int64_t &epochMs, int16_t &tzMinutes) {
  if (!running) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  bool pending = clockPending;
  epochMs = clockEpochMs;
  tzMinutes = clockTzMinutes;
  clockPending = false;
  xSemaphoreGive(lock);
  return pending;
}

bool DashboardServer::takeStreetReset() {
  if (!running) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  bool pending = streetResetPending;
  streetResetPending = false;
  xSemaphoreGive(lock);
  return pending;
}

void DashboardServer::handleStreet() {
  xSemaphoreTake(lock, portMAX_DELAY);
  strlcpy(streetBuffer, street, sizeof(streetBuffer));
  xSemaphoreGive(lock);

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", streetBuffer);
}

void DashboardServer::handleTime() {
  int64_t ms = atoll(server.arg("ms").c_str());
  long tz = server.arg("tz").toInt();
  if (ms < MIN_CLOCK_MS || tz < -14 * 60 || tz > 14 * 60) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"hora invalida\"}");
    return;
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  clockEpochMs = ms;
  clockTzMinutes = static_cast<int16_t>(tz);
  clockPending = true;
  xSemaphoreGive(lock);
  server.send(202, "application/json", "{\"ok\":true}");
}

void DashboardServer::handleStreetReset() {
  xSemaphoreTake(lock, portMAX_DELAY);
  streetResetPending = true;
  xSemaphoreGive(lock);
  server.send(202, "application/json", "{\"ok\":true}");
}

void DashboardServer::takeLampCommands(LampCommand out[], uint8_t count) {
  if (!running) return;
  xSemaphoreTake(lock, portMAX_DELAY);
  for (uint8_t z = 0; z < count && z < 4; z++) {
    out[z] = lampCommands[z];
    lampCommands[z].pending = false;
  }
  xSemaphoreGive(lock);
}

bool DashboardServer::takeLampCheck() {
  if (!running) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  bool pending = lampCheckPending;
  lampCheckPending = false;
  xSemaphoreGive(lock);
  return pending;
}

void DashboardServer::handleLamp() {
  long zone = server.arg("z").toInt();
  String mode = server.arg("mode");
  long pct = server.hasArg("pct") ? server.arg("pct").toInt() : Config::MANUAL_DEFAULT_PERCENT;
  uint8_t control;
  if (mode == "auto") control = 0;
  else if (mode == "off") control = 1;
  else if (mode == "on") control = 2;
  else control = 255;
  if (zone < 1 || zone > 4 || control == 255 || pct < 1 || pct > 100) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"comando invalido\"}");
    return;
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  LampCommand &cmd = lampCommands[zone - 1];
  cmd.pending = true;
  cmd.control = control;
  cmd.percent = static_cast<uint8_t>(pct);
  xSemaphoreGive(lock);
  server.send(202, "application/json", "{\"ok\":true}");
}

void DashboardServer::handleLampCheck() {
  xSemaphoreTake(lock, portMAX_DELAY);
  lampCheckPending = true;
  xSemaphoreGive(lock);
  server.send(202, "application/json", "{\"ok\":true}");
}
