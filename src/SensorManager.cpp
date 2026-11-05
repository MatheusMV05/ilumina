#include "SensorManager.h"

#include <driver/adc.h>
#include <driver/rtc_io.h>
#include <math.h>

#include "AppConfig.h"

namespace {
constexpr float SOUND_SPEED_CM_PER_US = 0.0343f;
constexpr float ADC_FULL_SCALE = 4095.0f;
constexpr uint32_t ADC_SAMPLE_SPACING_US = 200;  // amostras espaçadas revelam pino solto
constexpr uint32_t PULL_SETTLE_US = 300;

// Disparos do comparador (DO) do KY-038, contados por interrupção
volatile uint16_t soundDoEdges = 0;
void IRAM_ATTR onSoundDo() { soundDoEdges++; }

// Nível em dB relativo ao fundo de escala do ADC (não é dB SPL)
float toDbfs(float amplitude) {
  return 20.0f * log10f(fmax(amplitude, 0.5f) / ADC_FULL_SCALE);
}

LinkStatus link(Link state, LinkReason reason = LinkReason::NONE) {
  LinkStatus s;
  s.state = state;
  s.reason = reason;
  return s;
}
}  // namespace

// ---------- Inicialização ----------

void SensorManager::begin() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);  // faixa de ~0 a 3,1 V

  pinMode(Pins::ULTRASONIC_TRIG, OUTPUT);
  digitalWrite(Pins::ULTRASONIC_TRIG, LOW);
  pinMode(Pins::ULTRASONIC_ECHO, INPUT);
  if (Config::IR_INSTALLED) pinMode(Pins::IR_OUT, INPUT_PULLUP);

  pinMode(Pins::SOUND_AO, INPUT);
  pinMode(Pins::SOUND_DO, INPUT);
  attachInterrupt(digitalPinToInterrupt(Pins::SOUND_DO), onSoundDo,
                  Config::SOUND_DO_ACTIVE_HIGH ? RISING : FALLING);

  dht.begin();
}

void SensorManager::update(uint32_t now) {
  runProbes(now);
  updateLdrs(now);
  updateUltrasonic(now);
  updateIr(now);
  updateDht(now);
  updateMq135(now);
  updateNoise(now);
}

// ---------- Testes de conexão ----------

// Lê o pino com pull-up e com pull-down internos. Se o nível acompanha o pull,
// nada está segurando a linha: fio solto ou módulo ausente.
bool SensorManager::digitalPinFloats(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(PULL_SETTLE_US);
  bool withPullUp = digitalRead(pin);
  pinMode(pin, INPUT_PULLDOWN);
  delayMicroseconds(PULL_SETTLE_US);
  bool withPullDown = digitalRead(pin);
  return withPullUp && !withPullDown;
}

void SensorManager::runProbes(uint32_t now) {
  if (lastProbeMs != 0 && now - lastProbeMs < Config::PROBE_INTERVAL_MS) return;
  lastProbeMs = now | 1;

  // IR: módulo ligado mantém a saída em um nível definido
  if (Config::IR_INSTALLED) {
    bool irFloats = digitalPinFloats(Pins::IR_OUT);
    pinMode(Pins::IR_OUT, INPUT_PULLUP);
    data.irLink = irFloats ? link(Link::NOT_CONNECTED, LinkReason::FLOATING) : link(Link::CONNECTED);
  }

  // ECHO: com o divisor montado, o resistor até o GND segura a linha.
  // Roda entre medições; um ECHO em HIGH de verdade não acompanha o pull-down.
  echoFloating = digitalPinFloats(Pins::ULTRASONIC_ECHO);
  pinMode(Pins::ULTRASONIC_ECHO, INPUT);

  // Pinos de ADC com pull interno: pino solto vai de 0 a 4095 com o pull
  if (Config::MQ135_INSTALLED) mqProbeFloating = adcPinFloats(Pins::MQ135_AO);
  ambientProbeFloating = adcPinFloats(Pins::LDR_AMBIENT);
  localProbeFloating = adcPinFloats(Pins::LDR_LOCAL);
}

// GPIOs 34 a 39 não têm pull interno: nesses só o ruído da leitura indica pino solto
bool SensorManager::adcHasPulls(uint8_t pin) {
  return pin < 34;
}

// Liga pull-up e depois pull-down. Um divisor de 10 kΩ segura a tensão;
// um pino solto acompanha o pull de ponta a ponta.
bool SensorManager::adcPinFloats(uint8_t pin) {
  if (!adcHasPulls(pin)) return false;
  adc1_channel_t channel = static_cast<adc1_channel_t>(digitalPinToAnalogChannel(pin));
  gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  rtc_gpio_pulldown_dis(gpio);
  rtc_gpio_pullup_en(gpio);
  delayMicroseconds(PULL_SETTLE_US);
  int withPullUp = adc1_get_raw(channel);
  rtc_gpio_pullup_dis(gpio);
  rtc_gpio_pulldown_en(gpio);
  delayMicroseconds(PULL_SETTLE_US);
  int withPullDown = adc1_get_raw(channel);
  rtc_gpio_pulldown_dis(gpio);
  return withPullUp - withPullDown > 3000;
}

// ---------- LDRs ----------

void SensorManager::readLdr(uint8_t pin, Reading<int> &reading, float &noise, LinkStatus &status,
                            uint32_t &badSince, bool probeFloating, bool &everLit,
                            uint32_t now) {
  double sum = 0, sumSquares = 0;
  for (uint8_t i = 0; i < Config::LDR_SAMPLES; i++) {
    int v = analogRead(pin);
    sum += v;
    sumSquares += static_cast<double>(v) * v;
    delayMicroseconds(ADC_SAMPLE_SPACING_US);
  }
  float mean = sum / Config::LDR_SAMPLES;
  float stddev = sqrt(fmax(0.0, sumSquares / Config::LDR_SAMPLES - mean * mean));
  noise = noise == 0 ? stddev : noise + Config::ADC_NOISE_SMOOTHING * (stddev - noise);

  int adc = static_cast<int>(mean + 0.5f);
  // Com pull interno o teste de pull é conclusivo; sem ele, usa o ruído
  bool floating = adcHasPulls(pin) ? probeFloating : noise > Config::ADC_FLOATING_STDDEV;
  if (floating) {
    status = link(Link::NOT_CONNECTED, LinkReason::FLOATING);
  } else if (adc < Config::LDR_MIN_PLAUSIBLE && !everLit) {
    // Nunca mediu luz desde o boot: o pino só enxerga o resistor até o GND.
    // Depois de já ter medido luz, leitura baixa significa apenas escuro.
    status = link(Link::SUSPECT, LinkReason::STUCK_LOW);
  } else if (adc > Config::LDR_MAX_PLAUSIBLE) {
    status = link(Link::SUSPECT, LinkReason::STUCK_HIGH);
  } else {
    status = link(Link::CONNECTED);
    if (adc >= Config::LDR_MIN_PLAUSIBLE) everLit = true;
  }

  if (status.state == Link::CONNECTED) {
    badSince = 0;
    reading.valid = true;
  } else {
    if (badSince == 0) badSince = now | 1;  // 0 é reservado para "sem problema"
    // Sem leitura válida anterior, ou problema persistente: indisponível
    if (!reading.valid || now - badSince >= Config::LDR_FAULT_CONFIRM_MS) reading.valid = false;
  }
  reading.value = Config::LDR_INVERTED ? 4095 - adc : adc;
  reading.timestamp = now;
}

int SensorManager::sampleLocalLdr() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < Config::LDR_SAMPLES; i++) {
    sum += analogRead(Pins::LDR_LOCAL);
    delayMicroseconds(ADC_SAMPLE_SPACING_US);
  }
  int adc = sum / Config::LDR_SAMPLES;
  return Config::LDR_INVERTED ? 4095 - adc : adc;
}

void SensorManager::updateLdrs(uint32_t now) {
  if (lastLdrMs != 0 && now - lastLdrMs < Config::LDR_INTERVAL_MS) return;
  lastLdrMs = now | 1;
  readLdr(Pins::LDR_AMBIENT, data.ambientAdc, data.ambientNoise, data.ambientLink, ambientBadSince,
          ambientProbeFloating, ambientEverLit, now);
  readLdr(Pins::LDR_LOCAL, data.localAdc, data.localNoise, data.localLink, localBadSince,
          localProbeFloating, localEverLit, now);
}

// ---------- HC-SR04 ----------

EchoResult SensorManager::measureDistance(float &distanceCm) {
  if (digitalRead(Pins::ULTRASONIC_ECHO) == HIGH) return EchoResult::BUSY;

  digitalWrite(Pins::ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(Pins::ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(Pins::ULTRASONIC_TRIG, LOW);

  // O ECHO sobe ao fim da rajada ultrassônica, com ou sem objeto à frente
  uint32_t start = micros();
  while (digitalRead(Pins::ULTRASONIC_ECHO) == LOW) {
    if (micros() - start > Config::ECHO_RISE_TIMEOUT_US) return EchoResult::NO_RESPONSE;
  }
  uint32_t rise = micros();
  while (digitalRead(Pins::ULTRASONIC_ECHO) == HIGH) {
    if (micros() - rise > Config::ULTRASONIC_TIMEOUT_US) return EchoResult::NO_ECHO;
  }
  distanceCm = (micros() - rise) * SOUND_SPEED_CM_PER_US / 2.0f;
  return EchoResult::VALID;
}

void SensorManager::updateUltrasonic(uint32_t now) {
  if (lastUltrasonicMs != 0 && now - lastUltrasonicMs < Config::ULTRASONIC_INTERVAL_MS) return;
  lastUltrasonicMs = now | 1;

  float distance = 0;
  EchoResult result = measureDistance(distance);
  data.lastEcho = result;

  if (result == EchoResult::BUSY) {
    if (echoHighSince == 0) echoHighSince = now | 1;
  } else {
    echoHighSince = 0;
    echoHistory[echoHistoryPos] = result;
    echoHistoryPos = (echoHistoryPos + 1) % Config::ULTRASONIC_HISTORY;
    if (data.echoSamples < Config::ULTRASONIC_HISTORY) data.echoSamples++;

    uint8_t valid = 0, responded = 0;
    for (uint8_t i = 0; i < data.echoSamples; i++) {
      if (echoHistory[i] == EchoResult::VALID) valid++;
      if (echoHistory[i] != EchoResult::NO_RESPONSE) responded++;
    }
    data.echoValidPercent = valid * 100 / data.echoSamples;
    data.echoResponsePercent = responded * 100 / data.echoSamples;
  }

  if (result == EchoResult::VALID) {
    data.distanceCm.set(distance, now);
    data.ultrasonicEverEchoed = true;
    bool inRange = distance >= Config::MIN_DISTANCE_CM && distance <= Config::MAX_DISTANCE_CM;
    inRangeCount = inRange ? min<uint8_t>(inRangeCount + 1, 255) : 0;
  } else if (result != EchoResult::BUSY) {
    // Sem eco: só indica que não houve medição válida neste ciclo
    data.distanceCm.valid = false;
    data.distanceCm.timestamp = now;
    inRangeCount = 0;
  }
  data.ultrasonicPresence = inRangeCount >= Config::ULTRASONIC_CONFIRM_READINGS;

  bool stuck = echoHighSince != 0 && now - echoHighSince > Config::ULTRASONIC_STUCK_MS;
  if (echoFloating) {
    data.ultrasonicLink = link(Link::NOT_CONNECTED, LinkReason::FLOATING);
  } else if (stuck) {
    data.ultrasonicLink = link(Link::SUSPECT, LinkReason::ECHO_STUCK);
  } else if (data.echoSamples < 5) {
    data.ultrasonicLink = link(Link::UNKNOWN);
  } else if (data.echoResponsePercent == 0) {
    data.ultrasonicLink = link(Link::NOT_CONNECTED, LinkReason::NO_RESPONSE);
  } else if (data.echoResponsePercent < 50) {
    data.ultrasonicLink = link(Link::SUSPECT, LinkReason::INTERMITTENT);
  } else {
    data.ultrasonicLink = link(Link::CONNECTED);
  }
}

// ---------- Sensor IR ----------

void SensorManager::updateIr(uint32_t now) {
  if (!Config::IR_INSTALLED) return;
  bool raw = digitalRead(Pins::IR_OUT) == HIGH;

  if (!irInitialized) {
    irInitialized = true;
    irLastRaw = raw;
    irStableRaw = raw;
    irChangedMs = now;
  }
  if (raw != irLastRaw) {
    irLastRaw = raw;
    irChangedMs = now;
  }
  // Mudanças só contam com o módulo ligado: pino solto oscila sozinho
  if (raw != irStableRaw && now - irChangedMs >= Config::IR_DEBOUNCE_MS) {
    irStableRaw = raw;
    if (data.irLink.state == Link::CONNECTED) {
      data.irChanges++;
      data.irLastChangeMs = now | 1;
    }
  }

  bool detected = Config::IR_ACTIVE_LOW ? !irStableRaw : irStableRaw;
  data.irDetected.set(detected && data.irLink.state == Link::CONNECTED, now);
}

// ---------- DHT11 ----------

void SensorManager::updateDht(uint32_t now) {
  if (lastDhtMs != 0 && now - lastDhtMs < Config::DHT_INTERVAL_MS) return;
  lastDhtMs = now | 1;

  // force=true ignora o cache da biblioteca; o intervalo mínimo é respeitado acima
  float t = dht.readTemperature(false, true);
  float h = dht.readHumidity(false);
  bool ok = !isnan(t) && !isnan(h) && t >= -40.0f && t <= 80.0f && h >= 0.0f && h <= 100.0f;

  if (ok) {
    data.temperatureC.set(t, now);
    data.humidity.set(h, now);
    data.heatIndexC.set(dht.computeHeatIndex(t, h, false), now);
    data.dhtFailures = 0;
    dhtEverOk = true;
  } else if (data.dhtFailures < 255) {
    data.dhtFailures++;
  }

  dhtHistory[dhtHistoryPos] = ok;
  dhtHistoryPos = (dhtHistoryPos + 1) % Config::DHT_HISTORY;
  if (data.dhtSamples < Config::DHT_HISTORY) data.dhtSamples++;
  uint8_t successes = 0;
  for (uint8_t i = 0; i < data.dhtSamples; i++) successes += dhtHistory[i];
  data.dhtSuccessPercent = successes * 100 / data.dhtSamples;

  if (data.dhtFailures >= 2 && !dhtEverOk) {
    data.dhtLink = link(Link::NOT_CONNECTED, LinkReason::NO_RESPONSE);
  } else if (data.dhtFailures >= 3) {
    data.dhtLink = link(Link::SUSPECT, LinkReason::NO_RESPONSE);  // parou de responder
  } else if (dhtEverOk && data.dhtSuccessPercent < 70) {
    data.dhtLink = link(Link::SUSPECT, LinkReason::INTERMITTENT);
  } else if (dhtEverOk) {
    data.dhtLink = link(Link::CONNECTED);
  }
}

// ---------- MQ-135 ----------

void SensorManager::updateMq135(uint32_t now) {
  if (!Config::MQ135_INSTALLED) return;
  if (lastMq135Ms != 0 && now - lastMq135Ms < Config::MQ135_INTERVAL_MS) return;
  lastMq135Ms = now | 1;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < Config::MQ135_SAMPLES; i++) {
    sum += analogRead(Pins::MQ135_AO);
    delayMicroseconds(ADC_SAMPLE_SPACING_US);
  }
  int adc = sum / Config::MQ135_SAMPLES;

  data.mq135Adc.value = adc;
  data.mq135Adc.timestamp = now;

  // Pino solto: segue o pull interno (runProbes)
  if (mqProbeFloating) {
    data.mq135Link = link(Link::NOT_CONNECTED, LinkReason::FLOATING);
    data.mq135Adc.valid = false;
    return;
  }
  if (adc < Config::MQ135_MIN_PLAUSIBLE) {
    data.mq135Link = link(Link::SUSPECT, LinkReason::STUCK_LOW);
  } else if (adc > Config::MQ135_MAX_PLAUSIBLE) {
    data.mq135Link = link(Link::SUSPECT, LinkReason::STUCK_HIGH);
  } else {
    data.mq135Link = link(Link::CONNECTED);
  }
  data.mq135Adc.valid = data.mq135Link.state == Link::CONNECTED;
}

// ---------- KY-038 (som) ----------

// Amostra a saída AO por ~12 ms. O nível DC é a polarização do microfone;
// o som é a variação em torno dela. Nenhuma amostra é guardada.
void SensorManager::updateNoise(uint32_t now) {
  if (lastNoiseMs != 0 && now - lastNoiseMs < Config::NOISE_INTERVAL_MS) return;
  uint32_t elapsed = lastNoiseMs == 0 ? 0 : now - lastNoiseMs;
  lastNoiseMs = now | 1;

  double sum = 0, sumSquares = 0;
  int lowest = 4095, highest = 0;
  for (uint16_t i = 0; i < Config::SOUND_SAMPLES; i++) {
    int v = analogRead(Pins::SOUND_AO);
    sum += v;
    sumSquares += static_cast<double>(v) * v;
    lowest = min(lowest, v);
    highest = max(highest, v);
    delayMicroseconds(Config::SOUND_SAMPLE_SPACING_US);
  }
  float mean = sum / Config::SOUND_SAMPLES;
  float rms = sqrt(fmax(0.0, sumSquares / Config::SOUND_SAMPLES - mean * mean));
  float peak = fmax(highest - mean, mean - lowest);
  soundNoise = soundNoise == 0 ? rms : soundNoise + Config::ADC_NOISE_SMOOTHING * (rms - soundNoise);

  data.soundBias = static_cast<int>(mean + 0.5f);
  data.soundDoEvents = soundDoEdges;
  bool doLevel = digitalRead(Pins::SOUND_DO) == HIGH;
  data.soundDoActive = Config::SOUND_DO_ACTIVE_HIGH ? doLevel : !doLevel;

  // GPIO 34 não tem pull: pino solto só se revela pelo ruído alto mesmo em silêncio
  if (soundNoise > Config::SOUND_FLOATING_STDDEV) {
    data.micLink = link(Link::NOT_CONNECTED, LinkReason::FLOATING);
  } else if (data.soundBias < Config::SOUND_MIN_BIAS) {
    data.micLink = link(Link::SUSPECT, LinkReason::STUCK_LOW);
  } else if (data.soundBias > Config::SOUND_MAX_BIAS) {
    data.micLink = link(Link::SUSPECT, LinkReason::STUCK_HIGH);
  } else {
    data.micLink = link(Link::CONNECTED);
  }
  if (data.micLink.state != Link::CONNECTED) {
    data.noiseRms.valid = false;
    data.noiseDbfs.valid = false;
    return;
  }

  data.noiseRms.set(rms, now);
  data.noiseDbfs.set(toDbfs(rms), now);
  data.noisePeakDbfs = toDbfs(peak);

  // Piso de ruído: acompanha quedas na hora e subidas devagar
  float dbfs = data.noiseDbfs.value;
  if (data.noiseFloorDbfs <= -119.0f || dbfs < data.noiseFloorDbfs) {
    data.noiseFloorDbfs = dbfs;
  } else {
    data.noiseFloorDbfs =
        fmin(dbfs, data.noiseFloorDbfs + Config::SOUND_FLOOR_RISE_DB_PER_S * elapsed / 1000.0f);
  }
}

// ---------- Rótulos ----------

AirLevel classifyAir(int mq135Adc) {
  if (mq135Adc >= Config::MQ135_LEVEL_HIGH) return AirLevel::HIGH_LEVEL;
  if (mq135Adc >= Config::MQ135_LEVEL_MEDIUM) return AirLevel::MEDIUM_LEVEL;
  return AirLevel::LOW_LEVEL;
}

const char *airLevelOledLabel(AirLevel level) {
  switch (level) {
    case AirLevel::HIGH_LEVEL: return "ALTO";
    case AirLevel::MEDIUM_LEVEL: return "MEDIO";
    default: return "BAIXO";
  }
}

const char *airLevelTelemetryLabel(AirLevel level) {
  switch (level) {
    case AirLevel::HIGH_LEVEL: return "high";
    case AirLevel::MEDIUM_LEVEL: return "medium";
    default: return "low";
  }
}

const char *linkCode(Link l) {
  switch (l) {
    case Link::CONNECTED: return "CONNECTED";
    case Link::NOT_CONNECTED: return "NOT_CONNECTED";
    case Link::SUSPECT: return "SUSPECT";
    default: return "UNKNOWN";
  }
}

const char *linkReasonCode(LinkReason r) {
  switch (r) {
    case LinkReason::FLOATING: return "floating";
    case LinkReason::STUCK_LOW: return "stuck_low";
    case LinkReason::STUCK_HIGH: return "stuck_high";
    case LinkReason::NO_RESPONSE: return "no_response";
    case LinkReason::ECHO_STUCK: return "echo_stuck";
    case LinkReason::INTERMITTENT: return "intermittent";
    default: return "none";
  }
}

const char *echoResultCode(EchoResult r) {
  switch (r) {
    case EchoResult::VALID: return "VALID";
    case EchoResult::NO_ECHO: return "NO_ECHO";
    case EchoResult::BUSY: return "BUSY";
    default: return "NO_RESPONSE";
  }
}
