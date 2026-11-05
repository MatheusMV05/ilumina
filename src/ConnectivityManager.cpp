#include "ConnectivityManager.h"

#include "AppConfig.h"

namespace {
constexpr uint32_t TASK_STACK_BYTES = 6144;
constexpr uint32_t TASK_PERIOD_MS = 50;
constexpr BaseType_t NETWORK_CORE = 0;  // o loop do Arduino roda no núcleo 1

char sendBuffer[768];
}  // namespace

void ConnectivityManager::begin() {
  configured = strlen(WIFI_SSID) > 0;
  mqttEnabled = configured && MQTT_ENABLED && strlen(MQTT_HOST) > 0;

  // Rede própria do dashboard: funciona com ou sem roteador
  if (DASHBOARD_ENABLED) {
    WiFi.mode(configured ? WIFI_AP_STA : WIFI_AP);
    apActive = WiFi.softAP(DASHBOARD_AP_SSID, DASHBOARD_AP_PASSWORD);
    if (apActive) {
      Serial.printf("[WEB] Rede \"%s\" ativa. Dashboard: http://%s\n", DASHBOARD_AP_SSID,
                    WiFi.softAPIP().toString().c_str());
    } else {
      Serial.println("[WEB] Falha ao criar a rede do dashboard");
    }
  }

  if (!configured) {
    if (!apActive) WiFi.mode(WIFI_OFF);
    Serial.println("[WIFI] Sem credenciais: operando em MODO LOCAL");
    return;
  }

  snprintf(topicTelemetry, sizeof(topicTelemetry), "public-lighting/%s/telemetry", DEVICE_ID);
  snprintf(topicStatus, sizeof(topicStatus), "public-lighting/%s/status", DEVICE_ID);
  snprintf(topicAvailability, sizeof(topicAvailability), "public-lighting/%s/availability",
           DEVICE_ID);

  if (mqttEnabled) {
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(Config::MQTT_BUFFER_SIZE);
    mqtt.setSocketTimeout(Config::MQTT_SOCKET_TIMEOUT_S);
    mqtt.setKeepAlive(30);
  } else {
    Serial.println("[MQTT] Desabilitado ou sem host configurado");
  }

  lock = xSemaphoreCreateMutex();

  if (!DASHBOARD_ENABLED) WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // A senha nunca é impressa
  Serial.printf("[WIFI] Conectando a \"%s\"...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiStartMs = millis();
  lastWifiAttemptMs = wifiStartMs;

  xTaskCreatePinnedToCore(taskEntry, "net", TASK_STACK_BYTES, this, 1, nullptr, NETWORK_CORE);
}

void ConnectivityManager::taskEntry(void *arg) {
  static_cast<ConnectivityManager *>(arg)->taskLoop();
}

void ConnectivityManager::taskLoop() {
  for (;;) {
    uint32_t now = millis();
    handleWifi(now);
    if (mqttEnabled) handleMqtt(now);
    vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD_MS));
  }
}

void ConnectivityManager::handleWifi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiUp) {
      wifiUp = true;
      Serial.printf("[WIFI] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }

  if (wifiUp) {
    wifiUp = false;
    mqttUp = false;
    Serial.println("[WIFI] Conexao perdida. Controle local continua.");
  }

  if (now - lastWifiAttemptMs >= Config::WIFI_RETRY_INTERVAL_MS) {
    lastWifiAttemptMs = now;
    Serial.println("[WIFI] Nova tentativa de conexao");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void ConnectivityManager::handleMqtt(uint32_t now) {
  if (!wifiUp) {
    mqttUp = false;
    return;
  }

  if (!mqtt.connected()) {
    if (mqttUp) {
      mqttUp = false;
      Serial.println("[MQTT] Conexao perdida");
    }
    if (lastMqttAttemptMs != 0 && now - lastMqttAttemptMs < Config::MQTT_RETRY_INTERVAL_MS) {
      return;
    }
    lastMqttAttemptMs = now | 1;

    const char *user = strlen(MQTT_USERNAME) > 0 ? MQTT_USERNAME : nullptr;
    const char *pass = strlen(MQTT_PASSWORD) > 0 ? MQTT_PASSWORD : nullptr;
    Serial.printf("[MQTT] Conectando a %s:%d...\n", MQTT_HOST, MQTT_PORT);
    if (mqtt.connect(DEVICE_ID, user, pass, topicAvailability, 1, true, "offline")) {
      mqtt.publish(topicAvailability, "online", true);
      mqttUp = true;
      Serial.println("[MQTT] Conectado");
    } else {
      Serial.printf("[MQTT] Falha (estado %d). Nova tentativa em %lus\n", mqtt.state(),
                    Config::MQTT_RETRY_INTERVAL_MS / 1000);
      return;
    }
  }

  mqtt.loop();
  flushPending();
}

void ConnectivityManager::flushPending() {
  const char *topics[] = {topicTelemetry, topicStatus};
  char *buffers[] = {pendingTelemetry, pendingStatus};
  bool *flags[] = {&hasTelemetry, &hasStatus};

  for (uint8_t i = 0; i < 2; i++) {
    bool send = false;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (*flags[i]) {
      strlcpy(sendBuffer, buffers[i], sizeof(sendBuffer));
      *flags[i] = false;
      send = true;
    }
    xSemaphoreGive(lock);

    if (send && !mqtt.publish(topics[i], sendBuffer)) {
      Serial.printf("[MQTT] Falha ao publicar em %s\n", topics[i]);
    }
  }
}

void ConnectivityManager::queue(char *buffer, bool &pending, const char *json) {
  if (!mqttEnabled || !mqttUp) return;  // offline: descarta, sem acumular
  xSemaphoreTake(lock, portMAX_DELAY);
  strlcpy(buffer, json, MESSAGE_SIZE);
  pending = true;
  xSemaphoreGive(lock);
}

void ConnectivityManager::publishTelemetry(const char *json) {
  queue(pendingTelemetry, hasTelemetry, json);
}

void ConnectivityManager::publishStatus(const char *json) {
  queue(pendingStatus, hasStatus, json);
}
