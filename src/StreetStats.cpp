#include "StreetStats.h"

#include <Preferences.h>
#include <sys/time.h>
#include <time.h>

#include "AppConfig.h"

namespace {
constexpr const char *NVS_NAMESPACE = "rua";
constexpr const char *NVS_BUCKETS = "horas";
constexpr const char *NVS_TZ = "fuso";
constexpr const char *NVS_SINCE = "desde";
constexpr uint32_t STATS_VERSION = 1;  // muda se o formato do Bucket mudar
constexpr const char *NVS_VERSION = "versao";
// Qualquer hora depois de 2024 é "hora acertada"; antes disso é o relógio zerado do boot
constexpr time_t MIN_VALID_EPOCH = 1704067200;

void appendf(char *out, size_t size, size_t &pos, const char *format, ...) {
  if (pos >= size) return;
  va_list args;
  va_start(args, format);
  int n = vsnprintf(out + pos, size - pos, format, args);
  va_end(args);
  if (n > 0) pos = min(size, pos + static_cast<size_t>(n));
}
}  // namespace

void StreetStats::begin() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return;  // primeira vez: namespace ainda não existe
  if (prefs.getUInt(NVS_VERSION, 0) == STATS_VERSION &&
      prefs.getBytesLength(NVS_BUCKETS) == sizeof(buckets)) {
    prefs.getBytes(NVS_BUCKETS, buckets, sizeof(buckets));
    sinceEpoch = prefs.getUInt(NVS_SINCE, 0);
  }
  tzMinutes = prefs.getShort(NVS_TZ, tzMinutes);
  prefs.end();
}

void StreetStats::save() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  prefs.putUInt(NVS_VERSION, STATS_VERSION);
  prefs.putBytes(NVS_BUCKETS, buckets, sizeof(buckets));
  prefs.putUInt(NVS_SINCE, sinceEpoch);
  prefs.putShort(NVS_TZ, tzMinutes);
  prefs.end();
  dirty = false;
}

void StreetStats::setClock(int64_t epochMs, int16_t tz) {
  timeval tv{static_cast<time_t>(epochMs / 1000), static_cast<suseconds_t>((epochMs % 1000) * 1000)};
  settimeofday(&tv, nullptr);
  if (tz != tzMinutes) {
    tzMinutes = tz;
    dirty = true;
  }
}

void StreetStats::reset() {
  for (Bucket &b : buckets) b = Bucket{};
  sinceEpoch = clockValid() ? static_cast<uint32_t>(time(nullptr)) : 0;
  save();
}

bool StreetStats::clockValid() const { return time(nullptr) >= MIN_VALID_EPOCH; }

int StreetStats::currentHour() const {
  time_t local = time(nullptr) + static_cast<time_t>(tzMinutes) * 60;
  return static_cast<int>((local / 3600) % 24);
}

void StreetStats::update(const SensorReadings &r, bool presence, bool wifiConnected, uint32_t now) {
  // Com o Wi-Fi do roteador, o NTP mantém a hora certa sem depender do painel
  if (wifiConnected && !ntpStarted) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    ntpStarted = true;
  }

  // Passagem = começo de um movimento depois de um intervalo sem ninguém, para uma pessoa
  // parada à frente do sensor não contar várias vezes
  if (presence && !lastPresence &&
      (presenceOffSince == 0 || now - presenceOffSince >= Config::STREET_PASSAGE_GAP_MS) &&
      clockValid()) {
    buckets[currentHour()].passages++;
    dirty = true;
  }
  if (!presence && lastPresence) presenceOffSince = now | 1;
  lastPresence = presence;

  if (lastTickMs != 0 && now - lastTickMs < 1000) return;
  lastTickMs = now | 1;
  if (!clockValid()) return;

  if (sinceEpoch == 0) sinceEpoch = static_cast<uint32_t>(time(nullptr));
  Bucket &b = buckets[currentHour()];
  b.seconds++;
  if (presence) b.motionSeconds++;

  if (r.micLink.state == Link::CONNECTED && r.noiseDbfs.isFresh(now, Config::NOISE_STALE_MS)) {
    float above = fmaxf(0.0f, r.noiseDbfs.value - r.noiseFloorDbfs);
    b.noiseSeconds++;
    b.noiseSumDb += above;
    if (above >= Config::STREET_LOUD_DB) b.loudSeconds++;
    b.noisePeakDb = fmaxf(b.noisePeakDb, above);
  }

  if (r.temperatureC.isFresh(now, Config::DHT_STALE_MS) && r.humidity.isFresh(now, Config::DHT_STALE_MS)) {
    b.climateSeconds++;
    b.temperatureSum += r.temperatureC.value;
    b.humiditySum += r.humidity.value;
  }
  dirty = true;

  if (dirty && (lastSaveMs == 0 || now - lastSaveMs >= Config::STREET_SAVE_INTERVAL_MS)) {
    // Primeira gravação só depois de um intervalo inteiro: reinícios seguidos não gastam a flash
    if (lastSaveMs != 0) save();
    lastSaveMs = now | 1;
  }
}

size_t StreetStats::buildJson(char *out, size_t size) const {
  size_t pos = 0;
  bool valid = clockValid();
  appendf(out, size, pos, "{\"clock_valid\":%s,\"epoch\":", valid ? "true" : "false");
  if (valid) appendf(out, size, pos, "%lu", static_cast<unsigned long>(time(nullptr)));
  else appendf(out, size, pos, "null");
  appendf(out, size, pos, ",\"tz_min\":%d,\"hour\":", tzMinutes);
  if (valid) appendf(out, size, pos, "%d", currentHour());
  else appendf(out, size, pos, "null");
  appendf(out, size, pos, ",\"since\":");
  if (sinceEpoch) appendf(out, size, pos, "%lu", static_cast<unsigned long>(sinceEpoch));
  else appendf(out, size, pos, "null");
  appendf(out, size, pos, ",\"loud_db\":%.0f,\"hours\":[", Config::STREET_LOUD_DB);

  // Médias já calculadas aqui; null quando a hora ainda não tem amostra daquele sensor
  for (uint8_t h = 0; h < HOURS; h++) {
    const Bucket &b = buckets[h];
    appendf(out, size, pos, "%s{\"s\":%lu,\"p\":%lu,\"m\":%lu,", h ? "," : "",
            static_cast<unsigned long>(b.seconds), static_cast<unsigned long>(b.passages),
            static_cast<unsigned long>(b.motionSeconds));
    if (b.noiseSeconds) {
      appendf(out, size, pos, "\"n\":%.1f,\"l\":%.3f,\"pk\":%.1f,", b.noiseSumDb / b.noiseSeconds,
              static_cast<float>(b.loudSeconds) / b.noiseSeconds, b.noisePeakDb);
    } else {
      appendf(out, size, pos, "\"n\":null,\"l\":null,\"pk\":null,");
    }
    if (b.climateSeconds) {
      appendf(out, size, pos, "\"t\":%.1f,\"u\":%.0f}", b.temperatureSum / b.climateSeconds,
              b.humiditySum / b.climateSeconds);
    } else {
      appendf(out, size, pos, "\"t\":null,\"u\":null}");
    }
  }
  appendf(out, size, pos, "]}");
  return pos;
}
