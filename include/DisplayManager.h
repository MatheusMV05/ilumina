#pragma once

#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include "ConnectivityManager.h"
#include "Diagnostics.h"
#include "LightingController.h"
#include "SensorManager.h"

// Interface local: OLED + LED de status.
// Recebe os dados prontos; não lê sensores diretamente.
// Se o OLED não for encontrado, todos os métodos de tela viram no-op.
class DisplayManager {
 public:
  bool begin(uint8_t &address);
  bool available() const { return oledFound; }

  void setStatusLed(bool on);

  void showBoot();
  void showPrompt(const char *title, const char *line1, const char *line2 = "",
                  const char *line3 = "");
  void showDiagnosticsPage(const Diagnostics &diag, uint8_t page);
  void showSummary(const Diagnostics &diag);

  // Painel de operação: redesenha em intervalos e alterna páginas sem bloquear
  void update(uint32_t now, const SensorReadings &readings, const LightingController &lighting,
              const Diagnostics &diag, const ConnectivityManager &net, bool mq135WarmingUp);

  static constexpr uint8_t DIAG_PAGE_COUNT = 2;

 private:
  enum class Page : uint8_t { OPERATION, ENVIRONMENT, MONITORING, ALERTS, COUNT };

  void beginFrame(const char *title);
  void line(uint8_t row, const char *format, ...);
  void drawOperation(const SensorReadings &readings, const LightingController &lighting,
                     const Diagnostics &diag);
  void drawEnvironment(const SensorReadings &readings, bool mq135WarmingUp, uint32_t now);
  void drawMonitoring(const SensorReadings &readings, const ConnectivityManager &net,
                      uint32_t now);
  void drawAlerts(const Diagnostics &diag);

  Adafruit_SSD1306 oled{128, 64, &Wire, -1};
  bool oledFound = false;
  Page page = Page::OPERATION;
  uint32_t lastDrawMs = 0;
  uint32_t pageStartMs = 0;
};
