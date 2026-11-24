#include "Calibration.h"

#include <utility>

#include "AppConfig.h"
#include "LightingController.h"
#include "PinMap.h"
#include "SensorManager.h"

namespace {

constexpr uint32_t STREAM_INTERVAL_MS = 250;
constexpr uint8_t PROBE_REPEATS = 10;
constexpr uint8_t OPTICAL_ROUNDS = 5;
constexpr uint32_t OPTICAL_SETTLE_MS = 350;  // o LDR demora a responder à luz

// Como o teste elétrico interpreta o pino: o objetivo da calibração é confirmar isso na bancada
const char *lampVerdict(uint8_t highUp, uint8_t highDown) {
  if (highUp == PROBE_REPEATS && highDown == 0) return "segue os dois pulls: SOLTO?";
  if (highUp == 0 && highDown == 0) return "fica em LOW: LED+resistor ate o GND";
  if (highUp == PROBE_REPEATS && highDown == PROBE_REPEATS) return "fica em HIGH: curto com 3V3?";
  return "instavel";
}

int median(int *v, uint8_t n) {
  for (uint8_t i = 1; i < n; i++)
    for (uint8_t j = i; j > 0 && v[j - 1] > v[j]; j--) std::swap(v[j - 1], v[j]);
  return v[n / 2];
}

}  // namespace

bool Calibration::handle(char command, SensorManager &sensors, LightingController &lighting) {
  switch (command) {
    case 'h': printHelp(); return true;
    case 'c':
      streaming = !streaming;
      if (streaming) {
        Serial.println(
            "CAL_HEADER,ms,amb,amb_sd,amb_pu,amb_pd,amb_link,loc,loc_sd,loc_pu,loc_pd,loc_link,"
            "lamp_pct,l1u,l1d,l2u,l2d,l3u,l3d,l4u,l4d,dist_cm,echo,night");
      } else {
        Serial.println("[CAL] fluxo CSV desligado");
      }
      return true;
    case 'p': printProbeReport(sensors, lighting); return true;
    case 'o': runOpticalTest(lighting); return true;
    case 's': scanPins(); return true;
    default: return false;
  }
}

void Calibration::printHelp() const {
  Serial.println("[CAL] comandos: h ajuda | c fluxo CSV | p relatorio de ligacao | o teste optico | s varredura de pinos | j JSON");
}

void Calibration::update(uint32_t now, const SensorManager &sensors, LightingController &lighting) {
  if (!streaming || (lastRowMs != 0 && now - lastRowMs < STREAM_INTERVAL_MS)) return;
  lastRowMs = now | 1;
  const SensorReadings &r = sensors.readings();
  LampProbe p[Pins::LAMP_COUNT];
  for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) p[z] = lighting.probeLamp(z);
  Serial.printf("CAL,%lu,%d,%.1f,%d,%d,%s,%d,%.1f,%d,%d,%s,%u,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%d\n",
                static_cast<unsigned long>(now), r.ambientAdc.value, r.ambientNoise, r.ambientPullUp,
                r.ambientPullDown, linkCode(r.ambientLink.state), r.localAdc.value, r.localNoise,
                r.localPullUp, r.localPullDown, linkCode(r.localLink.state), lighting.zonePercent(0),
                p[0].highWithPullUp, p[0].highWithPullDown, p[1].highWithPullUp, p[1].highWithPullDown,
                p[2].highWithPullUp, p[2].highWithPullDown, p[3].highWithPullUp, p[3].highWithPullDown,
                r.distanceCm.valid ? String(r.distanceCm.value, 0).c_str() : "", echoResultCode(r.lastEcho),
                lighting.isNight());
}

void Calibration::printProbeReport(const SensorManager &sensors, LightingController &lighting) const {
  const SensorReadings &r = sensors.readings();
  Serial.println("\n[CAL] ===== RELATORIO DE LIGACAO =====");
  Serial.printf("[CAL] LDR amb   GPIO %u: media=%d ruido=%.1f pull-up=%d pull-down=%d -> %s/%s\n",
                Pins::LDR_AMBIENT, r.ambientAdc.value, r.ambientNoise, r.ambientPullUp, r.ambientPullDown,
                linkCode(r.ambientLink.state), linkReasonCode(r.ambientLink.reason));
  Serial.printf("[CAL] LDR local GPIO %u: media=%d ruido=%.1f pull-up=%d pull-down=%d -> %s/%s\n",
                Pins::LDR_LOCAL, r.localAdc.value, r.localNoise, r.localPullUp, r.localPullDown,
                linkCode(r.localLink.state), linkReasonCode(r.localLink.reason));
  Serial.println("[CAL]   (divisor ok: pull-up e pull-down quase iguais; solto: ~4095 e ~0)");
  for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) {
    uint8_t up = 0, down = 0;
    for (uint8_t i = 0; i < PROBE_REPEATS; i++) {
      LampProbe p = lighting.probeLamp(z);
      up += p.highWithPullUp;
      down += p.highWithPullDown;
      delay(2);
    }
    Serial.printf("[CAL] LED %u GPIO %2u: HIGH com pull-up %2u/%u, com pull-down %2u/%u -> %s\n", z + 1,
                  Pins::LAMPS[z], up, PROBE_REPEATS, down, PROBE_REPEATS, lampVerdict(up, down));
  }
  Serial.println("[CAL] =================================\n");
}

// Acende um LED por vez e mede a subida de cada LDR: mostra qual LED cada LDR enxerga
void Calibration::runOpticalTest(LightingController &lighting) const {
  Serial.println("\n[CAL] ===== TESTE OPTICO (acende um LED por vez) =====");
  lighting.setAllImmediate(0);
  delay(OPTICAL_SETTLE_MS);
  for (uint8_t z = 0; z < Pins::LAMP_COUNT; z++) {
    int dAmb[OPTICAL_ROUNDS], dLoc[OPTICAL_ROUNDS], offLoc = 0, onLoc = 0;
    for (uint8_t i = 0; i < OPTICAL_ROUNDS; i++) {
      int amb0 = SensorManager::sampleLdr(Pins::LDR_AMBIENT);
      int loc0 = SensorManager::sampleLdr(Pins::LDR_LOCAL);
      lighting.setZoneImmediate(z, 100);
      delay(OPTICAL_SETTLE_MS);
      int amb1 = SensorManager::sampleLdr(Pins::LDR_AMBIENT);
      int loc1 = SensorManager::sampleLdr(Pins::LDR_LOCAL);
      lighting.setZoneImmediate(z, 0);
      delay(OPTICAL_SETTLE_MS);
      dAmb[i] = amb1 - amb0;
      dLoc[i] = loc1 - loc0;
      offLoc = loc0;
      onLoc = loc1;
    }
    int amb = median(dAmb, OPTICAL_ROUNDS);
    int loc = median(dLoc, OPTICAL_ROUNDS);
    int spread = dAmb[OPTICAL_ROUNDS - 1] - dAmb[0];  // já ordenado pela mediana
    Serial.printf("[CAL] LED %u: LDR local %+5d (apagado %d, aceso %d)  LDR amb %+5d (variacao %d)  %s\n",
                  z + 1, loc, offLoc, onLoc, amb, spread,
                  loc >= Config::LAMP1_OPTICAL_MIN_DELTA ? "<- o LDR do poste ve este LED" : "");
  }
  Serial.printf("[CAL] limite atual para confirmar o LED 1: +%d ADC\n", Config::LAMP1_OPTICAL_MIN_DELTA);
  Serial.println("[CAL] ==================================================\n");
}

// ---------- Varredura dos pinos ----------

namespace {

struct ScanPin {
  uint8_t gpio;
  const char *role;  // o que a pinagem espera nesse pino
  bool adc1;         // dá para ler a tensão (ADC1 funciona com Wi-Fi ligado)
  bool pulls;        // tem pull interno (34 a 39 não têm)
};

// Só pinos que podem ser lidos sem atrapalhar o firmware: ficam de fora flash (6-11), UART (1, 3),
// I2C do OLED (21, 22), saídas dos LEDs e do TRIG, e o DHT11 (o pull atrapalharia a leitura)
const ScanPin SCAN[] = {
    {32, "LDR do poste", true, true},   {33, "LDR ambiente", true, true},
    {34, "KY-038 AO", true, false},     {35, "KY-038 DO", true, false},
    {36, "livre (VP)", true, false},    {39, "livre (VN)", true, false},
    {18, "ECHO do HC-SR04", false, true},
    {0, "livre (BOOT)", false, true},   {2, "livre (LED da placa)", false, true},
    {12, "livre", false, true},         {14, "livre", false, true},
    {15, "livre", false, true},         {16, "livre", false, true},
    {17, "livre", false, true},         {23, "livre (IR, nao instalado)", false, true},
};

// Lê o pino digital com cada pull: se acompanha os dois, nada segura a linha
void digitalPulls(uint8_t pin, bool &up, bool &down) {
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(300);
  up = digitalRead(pin);
  pinMode(pin, INPUT_PULLDOWN);
  delayMicroseconds(300);
  down = digitalRead(pin);
  pinMode(pin, INPUT);
}

}  // namespace

void Calibration::scanPins() const {
  Serial.println("\n[CAL] ===== VARREDURA DOS PINOS =====");
  for (const ScanPin &p : SCAN) {
    char analog[48] = "";
    if (p.adc1) {
      int mean = SensorManager::sampleLdr(p.gpio);  // média de 17 ms, serve para qualquer ADC
      snprintf(analog, sizeof(analog), "tensao %4d ADC (%.2f V)", mean, mean * 3.1f / 4095.0f);
    }
    const char *verdict;
    if (p.pulls) {
      bool up, down;
      digitalPulls(p.gpio, up, down);
      verdict = up && !down ? "solto (segue os pulls)" : up && down ? "preso em HIGH (algo liga em 3V3)"
                : !up && !down ? "preso em LOW (algo liga no GND)" : "?";
      // ECHO: depois da varredura volta a ser entrada simples, como o firmware usa
    } else {
      verdict = "sem pull interno: avalie pela tensao";
    }
    Serial.printf("[CAL] GPIO %2u %-26s %-24s %s\n", p.gpio, p.role, analog, verdict);
  }
  Serial.println("[CAL] Esperado: 32 e 33 com tensao entre 100 e 4000 e nao soltos; livres soltos.");
  Serial.println("[CAL] ==================================\n");
}
