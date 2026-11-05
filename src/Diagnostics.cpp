#include "Diagnostics.h"

#include <stdarg.h>

#include "AppConfig.h"
#include "ConnectivityManager.h"
#include "DisplayManager.h"
#include "LightingController.h"
#include "PinMap.h"

namespace {

// Pisca o LED de status durante as etapas do boot
void blinkStatus(DisplayManager &display) {
  display.setStatusLed((millis() / 250) % 2 == 0);
}

// Texto curto (ASCII) explicando por que um sensor não parece ligado
const char *reasonNote(Component c, LinkReason reason) {
  switch (reason) {
    case LinkReason::FLOATING:
      switch (c) {
        case Component::ULTRASONIC: return "ECHO solto ou sem divisor";
        case Component::MQ135: return "AO solto ou sem divisor";
        case Component::IR: return "saida OUT solta";
        case Component::MIC: return "AO solto: KY-038 nao ligado";
        default: return "pino solto: sensor nao ligado";
      }
    case LinkReason::STUCK_LOW:
      if (c == Component::MQ135) return "sem sinal: modulo sem 5V?";
      if (c == Component::MIC) return "AO em 0: KY-038 sem 3V3?";
      return "quase 0: LDR fora da fileira?";
    case LinkReason::STUCK_HIGH:
      if (c == Component::MQ135) return "no maximo: conferir divisor";
      if (c == Component::MIC) return "AO no maximo: conferir ligacao";
      return "no maximo: falta resistor 10k?";
    case LinkReason::NO_RESPONSE:
      return c == Component::DHT11 ? "sem resposta: conferir DATA/VCC" : "modulo nao responde: 5V/TRIG";
    case LinkReason::ECHO_STUCK: return "ECHO preso em HIGH";
    case LinkReason::INTERMITTENT: return "respostas intermitentes";
    default: return "";
  }
}

}  // namespace

// ---------- Rótulos ----------

const char *Diagnostics::healthSerialLabel(Health h) {
  switch (h) {
    case Health::OK: return "OK";
    case Health::WARNING: return "ATENÇÃO";
    case Health::FAILURE: return "FALHA";
    case Health::WARMING_UP: return "AQUECENDO";
    case Health::NOT_TESTED: return "NÃO TESTADO";
    case Health::NOT_AVAILABLE: return "N/D";
    case Health::VISUAL_CHECK: return "TESTE VISUAL";
  }
  return "?";
}

const char *Diagnostics::healthCode(Health h) {
  switch (h) {
    case Health::OK: return "OK";
    case Health::WARNING: return "WARNING";
    case Health::FAILURE: return "FAILURE";
    case Health::WARMING_UP: return "WARMING_UP";
    case Health::NOT_TESTED: return "NOT_TESTED";
    case Health::NOT_AVAILABLE: return "NOT_AVAILABLE";
    case Health::VISUAL_CHECK: return "VISUAL_CHECK";
  }
  return "UNKNOWN";
}

const char *Diagnostics::oledLabel(Component c) const {
  Health h = health(c);
  // Rede indisponível não é falha do sistema: o controle segue local
  if ((c == Component::WIFI || c == Component::MQTT) && h == Health::WARNING) return "OFFLINE";
  switch (h) {
    case Health::OK: return "OK";
    case Health::WARNING: return "ATENCAO";
    case Health::FAILURE: return "FALHA";
    case Health::WARMING_UP: return "AQUEC.";
    case Health::NOT_TESTED: return "NAO TEST.";
    case Health::NOT_AVAILABLE: return c == Component::WIFI ? "LOCAL" : "N/D";
    case Health::VISUAL_CHECK: return "VISUAL";
  }
  return "?";
}

const char *Diagnostics::summaryLabel(SystemSummary s) {
  switch (s) {
    case SystemSummary::NORMAL: return "SISTEMA NORMAL";
    case SystemSummary::ALERTS: return "SISTEMA COM ALERTAS";
    default: return "MODO SEGURO";
  }
}

const char *Diagnostics::summaryCode(SystemSummary s) {
  switch (s) {
    case SystemSummary::NORMAL: return "NORMAL";
    case SystemSummary::ALERTS: return "ALERTS";
    default: return "SAFE_MODE";
  }
}

// ---------- Estado e resumo ----------

void Diagnostics::set(Component c, Health h, const char *noteFormat, ...) {
  Entry &e = entries[componentIndex(c)];
  Health previous = e.health;
  char previousNote[sizeof(e.note)];
  strlcpy(previousNote, e.note, sizeof(previousNote));

  va_list args;
  va_start(args, noteFormat);
  vsnprintf(e.note, sizeof(e.note), noteFormat, args);
  va_end(args);
  e.health = h;

  if (logChanges && previous != h) {
    Serial.printf("[DIAG] %s: %s -> %s (%s)\n", componentName(c), healthSerialLabel(previous),
                  healthSerialLabel(h), e.note);
  }
}

bool Diagnostics::isAlert(Component c) const {
  Health h = health(c);
  // Aquecimento e teste visual são etapas esperadas, não alertas
  return h == Health::WARNING || h == Health::FAILURE || h == Health::NOT_TESTED;
}

bool Diagnostics::hasAlerts() const {
  for (uint8_t i = 0; i < COMPONENT_COUNT; i++) {
    if (isAlert(static_cast<Component>(i))) return true;
  }
  return false;
}

SystemSummary Diagnostics::summary() const {
  if (health(Component::LDR_AMBIENT) != Health::OK || !presenceAvailable()) {
    return SystemSummary::SAFE_MODE;
  }
  return hasAlerts() ? SystemSummary::ALERTS : SystemSummary::NORMAL;
}

void Diagnostics::printReport() const {
  Serial.println("\n========== DIAGNOSTICO ==========");
  for (uint8_t i = 0; i < COMPONENT_COUNT; i++) {
    Component c = static_cast<Component>(i);
    Serial.printf("  %-10s %-13s %s\n", componentName(c), healthSerialLabel(health(c)), note(c));
  }
  Serial.printf("  Resultado: %s\n", summaryLabel(summary()));
  if (!presenceAvailable()) {
    Serial.println("  Nenhum sensor de presenca confirmado: a noite as luzes ficam em nivel alto.");
  }
  Serial.println("=================================\n");
}

// ---------- Avaliação ----------

void Diagnostics::evaluateSensor(Component c, const LinkStatus &link,
                                 const SensorValidator &validator, Health connectedHealth,
                                 const char *connectedNote) {
  switch (link.state) {
    case Link::NOT_CONNECTED:
      set(c, Health::FAILURE, "%s", reasonNote(c, link.reason));
      return;
    case Link::SUSPECT:
      set(c, Health::WARNING, "%s", reasonNote(c, link.reason));
      return;
    case Link::UNKNOWN:
      set(c, Health::NOT_TESTED, "aguardando leituras");
      return;
    case Link::CONNECTED:
      break;
  }
  if (validator.passed(c)) {
    set(c, Health::OK, "validado no teste pratico");
  } else if (validator.failed(c) && connectedHealth != Health::WARMING_UP) {
    set(c, Health::WARNING, "conectado, mas nao reagiu ao teste");
  } else {
    set(c, connectedHealth, "%s", connectedNote);
  }
}

void Diagnostics::evaluate(const SensorReadings &r, const ConnectivityManager &net,
                           const SensorValidator &validator, uint32_t now) {
  if (oledOk) set(Component::OLED, Health::OK, "endereco 0x%02X", oledAddr);
  else set(Component::OLED, Health::FAILURE, "nao encontrado no I2C");

  evaluateSensor(Component::LDR_AMBIENT, r.ambientLink, validator, Health::OK,
                 "conectado, sinal estavel");
  evaluateSensor(Component::LDR_LOCAL, r.localLink, validator, Health::OK,
                 "conectado, sinal estavel");

  char text[48];
  snprintf(text, sizeof(text), "modulo responde, eco em %u%%", r.echoValidPercent);
  evaluateSensor(Component::ULTRASONIC, r.ultrasonicLink, validator, Health::OK, text);

  // Saída parada prova a ligação, mas não que o sensor detecta algo
  if (!Config::IR_INSTALLED) {
    set(Component::IR, Health::NOT_AVAILABLE, "nao instalado nesta montagem");
  } else if (r.irChanges > 0) {
    evaluateSensor(Component::IR, r.irLink, validator, Health::OK, "conectado e ja detectou");
  } else {
    evaluateSensor(Component::IR, r.irLink, validator, Health::NOT_TESTED,
                   "conectado; passe a mao para confirmar");
  }

  snprintf(text, sizeof(text), "respondendo (%u%% das leituras)", r.dhtSuccessPercent);
  evaluateSensor(Component::DHT11, r.dhtLink, validator, Health::OK, text);

  if (!Config::MQ135_INSTALLED) {
    set(Component::MQ135, Health::NOT_AVAILABLE, "nao instalado nesta montagem");
  } else if (now < Config::MQ135_WARMUP_MS) {
    snprintf(text, sizeof(text), "aquecendo, faltam %lus", (Config::MQ135_WARMUP_MS - now) / 1000);
    evaluateSensor(Component::MQ135, r.mq135Link, validator, Health::WARMING_UP, text);
  } else {
    evaluateSensor(Component::MQ135, r.mq135Link, validator, Health::OK, "leitura relativa estavel");
  }

  snprintf(text, sizeof(text), "ouvindo; comparador disparou %u vez(es)", r.soundDoEvents);
  evaluateSensor(Component::MIC, r.micLink, validator, Health::OK, text);

  // LEDs não têm realimentação: só o usuário pode confirmar
  // Exceção: o LED 1 é medido pelo LDR do poste, que fica encostado nele
  if (validator.passed(Component::LEDS)) {
    set(Component::LEDS, Health::OK, "confirmado pelo usuario");
  } else if (r.localLink.state != Link::CONNECTED) {
    set(Component::LEDS, Health::VISUAL_CHECK, "confirme no painel se acenderam");
  } else if (lamp1OpticalDelta >= Config::LAMP1_OPTICAL_MIN_DELTA) {
    set(Component::LEDS, Health::VISUAL_CHECK, "LED 1 medido (+%d); confirme os demais",
        lamp1OpticalDelta);
  } else {
    set(Component::LEDS, Health::WARNING, "LDR do poste nao viu o LED 1 (%+d)", lamp1OpticalDelta);
  }
  if (validator.passed(Component::STATUS_LED)) {
    set(Component::STATUS_LED, Health::OK, "confirmado pelo usuario");
  } else {
    set(Component::STATUS_LED, Health::VISUAL_CHECK, "confirme no painel se acendeu");
  }

  if (!net.wifiConfigured()) {
    set(Component::WIFI, Health::NOT_AVAILABLE, "sem credenciais (modo local)");
    set(Component::MQTT, Health::NOT_AVAILABLE, "desabilitado");
  } else {
    if (net.wifiConnected()) set(Component::WIFI, Health::OK, "conectado ao roteador");
    else set(Component::WIFI, Health::WARNING, "modo local, tentando conectar");
    if (!net.mqttConfigured()) set(Component::MQTT, Health::NOT_AVAILABLE, "desabilitado");
    else if (net.mqttConnected()) set(Component::MQTT, Health::OK, "conectado ao broker");
    else set(Component::MQTT, Health::WARNING, "broker indisponivel");
  }
}

// ---------- Sequência de boot ----------

void Diagnostics::runBootSequence(SensorManager &sensors, LightingController &lighting,
                                  DisplayManager &display, ConnectivityManager &net,
                                  const SensorValidator &validator, bool oledFound,
                                  uint8_t oledAddress) {
  logChanges = false;
  oledOk = oledFound;
  oledAddr = oledAddress;
  Serial.println("[DIAG] Iniciando diagnostico...");

  testLeds(sensors, lighting, display);
  waitForEvidence(sensors, display);
  promptUltrasonic(sensors, display);
  promptIr(sensors, display);
  waitForNetwork(net, display);

  evaluate(sensors.readings(), net, validator, millis());
  printReport();

  for (uint8_t p = 0; p < DisplayManager::DIAG_PAGE_COUNT; p++) {
    display.showDiagnosticsPage(*this, p);
    if (display.available()) delay(Config::DIAG_PAGE_MS);
  }
  display.showSummary(*this);
  if (display.available()) delay(Config::DIAG_PAGE_MS);

  display.setStatusLed(true);  // verde fixo: firmware em execução
  logChanges = true;
}

void Diagnostics::testLeds(SensorManager &sensors, LightingController &lighting,
                           DisplayManager &display) {
  char step[20];
  int dark = sensors.sampleLocalLdr();
  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) {
    snprintf(step, sizeof(step), "LED %u de %u", zone + 1, Pins::LAMP_COUNT);
    display.showPrompt("TESTE DE LEDS", "Verifique LEDs", step);
    lighting.setZoneImmediate(zone, 100);
    delay(Config::LED_TEST_STEP_MS);
    if (zone == 0) lamp1OpticalDelta = sensors.sampleLocalLdr() - dark;
    lighting.setZoneImmediate(zone, 0);
  }
  Serial.printf("[DIAG] LED 1 visto pelo LDR do poste: %+d ADC\n", lamp1OpticalDelta);
  display.showPrompt("TESTE DE LEDS", "Verifique LEDs", "LED de status");
  display.setStatusLed(true);
  delay(Config::LED_TEST_STEP_MS);
  display.setStatusLed(false);
}

// Roda os sensores por alguns segundos para juntar evidência de conexão
void Diagnostics::waitForEvidence(SensorManager &sensors, DisplayManager &display) {
  display.showPrompt("DIAGNOSTICO", "Verificando", "sensores...");
  uint32_t start = millis();
  while (millis() - start < Config::BOOT_SETTLE_MS) {
    sensors.update(millis());
    blinkStatus(display);
    delay(5);
  }
}

void Diagnostics::promptUltrasonic(SensorManager &sensors, DisplayManager &display) {
  // Sem módulo detectado não adianta pedir ação ao usuário
  if (sensors.readings().ultrasonicLink.state != Link::CONNECTED) return;

  uint32_t start = millis();
  char remaining[20];
  while (millis() - start < Config::ULTRASONIC_TEST_WINDOW_MS &&
         !sensors.readings().ultrasonicEverEchoed) {
    uint32_t left = (Config::ULTRASONIC_TEST_WINDOW_MS - (millis() - start)) / 1000 + 1;
    snprintf(remaining, sizeof(remaining), "Restam %lus", left);
    display.showPrompt("TESTE HC-SR04", "Aproxime objeto", "10-50cm", remaining);
    blinkStatus(display);
    sensors.update(millis());
    delay(5);
  }
}

void Diagnostics::promptIr(SensorManager &sensors, DisplayManager &display) {
  if (sensors.readings().irLink.state != Link::CONNECTED) return;

  uint32_t start = millis();
  char remaining[20];
  while (millis() - start < Config::IR_TEST_WINDOW_MS && sensors.readings().irChanges == 0) {
    uint32_t left = (Config::IR_TEST_WINDOW_MS - (millis() - start)) / 1000 + 1;
    snprintf(remaining, sizeof(remaining), "Restam %lus", left);
    display.showPrompt("TESTE IR", "Passe objeto", "no IR", remaining);
    blinkStatus(display);
    sensors.update(millis());
    delay(5);
  }
}

void Diagnostics::waitForNetwork(ConnectivityManager &net, DisplayManager &display) {
  if (!net.wifiConfigured()) return;

  // A conexão começou antes dos outros testes; espera só o que faltar do timeout
  display.showPrompt("DIAGNOSTICO", "Conectando Wi-Fi...");
  uint32_t deadline = net.wifiStartedMs() + Config::WIFI_CONNECT_TIMEOUT_MS;
  while (!net.wifiConnected() && static_cast<int32_t>(deadline - millis()) > 0) delay(100);

  if (!net.mqttConfigured()) return;
  uint32_t start = millis();
  while (net.wifiConnected() && !net.mqttConnected() &&
         millis() - start < Config::MQTT_SOCKET_TIMEOUT_S * 1000UL + 1000) {
    delay(100);
  }
}
