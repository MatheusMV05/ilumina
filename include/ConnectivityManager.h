#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

// Gerencia Wi-Fi e MQTT em uma tarefa própria (núcleo 0), para que conexões
// lentas ou quedas de rede nunca travem o controle da iluminação no loop.
// Não controla LEDs.
class ConnectivityManager {
 public:
  void begin();

  bool wifiConfigured() const { return configured; }
  bool mqttConfigured() const { return mqttEnabled; }
  bool wifiConnected() const { return wifiUp; }
  bool mqttConnected() const { return mqttUp; }
  uint32_t wifiStartedMs() const { return wifiStartMs; }
  bool dashboardApActive() const { return apActive; }

  // Enfileira a última mensagem; é descartada se estiver offline
  void publishTelemetry(const char *json);
  void publishStatus(const char *json);

 private:
  static void taskEntry(void *arg);
  void taskLoop();
  void handleWifi(uint32_t now);
  void handleMqtt(uint32_t now);
  void flushPending();
  void queue(char *buffer, bool &pending, const char *json);

  WiFiClient wifiClient;
  PubSubClient mqtt{wifiClient};
  SemaphoreHandle_t lock = nullptr;

  bool configured = false;
  bool mqttEnabled = false;
  bool apActive = false;
  volatile bool wifiUp = false;
  volatile bool mqttUp = false;

  uint32_t wifiStartMs = 0;
  uint32_t lastWifiAttemptMs = 0;
  uint32_t lastMqttAttemptMs = 0;

  char topicTelemetry[96] = "";
  char topicStatus[96] = "";
  char topicAvailability[96] = "";

  static constexpr size_t MESSAGE_SIZE = 768;
  char pendingTelemetry[MESSAGE_SIZE] = "";
  char pendingStatus[MESSAGE_SIZE] = "";
  bool hasTelemetry = false;
  bool hasStatus = false;
};
