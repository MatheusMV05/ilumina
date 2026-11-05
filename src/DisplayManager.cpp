#include "DisplayManager.h"

#include <Wire.h>
#include <stdarg.h>

#include "AppConfig.h"
#include "PinMap.h"

namespace {
constexpr uint8_t OLED_ADDRESSES[] = {0x3C, 0x3D};
constexpr uint8_t ROW_HEIGHT = 11;  // 6 linhas de 8 px em 64 px

// Ordem dos componentes nas duas páginas de diagnóstico
constexpr Component DIAG_PAGES[DisplayManager::DIAG_PAGE_COUNT][6] = {
    {Component::OLED, Component::DHT11, Component::LDR_AMBIENT, Component::LDR_LOCAL,
     Component::ULTRASONIC, Component::IR},
    {Component::MIC, Component::MQ135, Component::LEDS, Component::STATUS_LED, Component::WIFI,
     Component::MQTT},
};
}  // namespace

bool DisplayManager::begin(uint8_t &address) {
  pinMode(Pins::STATUS_LED, OUTPUT);
  setStatusLed(false);

  Wire.begin(Pins::OLED_SDA, Pins::OLED_SCL);
  address = 0;
  for (uint8_t candidate : OLED_ADDRESSES) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) {
      address = candidate;
      break;
    }
  }

  if (address == 0) {
    Serial.println("[OLED] Nao encontrado em 0x3C/0x3D. Confira SDA=21, SCL=22, VCC, GND.");
    return false;
  }
  if (!oled.begin(SSD1306_SWITCHCAPVCC, address)) {
    Serial.println("[OLED] Falha ao inicializar o SSD1306");
    return false;
  }
  oledFound = true;
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextWrap(false);
  return true;
}

void DisplayManager::setStatusLed(bool on) {
  bool level = Config::STATUS_LED_ACTIVE_LOW ? !on : on;
  digitalWrite(Pins::STATUS_LED, level ? HIGH : LOW);
}

// ---------- Primitivas ----------

void DisplayManager::beginFrame(const char *title) {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.print(title);
}

void DisplayManager::line(uint8_t row, const char *format, ...) {
  char text[24];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  oled.setCursor(0, row * ROW_HEIGHT);
  oled.print(text);
}

// ---------- Telas de inicialização ----------

void DisplayManager::showBoot() {
  if (!oledFound) return;
  beginFrame("ILUMINACAO IOT");
  line(2, "Inicializando...");
  line(3, "ESP32: OK");
  oled.display();
}

void DisplayManager::showPrompt(const char *title, const char *line1, const char *line2,
                                const char *line3) {
  if (!oledFound) return;
  beginFrame(title);
  line(2, "%s", line1);
  line(3, "%s", line2);
  line(4, "%s", line3);
  oled.display();
}

void DisplayManager::showDiagnosticsPage(const Diagnostics &diag, uint8_t pageIndex) {
  if (!oledFound || pageIndex >= DIAG_PAGE_COUNT) return;
  char title[24];
  snprintf(title, sizeof(title), "DIAGNOSTICO %u/%u", pageIndex + 1, DIAG_PAGE_COUNT);
  beginFrame(title);
  // 6 componentes por página: espaçamento menor que o das outras telas
  for (uint8_t i = 0; i < 6; i++) {
    Component c = DIAG_PAGES[pageIndex][i];
    oled.setCursor(0, 9 * (i + 1));
    oled.printf("%-10s %s", Diagnostics::componentName(c), diag.oledLabel(c));
  }
  oled.display();
}

void DisplayManager::showSummary(const Diagnostics &diag) {
  if (!oledFound) return;
  beginFrame("RESULTADO");
  switch (diag.summary()) {
    case SystemSummary::NORMAL: line(2, "SISTEMA NORMAL"); break;
    case SystemSummary::ALERTS:
      line(2, "SISTEMA COM");
      line(3, "ALERTAS");
      break;
    case SystemSummary::SAFE_MODE:
      line(2, "MODO SEGURO");
      line(3, "Luzes em nivel alto");
      break;
  }
  oled.display();
}

// ---------- Painel de operação ----------

void DisplayManager::update(uint32_t now, const SensorReadings &readings,
                            const LightingController &lighting, const Diagnostics &diag,
                            const ConnectivityManager &net, bool mq135WarmingUp) {
  if (!oledFound) return;

  if (now - pageStartMs >= Config::DISPLAY_PAGE_MS) {
    pageStartMs = now;
    page = static_cast<Page>((static_cast<uint8_t>(page) + 1) % static_cast<uint8_t>(Page::COUNT));
    if (page == Page::ALERTS && !diag.hasAlerts()) page = Page::OPERATION;
    lastDrawMs = 0;  // redesenha já na troca
  }
  if (lastDrawMs != 0 && now - lastDrawMs < Config::DISPLAY_INTERVAL_MS) return;
  lastDrawMs = now | 1;

  switch (page) {
    case Page::OPERATION: drawOperation(readings, lighting, diag); break;
    case Page::ENVIRONMENT: drawEnvironment(readings, mq135WarmingUp, now); break;
    case Page::MONITORING: drawMonitoring(readings, net, now); break;
    default: drawAlerts(diag); break;
  }
  oled.display();
}

void DisplayManager::drawOperation(const SensorReadings &readings,
                                   const LightingController &lighting, const Diagnostics &diag) {
  const char *period = !readings.ambientAdc.valid ? "N/D" : (lighting.isNight() ? "NOITE" : "DIA");
  char title[24];
  snprintf(title, sizeof(title), "ILUMINACAO: %s", period);
  beginFrame(title);
  if (diag.hasAlerts()) {
    oled.setCursor(122, 0);
    oled.print('!');
  }

  line(1, "LEDS: %u%%", lighting.zonePercent(0));
  line(2, "PRESENCA: %s", lighting.presenceActive() ? "SIM" : "NAO");
  line(3, "MODO: %s", LightingController::modeOledLabel(lighting.mode()));
  if (readings.distanceCm.valid) {
    line(4, "DIST: %.0fcm", readings.distanceCm.value);
  } else {
    line(4, "DIST: sem eco");
  }
  if (Config::IR_INSTALLED) line(5, "IR: %s", readings.irDetected.value ? "DETECTOU" : "LIVRE");
}

void DisplayManager::drawEnvironment(const SensorReadings &readings, bool mq135WarmingUp,
                                     uint32_t now) {
  beginFrame("AMBIENTE");
  if (readings.temperatureC.isFresh(now, Config::DHT_STALE_MS)) {
    line(1, "TEMP: %.1f C", readings.temperatureC.value);
    line(2, "UMID: %.0f%%", readings.humidity.value);
  } else {
    line(1, "TEMP: N/D");
    line(2, "UMID: N/D");
  }

  if (!Config::MQ135_INSTALLED) {
    // sem MQ-135 nesta montagem
  } else if (!readings.mq135Adc.valid) {
    line(3, "GAS ADC: N/D");
  } else {
    line(3, "GAS ADC: %d", readings.mq135Adc.value);
    line(4, "AR: %s", mq135WarmingUp ? "AQUECENDO"
                                     : airLevelOledLabel(classifyAir(readings.mq135Adc.value)));
  }
}

void DisplayManager::drawMonitoring(const SensorReadings &readings, const ConnectivityManager &net,
                                    uint32_t now) {
  beginFrame("MONITORAMENTO");
  if (readings.noiseRms.isFresh(now, Config::NOISE_STALE_MS)) {
    line(1, "SOM RMS: %.0f", readings.noiseRms.value);
  } else {
    line(1, "SOM RMS: N/D");
  }
  line(2, "LDR AMB: %d%s", readings.ambientAdc.value, readings.ambientAdc.valid ? "" : " !");
  line(3, "LDR LOCAL: %d%s", readings.localAdc.value, readings.localAdc.valid ? "" : " !");

  const char *network = !net.wifiConfigured() ? "LOCAL"
                        : !net.wifiConnected() ? "OFFLINE"
                        : net.mqttConnected()  ? "ONLINE+MQTT"
                                               : "ONLINE";
  line(4, "REDE: %s", network);
}

void DisplayManager::drawAlerts(const Diagnostics &diag) {
  beginFrame("ALERTAS");
  uint8_t row = 1;
  uint8_t hidden = 0;
  for (uint8_t i = 0; i < COMPONENT_COUNT; i++) {
    Component c = static_cast<Component>(i);
    if (!diag.isAlert(c)) continue;
    if (row <= 5) {
      line(row++, "%-10s %s", Diagnostics::componentName(c), diag.oledLabel(c));
    } else {
      hidden++;
    }
  }
  if (hidden > 0) {
    oled.setCursor(110, 0);
    oled.printf("+%u", hidden);
  }
}
