#include "SensorValidator.h"

#include "AppConfig.h"

bool SensorValidator::testable(Component c) {
  switch (c) {
    case Component::LDR_AMBIENT:
    case Component::LDR_LOCAL:
    case Component::ULTRASONIC:
    case Component::MIC:
    case Component::DHT11:
      return true;
    case Component::IR:
      return Config::IR_INSTALLED;
    case Component::MQ135:
      return Config::MQ135_INSTALLED;
    default:
      return false;
  }
}

bool SensorValidator::confirmable(Component c) {
  return c == Component::LEDS || c == Component::STATUS_LED;
}

uint32_t SensorValidator::windowMs(Component c) {
  return (c == Component::DHT11 || c == Component::MQ135) ? Config::TEST_WINDOW_SLOW_MS
                                                          : Config::TEST_WINDOW_MS;
}

const char *SensorValidator::stateCode(State s) {
  switch (s) {
    case State::RUNNING: return "RUNNING";
    case State::PASSED: return "PASSED";
    case State::FAILED: return "FAILED";
    default: return "IDLE";
  }
}

bool SensorValidator::start(Component c, const SensorReadings &r, uint32_t now) {
  if (!testable(c) || active >= 0) return false;

  Result &res = results[componentIndex(c)];
  res = Result();
  res.state = State::RUNNING;
  res.startedMs = now;

  switch (c) {
    case Component::LDR_AMBIENT:
    case Component::LDR_LOCAL: {
      const Reading<int> &ldr = c == Component::LDR_AMBIENT ? r.ambientAdc : r.localAdc;
      res.baseline = ldr.value;
      res.target = fmax(Config::LDR_TEST_MIN_DELTA, ldr.value * Config::LDR_TEST_RELATIVE);
      break;
    }
    case Component::ULTRASONIC:
      res.target = Config::ULTRASONIC_TEST_MAX_CM;
      res.best = 9999;  // menor distância vista
      break;
    case Component::IR:
      baselineIrChanges = r.irChanges;
      res.target = 1;
      break;
    case Component::MIC:
      res.baseline = r.noiseFloorDbfs;
      res.target = Config::SOUND_TEST_MIN_RISE_DB;
      baselineSoundEvents = r.soundDoEvents;
      break;
    case Component::DHT11:
      res.baseline = r.temperatureC.value;
      baselineHumidity = r.humidity.value;
      res.target = Config::DHT_TEST_HUMIDITY_RISE;
      break;
    case Component::MQ135:
      res.baseline = r.mq135Adc.value;
      res.target = fmax(Config::MQ135_TEST_MIN_DELTA, r.mq135Adc.value * Config::MQ135_TEST_RELATIVE);
      break;
    default:
      break;
  }

  active = static_cast<int8_t>(componentIndex(c));
  Serial.printf("[TESTE] %s iniciado\n", componentName(c));
  return true;
}

void SensorValidator::confirm(Component c, uint32_t now) {
  if (!confirmable(c)) return;
  Result &res = results[componentIndex(c)];
  res.state = State::PASSED;
  res.startedMs = now;
  res.finishedMs = now;
  Serial.printf("[TESTE] %s confirmado pelo usuario\n", componentName(c));
}

void SensorValidator::finish(Component c, bool ok, uint32_t now) {
  Result &res = results[componentIndex(c)];
  res.state = ok ? State::PASSED : State::FAILED;
  res.finishedMs = now;
  active = -1;
  Serial.printf("[TESTE] %s: %s\n", componentName(c), ok ? "PASSOU" : "sem reacao no tempo limite");
}

void SensorValidator::update(const SensorReadings &r, uint32_t now) {
  if (active < 0) return;
  Component c = static_cast<Component>(active);
  Result &res = results[active];
  bool ok = false;

  switch (c) {
    case Component::LDR_AMBIENT:
    case Component::LDR_LOCAL: {
      // Pino solto oscila sozinho: só vale com o sensor detectado como ligado
      const Reading<int> &ldr = c == Component::LDR_AMBIENT ? r.ambientAdc : r.localAdc;
      const LinkStatus &link = c == Component::LDR_AMBIENT ? r.ambientLink : r.localLink;
      if (link.state == Link::CONNECTED) {
        res.best = fmax(res.best, fabsf(ldr.value - res.baseline));
        ok = res.best >= res.target;
      }
      break;
    }
    case Component::ULTRASONIC:
      if (r.distanceCm.isFresh(now, Config::ULTRASONIC_INTERVAL_MS * 3) &&
          r.distanceCm.value >= 3.0f) {
        res.best = fmin(res.best, r.distanceCm.value);
        ok = res.best <= res.target;
      }
      break;
    case Component::IR:
      res.best = r.irChanges - baselineIrChanges;
      ok = res.best >= res.target;
      break;
    case Component::MIC:
      if (r.micLink.state == Link::CONNECTED) {
        // Passa com pico acima do silêncio ou com disparo do comparador (DO)
        res.best = fmax(res.best, r.noisePeakDbfs - res.baseline);
        ok = res.best >= res.target || r.soundDoEvents != baselineSoundEvents;
      }
      break;
    case Component::DHT11:
      if (r.temperatureC.isFresh(now, Config::DHT_STALE_MS) && res.baseline != 0) {
        float humidityRise = r.humidity.value - baselineHumidity;
        float temperatureRise = r.temperatureC.value - res.baseline;
        res.best = fmax(res.best, humidityRise);
        ok = humidityRise >= Config::DHT_TEST_HUMIDITY_RISE ||
             temperatureRise >= Config::DHT_TEST_TEMPERATURE_RISE;
      }
      break;
    case Component::MQ135:
      if (r.mq135Adc.valid) {
        res.best = fmax(res.best, fabsf(r.mq135Adc.value - res.baseline));
        ok = res.best >= res.target;
      }
      break;
    default:
      break;
  }

  if (ok) {
    finish(c, true, now);
  } else if (now - res.startedMs >= windowMs(c)) {
    finish(c, false, now);
  }
}

uint32_t SensorValidator::remainingMs(Component c, uint32_t now) const {
  const Result &res = result(c);
  if (res.state != State::RUNNING) return 0;
  uint32_t elapsed = now - res.startedMs;
  return elapsed < windowMs(c) ? windowMs(c) - elapsed : 0;
}
