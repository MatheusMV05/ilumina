#pragma once

#include <Arduino.h>

#include "ConnectivityManager.h"
#include "Diagnostics.h"
#include "LightingController.h"
#include "SensorManager.h"
#include "SensorValidator.h"

// Fotografia do sistema num instante, usada para montar os JSONs
struct AppSnapshot {
  const SensorReadings &readings;
  const LightingController &lighting;
  const Diagnostics &diag;
  const ConnectivityManager &net;
  const SensorValidator &validator;
  uint32_t now;
  bool mq135WarmingUp;
  bool presenceHc;  // presença vinda do HC-SR04 neste ciclo
  bool presenceIr;  // presença vinda do sensor IR neste ciclo
};

// Telemetria MQTT (formato da especificação). Sensor indisponível vira null.
size_t buildTelemetryJson(const AppSnapshot &s, char *out, size_t size);

// Saúde de cada componente e resumo do diagnóstico (tópico status)
size_t buildStatusJson(const AppSnapshot &s, char *out, size_t size);

// Estado completo para o dashboard: por componente, conexão, métricas e teste prático;
// mais a decisão de iluminação e os limites configurados
size_t buildDashboardJson(const AppSnapshot &s, char *out, size_t size);

// Explicação curta, em texto, de por que a iluminação está no nível atual
const char *lightingReason(const AppSnapshot &s);
