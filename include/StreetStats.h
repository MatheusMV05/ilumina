#pragma once

#include <Arduino.h>

#include "SensorManager.h"

// Estatísticas da rua por hora do dia (0 a 23h): quanta gente passa, quão barulhento é
// e como ficam temperatura e umidade. Acumula 24h por dia, mesmo com o painel fechado,
// e grava na flash (NVS) para sobreviver a reinícios.
//
// Relógio: o ESP32 não tem RTC. A hora vem do painel (o navegador manda a hora dele ao abrir)
// ou do NTP quando o Wi-Fi do roteador está conectado. Sem hora certa nada é registrado,
// porque não daria para saber em que faixa de horário a amostra cai.
class StreetStats {
 public:
  static constexpr uint8_t HOURS = 24;

  struct Bucket {
    uint32_t seconds = 0;         // segundos observados nesta hora do dia
    uint32_t passages = 0;        // passagens: início de um movimento detectado
    uint32_t motionSeconds = 0;   // segundos com movimento à frente do sensor
    uint32_t noiseSeconds = 0;    // segundos com leitura do microfone
    float noiseSumDb = 0;         // soma do nível acima do silêncio (dB)
    uint32_t loudSeconds = 0;     // segundos com som alto
    float noisePeakDb = 0;        // maior nível acima do silêncio
    uint32_t climateSeconds = 0;  // segundos com leitura do DHT11
    float temperatureSum = 0;
    float humiditySum = 0;
  };

  void begin();
  // Chamado a cada volta do loop; presença já combinada (HC-SR04 ou IR, nunca o som)
  void update(const SensorReadings &r, bool presence, bool wifiConnected, uint32_t now);

  // Hora vinda do painel: epoch em ms (UTC) e fuso em minutos (ex.: -180 para Brasília)
  void setClock(int64_t epochMs, int16_t tzMinutes);
  void reset();

  bool clockValid() const;
  size_t buildJson(char *out, size_t size) const;

 private:
  int currentHour() const;
  void save();

  Bucket buckets[HOURS];
  int16_t tzMinutes = -180;
  uint32_t sinceEpoch = 0;  // primeira amostra registrada (para o painel dizer "desde quando")
  bool ntpStarted = false;
  bool dirty = false;
  bool lastPresence = false;
  uint32_t presenceOffSince = 0;
  uint32_t lastTickMs = 0;
  uint32_t lastSaveMs = 0;
};
