#include "Telemetry.h"

#include <stdarg.h>

#include "AppConfig.h"

namespace {

// Escreve JSON num buffer fixo, sem alocação dinâmica
class JsonWriter {
 public:
  JsonWriter(char *buffer, size_t size) : out(buffer), cap(size) { out[0] = '\0'; }

  void raw(const char *format, ...) {
    if (len >= cap) return;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(out + len, cap - len, format, args);
    va_end(args);
    if (written > 0) len = min(cap - 1, len + static_cast<size_t>(written));
  }

  // Texto entre aspas, escapando aspas e barras
  void str(const char *text) {
    raw("\"");
    for (const char *p = text; *p; p++) {
      if (*p == '"' || *p == '\\') raw("\\%c", *p);
      else if (static_cast<uint8_t>(*p) >= 0x20) raw("%c", *p);
    }
    raw("\"");
  }

  void key(const char *name) {
    comma();
    str(name);
    raw(":");
    needsComma = false;
  }
  void open(char bracket) {
    comma();
    raw("%c", bracket);
    needsComma = false;
  }
  void close(char bracket) {
    raw("%c", bracket);
    needsComma = true;
  }
  void value(const char *format, ...) {
    comma();
    if (len < cap) {
      va_list args;
      va_start(args, format);
      int written = vsnprintf(out + len, cap - len, format, args);
      va_end(args);
      if (written > 0) len = min(cap - 1, len + static_cast<size_t>(written));
    }
    needsComma = true;
  }
  void text(const char *t) {
    comma();
    str(t);
    needsComma = true;
  }
  void boolean(bool b) { value(b ? "true" : "false"); }
  void null() { value("null"); }

  template <typename T>
  void number(const Reading<T> &r, bool usable, const char *format) {
    if (usable) value(format, r.value);
    else null();
  }

  size_t length() const { return len; }

 private:
  void comma() {
    if (needsComma) raw(",");
  }

  char *out;
  size_t cap;
  size_t len = 0;
  bool needsComma = false;
};


void writeLink(JsonWriter &j, const LinkStatus &link) {
  j.key("link");
  j.open('{');
  j.key("state");
  j.text(linkCode(link.state));
  j.key("reason");
  j.text(linkReasonCode(link.reason));
  j.close('}');
}

void writeTest(JsonWriter &j, const AppSnapshot &s, Component c) {
  const SensorValidator::Result &res = s.validator.result(c);
  j.key("test");
  j.open('{');
  j.key("available");
  j.boolean(SensorValidator::testable(c));
  j.key("confirmable");
  j.boolean(SensorValidator::confirmable(c));
  j.key("state");
  j.text(SensorValidator::stateCode(res.state));
  j.key("window_ms");
  j.value("%lu", SensorValidator::windowMs(c));
  j.key("remaining_ms");
  j.value("%lu", s.validator.remainingMs(c, s.now));
  j.key("baseline");
  j.value("%.1f", res.baseline);
  j.key("best");
  j.value("%.1f", res.best);
  j.key("target");
  j.value("%.1f", res.target);
  j.key("finished_ago_ms");
  if (res.finishedMs != 0) j.value("%lu", s.now - res.finishedMs);
  else j.null();
  j.close('}');
}

void writeLdrData(JsonWriter &j, const Reading<int> &r, float noise) {
  j.key("value");
  j.number(r, r.valid, "%d");
  j.key("raw");
  j.value("%d", r.value);
  j.key("noise");
  j.value("%.1f", noise);
}

// Métricas específicas de cada componente
void writeComponentData(JsonWriter &j, const AppSnapshot &s, Component c) {
  const SensorReadings &r = s.readings;
  j.key("data");
  j.open('{');
  switch (c) {
    case Component::LDR_AMBIENT: writeLdrData(j, r.ambientAdc, r.ambientNoise); break;
    case Component::LDR_LOCAL: writeLdrData(j, r.localAdc, r.localNoise); break;
    case Component::ULTRASONIC:
      j.key("distance_cm");
      j.number(r.distanceCm, r.distanceCm.valid, "%.1f");
      j.key("last");
      j.text(echoResultCode(r.lastEcho));
      j.key("echo_pct");
      j.value("%u", r.echoValidPercent);
      j.key("response_pct");
      j.value("%u", r.echoResponsePercent);
      j.key("in_range");
      j.boolean(s.presenceHc);
      break;
    case Component::IR:
      j.key("detected");
      j.boolean(r.irDetected.value);
      j.key("changes");
      j.value("%u", r.irChanges);
      j.key("last_change_ago_ms");
      if (r.irLastChangeMs != 0) j.value("%lu", s.now - r.irLastChangeMs);
      else j.null();
      j.key("active_low");
      j.boolean(Config::IR_ACTIVE_LOW);
      break;
    case Component::DHT11: {
      bool fresh = r.temperatureC.isFresh(s.now, Config::DHT_STALE_MS);
      j.key("temperature_c");
      j.number(r.temperatureC, fresh, "%.1f");
      j.key("humidity_percent");
      j.number(r.humidity, fresh, "%.1f");
      j.key("heat_index_c");
      j.number(r.heatIndexC, fresh, "%.1f");
      j.key("success_pct");
      j.value("%u", r.dhtSuccessPercent);
      j.key("age_ms");
      if (r.temperatureC.valid) j.value("%lu", s.now - r.temperatureC.timestamp);
      else j.null();
      break;
    }
    case Component::MQ135:
      j.key("adc");
      j.number(r.mq135Adc, r.mq135Adc.valid, "%d");
      j.key("raw");
      j.value("%d", r.mq135Adc.value);
      j.key("warming");
      j.boolean(s.mq135WarmingUp);
      j.key("warmup_left_ms");
      j.value("%lu", s.mq135WarmingUp ? Config::MQ135_WARMUP_MS - s.now : 0);
      j.key("level");
      if (r.mq135Adc.valid && !s.mq135WarmingUp) {
        j.text(airLevelTelemetryLabel(classifyAir(r.mq135Adc.value)));
      } else {
        j.null();
      }
      break;
    case Component::MIC: {
      bool fresh = r.noiseRms.isFresh(s.now, Config::NOISE_STALE_MS);
      j.key("rms");
      j.number(r.noiseRms, fresh, "%.1f");
      j.key("dbfs");
      j.number(r.noiseDbfs, fresh, "%.1f");
      j.key("peak_dbfs");
      if (fresh) j.value("%.1f", r.noisePeakDbfs);
      else j.null();
      j.key("floor_dbfs");
      if (fresh) j.value("%.1f", r.noiseFloorDbfs);
      else j.null();
      j.key("bias");
      j.value("%d", r.soundBias);
      j.key("do_events");
      j.value("%u", r.soundDoEvents);
      j.key("do_active");
      j.boolean(r.soundDoActive);
      break;
    }
    case Component::LEDS:
      j.key("zones");
      j.open('[');
      for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) {
        j.value("%u", s.lighting.zonePercent(zone));
      }
      j.close(']');
      j.key("lamp1_optical_delta");
      if (s.readings.localLink.state == Link::CONNECTED) j.value("%d", s.diag.lamp1Delta());
      else j.null();
      j.key("lamp1_optical_min");
      j.value("%d", Config::LAMP1_OPTICAL_MIN_DELTA);
      break;
    default:
      break;
  }
  j.close('}');
}

const LinkStatus *componentLink(const SensorReadings &r, Component c) {
  switch (c) {
    case Component::LDR_AMBIENT: return &r.ambientLink;
    case Component::LDR_LOCAL: return &r.localLink;
    case Component::ULTRASONIC: return &r.ultrasonicLink;
    case Component::IR: return &r.irLink;
    case Component::DHT11: return &r.dhtLink;
    case Component::MQ135: return &r.mq135Link;
    case Component::MIC: return &r.micLink;
    default: return nullptr;
  }
}

void writeHealth(JsonWriter &j, const Diagnostics &diag, Component c) {
  j.key("code");
  j.text(componentCode(c));
  j.key("name");
  j.text(componentName(c));
  j.key("health");
  j.text(Diagnostics::healthCode(diag.health(c)));
  j.key("label");
  j.text(Diagnostics::healthSerialLabel(diag.health(c)));
  j.key("note");
  j.text(diag.note(c));
  j.key("alert");
  j.boolean(diag.isAlert(c));
}

// Campos de leitura da telemetria MQTT, com null quando indisponível
void writeSensorFields(JsonWriter &j, const AppSnapshot &s) {
  const SensorReadings &r = s.readings;
  bool dhtFresh = r.temperatureC.isFresh(s.now, Config::DHT_STALE_MS);
  bool noiseFresh = r.noiseRms.isFresh(s.now, Config::NOISE_STALE_MS);

  j.key("ambient_adc");
  j.number(r.ambientAdc, r.ambientAdc.valid, "%d");
  j.key("local_ldr_adc");
  j.number(r.localAdc, r.localAdc.valid, "%d");
  j.key("distance_cm");
  j.number(r.distanceCm, r.distanceCm.valid, "%.1f");
  j.key("ir_detected");
  j.boolean(r.irDetected.value);
  j.key("temperature_c");
  j.number(r.temperatureC, dhtFresh, "%.1f");
  j.key("humidity_percent");
  j.number(r.humidity, dhtFresh, "%.1f");
  j.key("mq135_adc");
  j.number(r.mq135Adc, r.mq135Adc.valid, "%d");
  j.key("noise_rms");
  j.number(r.noiseRms, noiseFresh, "%.1f");
}

}  // namespace

const char *lightingReason(const AppSnapshot &s) {
  switch (s.lighting.mode()) {
    case LightingMode::DAY_OFF: return "Ambiente claro: luminarias apagadas";
    case LightingMode::ECONOMY: return "Noite sem presenca: nivel reduzido para economizar";
    case LightingMode::ACTIVE:
      return (s.presenceHc || s.presenceIr) ? "Presenca detectada: intensidade maxima"
                                            : "Presenca recente: mantendo intensidade maxima";
    default:
      if (!s.readings.ambientAdc.valid) {
        return "LDR ambiente indisponivel: modo seguro (nunca assume dia)";
      }
      return "Noite sem sensor de presenca confirmado: modo seguro";
  }
}

size_t buildTelemetryJson(const AppSnapshot &s, char *out, size_t size) {
  JsonWriter j(out, size);
  j.open('{');
  j.key("uptime_ms");
  j.value("%lu", s.now);
  writeSensorFields(j, s);
  j.key("is_night");
  if (s.readings.ambientAdc.valid) j.boolean(s.lighting.isNight());
  else j.null();
  j.key("presence");
  j.boolean(s.presenceHc || s.presenceIr);
  j.key("lighting_percent");
  j.value("%u", s.lighting.zonePercent(0));
  j.key("mq135_level");
  if (s.readings.mq135Adc.valid && !s.mq135WarmingUp) {
    j.text(airLevelTelemetryLabel(classifyAir(s.readings.mq135Adc.value)));
  } else {
    j.null();
  }
  j.key("noise_dbfs");
  j.number(s.readings.noiseDbfs, s.readings.noiseDbfs.isFresh(s.now, Config::NOISE_STALE_MS),
           "%.1f");
  j.key("mode");
  j.text(LightingController::modeTelemetryLabel(s.lighting.mode()));
  j.close('}');
  return j.length();
}

size_t buildStatusJson(const AppSnapshot &s, char *out, size_t size) {
  JsonWriter j(out, size);
  j.open('{');
  j.key("uptime_ms");
  j.value("%lu", s.now);
  j.key("summary");
  j.text(Diagnostics::summaryCode(s.diag.summary()));
  j.key("components");
  j.open('[');
  for (uint8_t i = 0; i < COMPONENT_COUNT; i++) {
    Component c = static_cast<Component>(i);
    j.open('{');
    writeHealth(j, s.diag, c);
    const LinkStatus *link = componentLink(s.readings, c);
    if (link) writeLink(j, *link);
    j.close('}');
  }
  j.close(']');
  j.close('}');
  return j.length();
}

size_t buildDashboardJson(const AppSnapshot &s, char *out, size_t size) {
  const LightingController &l = s.lighting;
  JsonWriter j(out, size);

  j.open('{');
  j.key("device");
  j.text(DEVICE_ID);
  j.key("uptime_ms");
  j.value("%lu", s.now);
  j.key("summary");
  j.text(Diagnostics::summaryCode(s.diag.summary()));

  j.key("network");
  j.open('{');
  j.key("wifi_configured");
  j.boolean(s.net.wifiConfigured());
  j.key("wifi");
  j.boolean(s.net.wifiConnected());
  j.key("mqtt_configured");
  j.boolean(s.net.mqttConfigured());
  j.key("mqtt");
  j.boolean(s.net.mqttConnected());
  j.close('}');

  j.key("lighting");
  j.open('{');
  j.key("mode");
  j.text(LightingController::modeTelemetryLabel(l.mode()));
  j.key("is_night");
  if (s.readings.ambientAdc.valid) j.boolean(l.isNight());
  else j.null();
  j.key("target_percent");
  j.value("%u", l.targetPercent());
  j.key("zones");
  j.open('[');
  for (uint8_t zone = 0; zone < Pins::LAMP_COUNT; zone++) j.value("%u", l.zonePercent(zone));
  j.close(']');
  j.key("presence_hc");
  j.boolean(s.presenceHc);
  j.key("presence_ir");
  j.boolean(s.presenceIr);
  j.key("presence_available");
  j.boolean(s.diag.presenceAvailable());
  j.key("hc_usable");
  j.boolean(s.diag.ultrasonicUsable());
  j.key("ir_usable");
  j.boolean(s.diag.irUsable());
  j.key("hold_remaining_ms");
  j.value("%lu", l.holdRemainingMs(s.now));
  j.key("reason");
  j.text(lightingReason(s));
  j.close('}');

  j.key("config");
  j.open('{');
  j.key("day_threshold");
  j.value("%d", Config::DAY_THRESHOLD);
  j.key("night_threshold");
  j.value("%d", Config::NIGHT_THRESHOLD);
  j.key("standby_percent");
  j.value("%u", Config::LIGHT_STANDBY_PERCENT);
  j.key("active_percent");
  j.value("%u", Config::LIGHT_ACTIVE_PERCENT);
  j.key("hold_ms");
  j.value("%lu", Config::PRESENCE_HOLD_MS);
  j.key("min_distance_cm");
  j.value("%.0f", Config::MIN_DISTANCE_CM);
  j.key("max_distance_cm");
  j.value("%.0f", Config::MAX_DISTANCE_CM);
  j.key("mq135_medium");
  j.value("%d", Config::MQ135_LEVEL_MEDIUM);
  j.key("mq135_high");
  j.value("%d", Config::MQ135_LEVEL_HIGH);
  j.close('}');

  j.key("components");
  j.open('[');
  for (uint8_t i = 0; i < COMPONENT_COUNT; i++) {
    Component c = static_cast<Component>(i);
    j.open('{');
    writeHealth(j, s.diag, c);
    const LinkStatus *link = componentLink(s.readings, c);
    if (link) writeLink(j, *link);
    writeComponentData(j, s, c);
    writeTest(j, s, c);
    j.close('}');
  }
  j.close(']');
  j.close('}');
  return j.length();
}
