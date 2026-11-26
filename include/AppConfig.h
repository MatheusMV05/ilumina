#pragma once

#include <Arduino.h>

// Credenciais ficam em include/secrets.h (fora do controle de versão).
// Copie secrets.example.h para include/secrets.h e preencha.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef MQTT_ENABLED
#define MQTT_ENABLED false
#endif
#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef DEVICE_ID
#define DEVICE_ID "poste-01"
#endif
// Dashboard web: rede própria do ESP32 (sempre disponível, mesmo sem roteador)
#ifndef DASHBOARD_ENABLED
#define DASHBOARD_ENABLED true
#endif
#ifndef DASHBOARD_AP_SSID
#define DASHBOARD_AP_SSID "Iluminacao-IoT"
#endif
#ifndef DASHBOARD_AP_PASSWORD
#define DASHBOARD_AP_PASSWORD "iluminacao123"  // mínimo de 8 caracteres
#endif

// Todos os parâmetros ajustáveis do firmware ficam aqui.
// Valores marcados com "calibrar" são pontos de partida, não medições.
namespace Config {

// ---------- Serial ----------
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t SERIAL_LOG_INTERVAL_MS = 2000;

// ---------- Iluminação ----------
constexpr uint8_t LIGHT_OFF_PERCENT = 0;
// Sem movimento o brilho acompanha o escuro: no começo do anoitecer (LDR logo abaixo de
// NIGHT_THRESHOLD) acende em LIGHT_DUSK_PERCENT e sobe aos poucos até LIGHT_NIGHT_PERCENT
// quando o LDR chega em DARK_THRESHOLD. O máximo fica reservado para quando há movimento.
constexpr uint8_t LIGHT_DUSK_PERCENT = 20;       // fim de tarde, ainda um pouco claro
constexpr uint8_t LIGHT_NIGHT_PERCENT = 60;      // noite fechada sem movimento
constexpr uint8_t LIGHT_ACTIVE_PERCENT = 100;
constexpr uint8_t LIGHT_SAFE_PERCENT = 100;      // modo seguro: intensidade alta
constexpr uint32_t PRESENCE_HOLD_MS = 10000;     // mantém o máximo após o último movimento
constexpr uint32_t LIGHT_FADE_MS_PER_PERCENT = 4;  // 0 = troca direta, sem transição
constexpr uint32_t LIGHT_PWM_FREQ_HZ = 5000;
constexpr uint8_t LIGHT_PWM_RESOLUTION_BITS = 8;

// ---------- Luminosidade (ADC 12 bits) — calibrar ----------
constexpr int DAY_THRESHOLD = 2200;    // acima disso apaga (dia)
constexpr int NIGHT_THRESHOLD = 1800;  // abaixo disso acende (começo do anoitecer)
constexpr int DARK_THRESHOLD = 900;    // abaixo disso é noite fechada: brilho de LIGHT_NIGHT_PERCENT
// true se a montagem fizer a leitura DIMINUIR com mais luz (LDR e resistor trocados)
constexpr bool LDR_INVERTED = false;
constexpr uint8_t LDR_SAMPLES = 16;
// Leitura lenta (testes ópticos e calibração): 64 amostras em ~17 ms = 2 ciclos de 120 Hz.
// Medido nesta montagem: janela curta de 3 ms mostrou ruído de 24–38 ADC sob a luz da sala
constexpr uint8_t LDR_SLOW_SAMPLES = 64;
constexpr uint32_t LDR_SLOW_SPACING_US = 260;
// Decisão de dia/noite usa a leitura filtrada (média móvel, ~1 s a 4 leituras por segundo)
constexpr float LDR_DECISION_SMOOTHING = 0.25f;
// Os LEDs acesos deslocam a leitura do LDR ambiente (GND e 3V3 compartilhados na protoboard).
// Medido nesta montagem: +25 a +111 ADC por LED a 100%. O boot mede o desvio com os 4 LEDs
// acesos e o firmware desconta proporcionalmente ao brilho atual, para a luz não se "ver"
constexpr int LDR_CROSSTALK_MAX = 800;  // acima disso a medição do boot é descartada
// Leituras coladas nos extremos indicam ligação errada. Perto de 0 desde o boot: o
// pino só vê o resistor até o GND (LDR fora da fileira ou sem 3V3). Depois que o LDR
// já mediu luz, leituras baixas passam a valer como "escuro" (dedo, noite).
constexpr int LDR_MIN_PLAUSIBLE = 100;
constexpr int LDR_MAX_PLAUSIBLE = 4090;
constexpr uint32_t LDR_FAULT_CONFIRM_MS = 5000;
// Pino de ADC solto oscila centenas de unidades; ligado a um divisor fica estável.
// Medido nesta placa: solto ≈ 600–900 de desvio padrão; ligado ≈ 3.
constexpr float ADC_FLOATING_STDDEV = 40.0f;
constexpr float ADC_NOISE_SMOOTHING = 0.3f;  // média móvel exponencial do ruído

// ---------- HC-SR04 ----------
constexpr float MIN_DISTANCE_CM = 5.0f;
constexpr float MAX_DISTANCE_CM = 120.0f;
constexpr uint32_t ULTRASONIC_TIMEOUT_US = 25000;
constexpr uint8_t ULTRASONIC_CONFIRM_READINGS = 2;  // leituras seguidas com movimento
// Movimento, não só presença: um objeto parado (fio, borda da protoboard) vira "fundo".
// Medido nesta montagem: objeto fixo entre 2 e 7 cm deixava a luz presa no máximo
constexpr float MOTION_MIN_CHANGE_CM = 8.0f;        // diferença para o fundo que conta como movimento
constexpr uint32_t MOTION_BACKGROUND_MS = 8000;     // tempo para um objeto parado virar fundo
constexpr float ULTRASONIC_NO_ECHO_CM = 400.0f;     // sem eco = nada à frente até o alcance do sensor
// O módulo sobe o ECHO ~0,5 ms após o TRIG, haja objeto ou não: prova que ele existe
constexpr uint32_t ECHO_RISE_TIMEOUT_US = 5000;
constexpr uint32_t ULTRASONIC_STUCK_MS = 1000;  // ECHO em HIGH sem parar => falha
constexpr uint8_t ULTRASONIC_HISTORY = 32;      // medições usadas nas taxas de eco

// ---------- Componentes instalados nesta montagem ----------
// IR e MQ-135 não estão na pinagem atual. Para usar, escolha pinos livres em
// PinMap.h e troque para true. Desligados, aparecem como "não instalado".
constexpr bool IR_INSTALLED = false;
constexpr bool MQ135_INSTALLED = false;

// ---------- Sensor IR ----------
constexpr bool IR_ACTIVE_LOW = true;  // maioria dos módulos vai a LOW ao detectar — confirmar
constexpr uint32_t IR_DEBOUNCE_MS = 50;

// ---------- LED de status ----------
constexpr bool STATUS_LED_ACTIVE_LOW = false;  // ânodo no GPIO, cátodo no GND

// ---------- DHT11 ----------
constexpr uint8_t DHT_HISTORY = 10;  // leituras usadas na taxa de sucesso

// ---------- MQ-135 ----------
constexpr uint32_t MQ135_WARMUP_MS = 120000;  // pode precisar de muito mais no primeiro uso
constexpr uint8_t MQ135_SAMPLES = 8;
constexpr int MQ135_MIN_PLAUSIBLE = 5;
constexpr int MQ135_MAX_PLAUSIBLE = 4090;
// Classificação RELATIVA do valor ADC (não é ppm) — calibrar no ambiente
constexpr int MQ135_LEVEL_MEDIUM = 1500;
constexpr int MQ135_LEVEL_HIGH = 2500;

// ---------- KY-038 (som) ----------
// A saída AO fica num nível DC fixo e o som aparece como variação em torno dele.
constexpr uint16_t SOUND_SAMPLES = 128;           // por bloco
constexpr uint32_t SOUND_SAMPLE_SPACING_US = 80;  // bloco de ~12 ms
// Pino solto (34 não tem pull) oscila centenas de ADC mesmo em silêncio;
// o KY-038 em silêncio varia poucas unidades. Medido nesta placa: solto ≈ 600–900.
constexpr float SOUND_FLOATING_STDDEV = 300.0f;
constexpr int SOUND_MIN_BIAS = 20;      // AO preso em 0: módulo sem alimentação
constexpr int SOUND_MAX_BIAS = 4075;    // AO preso no máximo
constexpr bool SOUND_DO_ACTIVE_HIGH = true;  // DO sobe quando o som passa do limiar — confirmar
constexpr float SOUND_FLOOR_RISE_DB_PER_S = 0.5f;  // piso de ruído sobe devagar

// ---------- Intervalos ----------
constexpr uint32_t DHT_INTERVAL_MS = 2000;
constexpr uint32_t LDR_INTERVAL_MS = 250;
constexpr uint32_t ULTRASONIC_INTERVAL_MS = 100;
constexpr uint32_t MQ135_INTERVAL_MS = 1000;
constexpr uint32_t NOISE_INTERVAL_MS = 200;
constexpr uint32_t PROBE_INTERVAL_MS = 3000;  // testes de conexão (pull interno)
constexpr uint32_t DISPLAY_INTERVAL_MS = 250;
constexpr uint32_t DISPLAY_PAGE_MS = 3000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 15000;
constexpr uint32_t STATUS_INTERVAL_MS = 60000;
// Leitura mais antiga que isso é tratada como indisponível
constexpr uint32_t DHT_STALE_MS = 10000;
constexpr uint32_t NOISE_STALE_MS = 2000;

// ---------- Testes práticos (disparados pelo dashboard) ----------
constexpr uint32_t TEST_WINDOW_MS = 15000;
constexpr uint32_t TEST_WINDOW_SLOW_MS = 45000;  // DHT11 e MQ-135 reagem devagar
constexpr int LDR_TEST_MIN_DELTA = 150;          // e pelo menos 25% da leitura inicial
constexpr float LDR_TEST_RELATIVE = 0.25f;
constexpr float ULTRASONIC_TEST_MAX_CM = 60.0f;
constexpr float SOUND_TEST_MIN_RISE_DB = 10.0f;  // acima do piso de ruído (ou disparo do DO)
constexpr float DHT_TEST_HUMIDITY_RISE = 2.0f;
constexpr float DHT_TEST_TEMPERATURE_RISE = 1.0f;  // DHT11 mede em passos de 1 °C
constexpr int MQ135_TEST_MIN_DELTA = 60;         // e pelo menos 8% da leitura inicial
constexpr float MQ135_TEST_RELATIVE = 0.08f;

// ---------- Verificação dos LEDs (ausência / fio solto) ----------
// Cada LED aceso desloca o LDR ambiente pela corrente no GND/3V3 compartilhado.
// Medido nesta montagem: +25 a +111 ADC por LED, ruído da média de 3 a 19.
constexpr uint8_t LAMP_CHECK_ROUNDS = 4;
constexpr uint32_t LAMP_CHECK_SETTLE_MS = 60;
constexpr int LAMP_CHECK_MIN_DELTA = 12;        // abaixo disso nenhum LED pode ser afirmado
constexpr float LAMP_CHECK_RELATIVE = 0.35f;    // LED precisa de 35% da resposta do mais forte
// Repete a verificação sozinho para um LED retirado aparecer sem clique no painel.
// Cada verificação pisca os LEDs por ~2,5 s; 0 desliga a repetição automática.
constexpr uint32_t LAMP_AUTO_CHECK_MS = 30000;
constexpr uint8_t MANUAL_DEFAULT_PERCENT = 100; // brilho de "Ligada" no painel

// ---------- Diagnóstico de inicialização ----------
constexpr uint32_t BOOT_SCREEN_MS = 1500;
constexpr uint32_t BOOT_SETTLE_MS = 2500;  // coleta inicial de evidências dos sensores
constexpr uint32_t LED_TEST_STEP_MS = 400;
// O LDR do poste fica encostado no LED do poste 1: se a leitura subir pelo menos
// isso com o LED aceso, o LED 1 está confirmado por medição
constexpr int LAMP1_OPTICAL_MIN_DELTA = 150;
constexpr uint32_t ULTRASONIC_TEST_WINDOW_MS = 6000;
constexpr uint32_t IR_TEST_WINDOW_MS = 6000;
constexpr uint32_t DIAG_PAGE_MS = 3000;

// ---------- Estatísticas da rua (por hora do dia) ----------
constexpr uint32_t STREET_PASSAGE_GAP_MS = 3000;      // sem movimento por esse tempo = próxima é outra passagem
constexpr float STREET_LOUD_DB = 12.0f;               // acima do silêncio: conta como "som alto"
constexpr uint32_t STREET_SAVE_INTERVAL_MS = 600000;  // grava na flash a cada 10 min (poupa a memória)
constexpr uint32_t STREET_JSON_INTERVAL_MS = 5000;
constexpr size_t STREET_JSON_SIZE = 4096;

// ---------- Rede ----------
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 30000;
constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 15000;
constexpr uint16_t MQTT_SOCKET_TIMEOUT_S = 3;
constexpr uint16_t MQTT_BUFFER_SIZE = 768;

// ---------- Dashboard ----------
constexpr uint32_t DASHBOARD_STATE_INTERVAL_MS = 500;
constexpr size_t DASHBOARD_JSON_SIZE = 8192;  // ~5,4 KB em uso com os 4 LEDs
constexpr uint16_t DASHBOARD_PORT = 80;

}  // namespace Config
