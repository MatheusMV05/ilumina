// Gestão Inteligente de Iluminação Pública com IoT — firmware principal.
// Este arquivo só coordena os módulos; a lógica fica em cada um deles.

#include <Arduino.h>

#include "AppConfig.h"
#include "Calibration.h"
#include "ConnectivityManager.h"
#include "DashboardServer.h"
#include "Diagnostics.h"
#include "LampMonitor.h"
#include "DisplayManager.h"
#include "LightingController.h"
#include "SensorManager.h"
#include "SensorValidator.h"
#include "StreetStats.h"
#include "Telemetry.h"

namespace {

SensorManager sensors;
LightingController lighting;
DisplayManager display;
Diagnostics diagnostics;
ConnectivityManager network;
DashboardServer dashboard;
SensorValidator validator;
Calibration calibration;
StreetStats street;
LampMonitor lamps;

char jsonBuffer[Config::DASHBOARD_JSON_SIZE];
char streetBuffer[Config::STREET_JSON_SIZE];

uint32_t lastSerialLogMs = 0;
uint32_t lastTelemetryMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastDashboardMs = 0;
uint32_t lastStreetJsonMs = 0;
bool mqttWasConnected = false;

bool due(uint32_t &last, uint32_t interval, uint32_t now) {
  if (last != 0 && now - last < interval) return false;
  last = now | 1;
  return true;
}

void logState(const AppSnapshot &s) {
  const SensorReadings &r = s.readings;
  Serial.printf(
      "[LOOP] LDR amb=%d%s usado=%d local=%d | dist=%s%.0fcm fundo=%.0fcm mov=%s IR=%s | %s %u%% (%s) | "
      "T=%.1f U=%.0f MQ=%d SOM=%.0f\n",
      r.ambientAdc.value, r.ambientAdc.valid ? "" : "(!)", s.ambientUsed, r.localAdc.value,
      r.distanceCm.valid ? "" : "sem eco/", r.distanceCm.value, r.motionBackgroundCm,
      r.ultrasonicPresence ? "SIM" : "nao", r.irDetected.value ? "SIM" : "nao",
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
  street.begin();
  dashboard.begin();

  diagnostics.attachLamps(lamps);
  diagnostics.runBootSequence(sensors, lighting, display, network, validator, oledFound,
                              oledAddress);
  // Verifica cada LED no fim do boot: um LED ausente já aparece como pendência
  lamps.check(lighting, sensors.readings().ambientLink.state == Link::CONNECTED, millis());
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
  // Comandos de luminária e verificação dos LEDs pedidos pelo painel
  DashboardServer::LampCommand lampCommands[Pins::LAMP_COUNT];
  dashboard.takeLampCommands(lampCommands, Pins::LAMP_COUNT);
  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) {
    if (!lampCommands[zone].pending) continue;
    lighting.setZoneControl(zone, static_cast<ZoneControl>(lampCommands[zone].control), lampCommands[zone].percent);
    Serial.printf("[LEDS] painel: L%u -> %s %u%%\n", zone + 1,
                  LightingController::controlLabel(lighting.zoneControl(zone)), lighting.manualPercent(zone));
  }
  if (dashboard.takeLampCheck()) lamps.check(lighting, r.ambientLink.state == Link::CONNECTED, now);
  validator.update(r, now);
  diagnostics.evaluate(r, network, validator, now);

  // Presença = HC-SR04 OU IR. Um sensor em falha confirmada não dispara presença.
  // O som (KY-038) não conta como presença: só é medido e exibido.
  bool presenceHc = r.ultrasonicPresence &&
                    diagnostics.health(Component::ULTRASONIC) != Health::FAILURE;
  bool presenceIr = r.irDetected.value;

  // Leitura que decide dia/noite: filtrada e sem o desvio que os próprios LEDs causam no LDR
  float lampLevel = 0;
  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) lampLevel += lighting.zonePercent(zone);
  lampLevel /= Pins::LAMP_COUNT * 100.0f;
  int ambientUsed = static_cast<int>(r.ambientFiltered - diagnostics.lampCrosstalk() * lampLevel + 0.5f);

  LightingInputs inputs;
  inputs.ambientValid = r.ambientAdc.valid && r.ambientFiltered >= 0;
  inputs.ambientAdc = ambientUsed;
  inputs.presenceDetected = presenceHc || presenceIr;
  inputs.presenceAvailable = diagnostics.presenceAvailable();
  lighting.update(inputs, now);

  // Estatísticas da rua: o som nunca conta como presença, só como nível de barulho
  int64_t clockMs;
  int16_t tzMinutes;
  if (dashboard.takeClock(clockMs, tzMinutes)) street.setClock(clockMs, tzMinutes);
  if (dashboard.takeStreetReset()) street.reset();
  street.update(r, inputs.presenceDetected, network.wifiConnected(), now);

  bool mq135WarmingUp = now < Config::MQ135_WARMUP_MS;
  display.update(now, r, lighting, diagnostics, network, mq135WarmingUp);

  AppSnapshot snapshot{r,   lighting,       diagnostics, network,    validator, lamps,
                       now, mq135WarmingUp, presenceHc,  presenceIr, ambientUsed};

  if (due(lastSerialLogMs, Config::SERIAL_LOG_INTERVAL_MS, now)) {
    logState(snapshot);
  }

  if (DASHBOARD_ENABLED && due(lastDashboardMs, Config::DASHBOARD_STATE_INTERVAL_MS, now)) {
    buildDashboardJson(snapshot, jsonBuffer, sizeof(jsonBuffer));
    dashboard.setState(jsonBuffer);
  }
  if (DASHBOARD_ENABLED && due(lastStreetJsonMs, Config::STREET_JSON_INTERVAL_MS, now)) {
    street.buildJson(streetBuffer, sizeof(streetBuffer));
    dashboard.setStreet(streetBuffer);
  }

  // Depuração pelo Serial Monitor: "j" imprime o JSON do dashboard; "h" lista os comandos de calibração
  while (Serial.available()) {
    char command = Serial.read();
    if (calibration.handle(command, sensors, lighting)) continue;
    if (command == 'j') {
      size_t length = buildDashboardJson(snapshot, jsonBuffer, sizeof(jsonBuffer));
      Serial.printf("[JSON %u/%u bytes] %s\n", length, sizeof(jsonBuffer), jsonBuffer);
    }
  }
  calibration.update(now, sensors, lighting);

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
