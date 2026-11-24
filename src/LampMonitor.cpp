#include "LampMonitor.h"

#include "AppConfig.h"
#include "SensorManager.h"

void LampMonitor::check(LightingController &lighting, bool ambientLdrConnected, uint32_t now) {
  checkedAtMs = now | 1;
  noSignal_ = false;
  if (!ambientLdrConnected) {
    for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) {
      status_[z] = LampStatus::UNKNOWN;
      delta_[z] = 0;
    }
    return;
  }

  uint8_t saved[Pins::LAMP_COUNT];
  for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) saved[z] = lighting.zonePercent(z);

  // Cada LED alterna apagado/aceso algumas vezes; a média das diferenças descarta
  // variações lentas da luz ambiente durante a medição
  for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) {
    long sum = 0;
    for (uint8_t round = 0; round < Config::LAMP_CHECK_ROUNDS; round++) {
      lighting.setZoneImmediate(z, 0);
      delay(Config::LAMP_CHECK_SETTLE_MS);
      int off = SensorManager::sampleLdr(Pins::LDR_AMBIENT);
      lighting.setZoneImmediate(z, 100);
      delay(Config::LAMP_CHECK_SETTLE_MS);
      int on = SensorManager::sampleLdr(Pins::LDR_AMBIENT);
      sum += on - off;
    }
    delta_[z] = static_cast<int>(sum / Config::LAMP_CHECK_ROUNDS);
    lighting.setZoneImmediate(z, saved[z]);
  }

  int strongest = 0;
  for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) strongest = max(strongest, delta_[z]);

  if (strongest < Config::LAMP_CHECK_MIN_DELTA) {
    // Ou todos estão fora, ou esta montagem não tem desvio mensurável: não afirma nada
    noSignal_ = true;
    for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) status_[z] = LampStatus::UNKNOWN;
  } else {
    int needed = max(Config::LAMP_CHECK_MIN_DELTA,
                     static_cast<int>(strongest * Config::LAMP_CHECK_RELATIVE + 0.5f));
    for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) {
      status_[z] = delta_[z] >= needed ? LampStatus::OK : LampStatus::MISSING;
    }
  }

  Serial.printf("[LEDS] verificacao:");
  for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) {
    Serial.printf(" L%u %+d %s", z + 1, delta_[z], statusLabel(status_[z]));
  }
  Serial.println(noSignal_ ? " (nenhum LED mudou o LDR)" : "");
}

uint8_t LampMonitor::missingCount() const {
  uint8_t n = 0;
  for (LampStatus s : status_) n += s == LampStatus::MISSING;
  return n;
}

const char *LampMonitor::statusLabel(LampStatus s) {
  switch (s) {
    case LampStatus::OK: return "OK";
    case LampStatus::MISSING: return "MISSING";
    default: return "UNKNOWN";
  }
}
