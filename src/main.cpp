// Gestão Inteligente de Iluminação Pública com IoT — firmware principal.
// Este arquivo só coordena os módulos; a lógica fica em cada um deles.

#include <Arduino.h>

#include "AppConfig.h"
#include "ConnectivityManager.h"
#include "DashboardServer.h"
#include "Diagnostics.h"
#include "DisplayManager.h"
#include "LightingController.h"
#include "SensorManager.h"
#include "SensorValidator.h"
#include "Telemetry.h"

namespace {

SensorManager sensors;
LightingController lighting;
DisplayManager display;
Diagnostics diagnostics;
ConnectivityManager network;
DashboardServer dashboard;
SensorValidator validator;

char jsonBuffer[Config::DASHBOARD_JSON_SIZE];

uint32_t lastSerialLogMs = 0;
uint32_t lastTelemetryMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastDashboardMs = 0;
bool mqttWasConnected = false;

bool due(uint32_t &last, uint32_t interval, uint32_t now) {
  if (last != 0 && now - last < interval) return false;
  last = now | 1;
  return true;
}

void logState(const AppSnapshot &s) {
  const SensorReadings &r = s.readings;
  Serial.printf(
      "[LOOP] LDR amb=%d%s local=%d | dist=%s%.0fcm IR=%s | %s %u%% (%s) | T=%.1f U=%.0f "
      "MQ=%d SOM=%.0f\n",
      r.ambientAdc.value, r.ambientAdc.valid ? "" : "(!)", r.localAdc.value,
      r.distanceCm.valid ? "" : "sem eco/", r.distanceCm.value, r.irDetected.value ? "SIM" : "nao",
      s.readings.ambientAdc.valid ? (s.lighting.isNight() ? "NOITE" : "DIA") : "N/D",
      s.lighting.zonePercent(0), LightingController::modeTelemetryLabel(s.lighting.mode()),
      r.temperatureC.value, r.humidity.value, r.mq135Adc.value, r.noiseRms.value);
}

}  // namespace

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  delay(300);
  Serial.println("\n=== ILUMINACAO IOT ===");

  sensors.begin();
  lighting.begin();

  uint8_t oledAddress = 0;
  bool oledFound = display.begin(oledAddress);
  display.showBoot();
  delay(Config::BOOT_SCREEN_MS);

  // A rede começa a conectar em segundo plano durante os testes
  network.begin();
  dashboard.begin();

  diagnostics.runBootSequence(sensors, lighting, display, network, validator, oledFound,
                              oledAddress);
}

void loop() {
  uint32_t now = millis();

  sensors.update(now);
  const SensorReadings &r = sensors.readings();

  // Pedidos do dashboard: testes práticos e confirmações visuais
  Component requested;
  switch (dashboard.takeRequest(requested)) {
    case DashboardServer::RequestType::TEST: validator.start(requested, r, now); break;
    case DashboardServer::RequestType::CONFIRM: validator.confirm(requested, now); break;
    default: break;
  }
  validator.update(r, now);
  diagnostics.evaluate(r, network, validator, now);

  // Presença = HC-SR04 OU IR. Um sensor em falha confirmada não dispara presença.
  bool presenceHc = r.ultrasonicPresence &&
                    diagnostics.health(Component::ULTRASONIC) != Health::FAILURE;
  bool presenceIr = r.irDetected.value;

  LightingInputs inputs;
  inputs.ambientValid = r.ambientAdc.valid;
  inputs.ambientAdc = r.ambientAdc.value;
  inputs.presenceDetected = presenceHc || presenceIr;
  inputs.presenceAvailable = diagnostics.presenceAvailable();
  lighting.update(inputs, now);

  bool mq135WarmingUp = now < Config::MQ135_WARMUP_MS;
  display.update(now, r, lighting, diagnostics, network, mq135WarmingUp);

  AppSnapshot snapshot{r,   lighting,       diagnostics, network,   validator,
                       now, mq135WarmingUp, presenceHc,  presenceIr};

  if (due(lastSerialLogMs, Config::SERIAL_LOG_INTERVAL_MS, now)) {
    logState(snapshot);
  }

  if (DASHBOARD_ENABLED && due(lastDashboardMs, Config::DASHBOARD_STATE_INTERVAL_MS, now)) {
    buildDashboardJson(snapshot, jsonBuffer, sizeof(jsonBuffer));
    dashboard.setState(jsonBuffer);
  }

  // Depuração: enviar "j" pelo Serial Monitor imprime o JSON do dashboard
  if (Serial.available() && Serial.read() == 'j') {
    size_t length = buildDashboardJson(snapshot, jsonBuffer, sizeof(jsonBuffer));
    Serial.printf("[JSON %u/%u bytes] %s\n", length, sizeof(jsonBuffer), jsonBuffer);
  }

  if (network.mqttConfigured()) {
    // Status sai logo após (re)conectar e depois periodicamente
    bool mqttUp = network.mqttConnected();
    if ((mqttUp && !mqttWasConnected) || due(lastStatusMs, Config::STATUS_INTERVAL_MS, now)) {
      lastStatusMs = now | 1;
      buildStatusJson(snapshot, jsonBuffer, sizeof(jsonBuffer));
      network.publishStatus(jsonBuffer);
    }
    mqttWasConnected = mqttUp;

    if (due(lastTelemetryMs, Config::TELEMETRY_INTERVAL_MS, now)) {
      buildTelemetryJson(snapshot, jsonBuffer, sizeof(jsonBuffer));
      network.publishTelemetry(jsonBuffer);
    }
  }
}
