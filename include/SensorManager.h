#pragma once

#include <Arduino.h>
#include <DHT.h>

#include "PinMap.h"

// Última leitura de um sensor, com validade e momento em que foi obtida
template <typename T>
struct Reading {
  T value{};
  bool valid = false;
  uint32_t timestamp = 0;

  void set(T newValue, uint32_t now) {
    value = newValue;
    valid = true;
    timestamp = now;
  }

  bool isFresh(uint32_t now, uint32_t maxAgeMs) const {
    return valid && (now - timestamp) <= maxAgeMs;
  }
};

// Evidência física de que o sensor está ligado — independente do valor medido
enum class Link : uint8_t {
  UNKNOWN,        // ainda sem evidência suficiente
  CONNECTED,      // o hardware responde / o sinal é coerente
  NOT_CONNECTED,  // assinatura de fio solto ou módulo ausente
  SUSPECT,        // responde, mas de forma anormal (curto, intermitente)
};

enum class LinkReason : uint8_t {
  NONE,
  FLOATING,       // pino solto (ruído alto ou segue o pull interno)
  STUCK_LOW,      // leitura presa em 0
  STUCK_HIGH,     // leitura presa no máximo
  NO_RESPONSE,    // módulo não respondeu ao comando
  ECHO_STUCK,     // ECHO do HC-SR04 preso em HIGH
  INTERMITTENT,   // responde às vezes
};

struct LinkStatus {
  Link state = Link::UNKNOWN;
  LinkReason reason = LinkReason::NONE;
};

// Resultado de uma medição isolada do HC-SR04
enum class EchoResult : uint8_t {
  VALID,        // eco recebido, distância calculada
  NO_ECHO,      // módulo respondeu, mas nenhum objeto dentro do timeout
  NO_RESPONSE,  // ECHO nunca subiu: módulo ausente, sem 5 V ou TRIG solto
  BUSY,         // ECHO ainda em HIGH da medição anterior
};

struct SensorReadings {
  // LDRs (ADC, não lux)
  Reading<int> ambientAdc;
  Reading<int> localAdc;
  float ambientNoise = 0;  // desvio padrão das amostras
  float localNoise = 0;
  LinkStatus ambientLink, localLink;

  // HC-SR04
  Reading<float> distanceCm;
  EchoResult lastEcho = EchoResult::NO_RESPONSE;
  uint8_t echoValidPercent = 0;     // medições com eco
  uint8_t echoResponsePercent = 0;  // medições em que o módulo respondeu
  uint8_t echoSamples = 0;
  bool ultrasonicPresence = false;  // objeto confirmado na faixa configurada
  bool ultrasonicEverEchoed = false;
  LinkStatus ultrasonicLink;

  // Sensor IR
  Reading<bool> irDetected;  // já com polaridade aplicada
  uint16_t irChanges = 0;
  uint32_t irLastChangeMs = 0;
  LinkStatus irLink;

  // DHT11
  Reading<float> temperatureC;
  Reading<float> humidity;
  Reading<float> heatIndexC;
  uint8_t dhtSuccessPercent = 0;
  uint8_t dhtSamples = 0;
  uint8_t dhtFailures = 0;  // leituras inválidas seguidas
  LinkStatus dhtLink;

  // MQ-135 (relativo, não ppm)
  Reading<int> mq135Adc;
  LinkStatus mq135Link;

  // KY-038 (relativo ao fundo de escala do ADC, não dB SPL)
  Reading<float> noiseRms;
  Reading<float> noiseDbfs;
  float noisePeakDbfs = -120;   // pico desde a última atualização
  float noiseFloorDbfs = -120;  // piso de ruído do ambiente
  int soundBias = 0;           // nível DC da saída AO
  uint16_t soundDoEvents = 0;  // disparos do comparador desde o boot
  bool soundDoActive = false;  // DO acima do limiar agora
  LinkStatus micLink;
};

// Classificação RELATIVA do MQ-135 pelos limites do AppConfig (não é ppm)
enum class AirLevel : uint8_t { LOW_LEVEL, MEDIUM_LEVEL, HIGH_LEVEL };

AirLevel classifyAir(int mq135Adc);
const char *airLevelOledLabel(AirLevel level);
const char *airLevelTelemetryLabel(AirLevel level);

const char *linkCode(Link link);
const char *linkReasonCode(LinkReason reason);
const char *echoResultCode(EchoResult result);

// Inicializa e lê os sensores em intervalos próprios, e verifica periodicamente
// se cada um está fisicamente ligado. Não toma decisões de iluminação.
class SensorManager {
 public:
  void begin();
  void update(uint32_t now);

  const SensorReadings &readings() const { return data; }
  // Leitura imediata do LDR do poste (teste óptico do LED 1 no boot)
  int sampleLocalLdr();

 private:
  void updateLdrs(uint32_t now);
  void updateUltrasonic(uint32_t now);
  void updateIr(uint32_t now);
  void updateDht(uint32_t now);
  void updateMq135(uint32_t now);
  void updateNoise(uint32_t now);
  void runProbes(uint32_t now);

  EchoResult measureDistance(float &distanceCm);
  void readLdr(uint8_t pin, Reading<int> &reading, float &noise, LinkStatus &link,
               uint32_t &badSince, bool probeFloating, bool &everLit, uint32_t now);
  static bool adcHasPulls(uint8_t pin);
  bool adcPinFloats(uint8_t pin);
  bool digitalPinFloats(uint8_t pin);

  DHT dht{Pins::DHT_DATA, DHT11};
  SensorReadings data;

  uint32_t lastLdrMs = 0;
  uint32_t lastUltrasonicMs = 0;
  uint32_t lastDhtMs = 0;
  uint32_t lastMq135Ms = 0;
  uint32_t lastNoiseMs = 0;
  uint32_t lastProbeMs = 0;

  uint32_t ambientBadSince = 0;
  bool ambientProbeFloating = false;
  bool localProbeFloating = false;
  bool ambientEverLit = false;  // já leu luz normal desde o boot
  bool localEverLit = false;
  uint32_t localBadSince = 0;

  // HC-SR04
  uint8_t inRangeCount = 0;
  uint32_t echoHighSince = 0;
  bool echoFloating = false;
  EchoResult echoHistory[32] = {};
  uint8_t echoHistoryPos = 0;

  // IR
  bool irLastRaw = false;
  bool irStableRaw = false;
  bool irInitialized = false;
  uint32_t irChangedMs = 0;

  // DHT11
  bool dhtHistory[16] = {};
  uint8_t dhtHistoryPos = 0;
  bool dhtEverOk = false;

  // MQ-135
  bool mqProbeFloating = false;

  // KY-038
  float soundNoise = 0;  // média móvel do RMS, usada para detectar pino solto
};
