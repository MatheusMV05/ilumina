# Gestão Inteligente de Iluminação Pública com IoT

Firmware do protótipo acadêmico descrito em
[`gestao_inteligente_iluminacao_publica_iot.md`](gestao_inteligente_iluminacao_publica_iot.md).
Um ESP32 lê luminosidade, presença e sensores ambientais, controla quatro LEDs
que representam luminárias, mostra tudo no OLED e num dashboard web, e publica
telemetria via MQTT (opcional). O controle da iluminação funciona sem rede.

## Placa

NodeMCU-32S / ESP32 DevKit com módulo **ESP-WROOM-32**, 38 pinos, USB-C
(veja `pinagem_esp32_correta.png`). **Ligue sempre pelo nome impresso na placa**,
não pela posição em desenhos de outras placas — os modelos de 30 e 38 pinos têm
ordens diferentes.

## Mapa de pinos (`include/PinMap.h`)

| Componente | Pino do módulo | Liga em | Observação |
|---|---|---|---|
| OLED SSD1306 | VCC / GND / SDA / SCL | 3V3 / GND / GPIO 21 / GPIO 22 | I2C; conferir a ordem VCC/GND impressa no módulo |
| HC-SR04 | VCC / GND / TRIG / ECHO | Vin (5 V) / GND / GPIO 5 / GPIO 18 | ECHO pelo divisor 1 kΩ + 2 kΩ |
| KY-038 (som) | + / G / AO / DO | 3V3 / GND / GPIO 34 / GPIO 35 | 34 e 35 são só entrada |
| DHT11 | VCC / DATA / GND | 3V3 / GPIO 4 / GND | pull-up 10 kΩ entre DATA e 3V3 se o sensor for "nu" |
| LDR ambiente | uma perna / outra perna | 3V3 / GPIO 33 + 10 kΩ até GND | longe dos LEDs |
| LDR do poste | uma perna / outra perna | 3V3 / GPIO 32 + 10 kΩ até GND | encostado no LED do poste 1 (confirma que ele acende) |
| LEDs dos postes | ânodo | GPIO 25, 26, 27, 13 via 220 Ω | cátodo no GND |
| LED de status | ânodo | GPIO 19 via 220 Ω | cátodo no GND |

Sensor IR e MQ-135 **não fazem parte desta montagem**: estão desligados em
`AppConfig.h` (`IR_INSTALLED`, `MQ135_INSTALLED`) e aparecem como "não instalado".
Para usá-los, escolha pinos livres em `PinMap.h` e habilite. Sem o IR, a presença
vem só do HC-SR04.

Divisores: LDR = `3V3 ── LDR ──┬── GPIO ── 10 kΩ ── GND`.
ECHO e AO = `sinal ── R1 ──┬── GPIO ── R2 ── GND`.

> O divisor 1 kΩ (do ECHO ao GPIO) + 2 kΩ (do GPIO ao GND) entrega ~3,3 V, nível
> seguro e bem reconhecido pelo ESP32. O GPIO 5 (TRIG) é um pino de boot, mas
> como o TRIG do HC-SR04 é uma entrada, não interfere na inicialização.

## Alimentação

- ESP32 pelo USB-C. OLED, DHT11, KY-038 e LDRs em **3,3 V**.
- HC-SR04 e MQ-135 em **5 V** (fonte de 5 V/1 A recomendada; o aquecedor do MQ-135 consome bastante).
- **Todos os GNDs em comum.** Nunca ligue 5 V externo no pino 3V3 nem em paralelo com o USB.

## Compilar e gravar

Requer [PlatformIO](https://platformio.org/). As bibliotecas são instaladas sozinhas
(`platformio.ini`): Adafruit SSD1306, Adafruit GFX, DHT sensor library,
Adafruit Unified Sensor e PubSubClient.

```bash
pio run                 # compila
pio run -t upload       # grava (feche o Serial Monitor antes)
pio device monitor      # Serial a 115200
```

## Wi-Fi, MQTT e credenciais

Copie `secrets.example.h` para `include/secrets.h` e preencha. Esse arquivo
está no `.gitignore`. Sem ele, o firmware compila e roda em **modo local**.

- `WIFI_SSID` vazio → sem Wi-Fi de roteador (dashboard continua pela rede própria).
- `MQTT_ENABLED true` + `MQTT_HOST` → publica em
  `public-lighting/{DEVICE_ID}/telemetry` (a cada 15 s),
  `.../status` (saúde dos componentes, a cada 60 s e ao conectar) e
  `.../availability` (`online`/`offline`, retido, com LWT).
- Wi-Fi e MQTT rodam numa tarefa no núcleo 0: quedas e reconexões não travam a iluminação.
- A senha nunca é impressa no Serial.

## Dashboard

O ESP32 cria a rede **`Iluminacao-IoT`** (senha padrão `iluminacao123` —
**troque em `include/secrets.h`**). Conecte o celular ou o notebook nela e abra
**http://192.168.4.1**. Se o Wi-Fi do roteador estiver configurado, o painel
também responde no IP mostrado no Serial (`[WIFI] Conectado. IP: ...`).

O painel atualiza a cada segundo e mostra:

- **Resumo no topo** em linguagem simples ("Tudo funcionando", "Funcionando,
  com 3 pendências", "Modo seguro"), com a lista do que fazer;
- **Como a luz está sendo decidida**: claro/escuro (limites e histerese) →
  presença (HC-SR04 **OU** IR) → modo, com o motivo → nível de cada luminária;
- **Cartão por sensor** com a leitura e o que ela significa ("claro",
  "som moderado"...), mini-gráfico de 2 minutos e **três verificações**:
  1. **Conectado** — o firmware detecta o hardware no pino (veja abaixo);
  2. **Medindo** — o sinal é coerente (taxa de eco, leituras válidas, áudio contínuo);
  3. **Teste prático** — botão que pede um estímulo ("cubra o LDR", "bata
     palmas") e confirma que o sensor reagiu dentro do tempo;
  Se o sensor não for detectado, o cartão abre sozinho a ligação esperada.
- **Histórico de 5 minutos** do LDR ambiente e da intensidade dos LEDs;
- **Sistema**: OLED, Wi-Fi, MQTT e botões para você confirmar que viu os
  LEDs e o verde do RGB acenderem (o ESP32 não consegue medir isso).

API: `GET /api/state` (JSON), `POST /api/test?c=LDR_AMBIENT` (inicia teste
prático), `POST /api/confirm?c=LEDS` (confirmação visual). Nenhuma rota aciona
saídas. No Serial Monitor, enviar `j` imprime o JSON do painel.

### Como a conexão de cada sensor é detectada

Medido nesta placa: um pino de ADC solto oscila ±600–900 unidades; ligado a um
divisor, varia ±3.

| Sensor | Como o firmware sabe que está ligado |
|---|---|
| LDRs | GPIO 32/33 têm pull interno: pino solto segue o pull; presa em 0 desde o boot ou em 4095 = ligação errada |
| LED 1 | o LDR do poste, encostado nele, precisa subir ao menos 150 ADC quando o LED acende no boot |
| MQ-135 | ruído + liga o pull-up/pull-down interno do GPIO 32: pino solto segue o pull |
| HC-SR04 | ECHO solto segue o pull interno; com o divisor, o módulo **sempre** sobe o ECHO ~0,5 ms após o TRIG, haja objeto ou não |
| IR | pull-up × pull-down na saída: se o nível acompanha o pull, nada está ligado |
| DHT11 | responde ou não ao protocolo; taxa de leituras válidas |
| KY-038 | GPIO 34 não tem pull: AO solto oscila centenas de ADC mesmo em silêncio; AO em 0 = sem alimentação. DO conta disparos do comparador |
| OLED | responde no endereço I2C |

Para ver o layout sem o ESP32, abra `web/dashboard.html?demo` no navegador
(dados simulados, sinalizados na tela).

**Editar o painel:** altere `web/dashboard.html` e regere o cabeçalho embutido:

```bash
python3 - <<'EOF'
html = open('web/dashboard.html', encoding='utf-8').read()
open('include/DashboardPage.h', 'w', encoding='utf-8').write(
    '// Gerado a partir de web/dashboard.html — não edite aqui.\n'
    '// Para regerar, veja a seção "Dashboard" do README.\n'
    '#pragma once\n\n#include <Arduino.h>\n\n'
    f'const char DASHBOARD_HTML[] PROGMEM = R"DASHBOARD({html})DASHBOARD";\n')
EOF
```

## Funcionamento

### Diagnóstico de inicialização

Ao ligar, antes do painel normal, o OLED e o Serial mostram:

1. LEDs acendendo um a um e o verde do RGB → **teste visual** (o ESP32 não mede se o LED acendeu).
2. LDRs: leitura colada em 0 ou 4095 indica fio solto/sem resistor.
3. MQ-135: leitura plausível → **AQUECENDO** até `MQ135_WARMUP_MS`.
4. DHT11: até 2 tentativas de leitura válida.
5. KY-038: saída AO polarizada e estável (silêncio não é defeito).
6. **HC-SR04:** “Aproxime objeto 10-50cm” por 6 s. Eco → OK; sem eco → ATENÇÃO.
7. **IR:** “Passe objeto no IR” por 6 s. Mudou de nível → OK; senão → NÃO TESTADO.
8. Wi-Fi/MQTT, sem bloquear além do timeout.

Os testes manuais nunca travam: depois do tempo limite a inicialização segue com
o alerta. Os estados também são reavaliados durante a operação (ex.: o IR vira OK
na primeira vez que detectar algo).

### Lógica de iluminação

| Condição | LEDs |
|---|---|
| Dia | 0% |
| Noite, sem presença | 25% |
| Noite, com presença (ou até 10 s depois) | 100% |
| LDR ambiente indisponível | 100% (modo seguro: nunca assume dia) |
| Noite sem nenhum sensor de presença **confirmado** | 100% (modo seguro) |

Um sensor de presença só é considerado confirmado depois de dar sinal de vida
(eco válido no HC-SR04 ou mudança de nível no IR). Até lá, o sistema não depende
dele à noite. Faça os testes manuais do boot para sair do modo seguro.

As transições de intensidade são graduais (`LIGHT_FADE_MS_PER_PERCENT`).

## Calibração (`include/AppConfig.h`)

Nenhum valor abaixo é medição — são pontos de partida:

- **`DAY_THRESHOLD` / `NIGHT_THRESHOLD`** (2200/1800): observe o LDR ambiente
  no Serial (`[LOOP] LDR amb=...`) ou no dashboard, com o ambiente claro e
  depois coberto, e ponha os limites entre os dois valores, com folga entre eles.
  Se a leitura diminuir com mais luz, use `LDR_INVERTED = true`.
- **`IR_ACTIVE_LOW`**: confira no dashboard se “Saída” vira “obstáculo” ao
  passar a mão; se ficar invertido, troque o valor. Ajuste a distância no trimpot.
- **`RGB_COMMON_ANODE`**: `true` se o comum do RGB vai no 3,3 V.
- **`MIN/MAX_DISTANCE_CM`**: faixa do HC-SR04 que conta como presença.
- **`MQ135_WARMUP_MS`** e **`MQ135_LEVEL_MEDIUM/HIGH`**: classificação
  relativa (BAIXO/MÉDIO/ALTO), **não ppm**.
- **`ADC_FLOATING_STDDEV`**: ruído acima do qual um pino de ADC é considerado solto.
- **Testes práticos** (`TEST_WINDOW_MS`, `LDR_TEST_*`, `MIC_TEST_MIN_RISE_DB`...):
  quanto o sensor precisa reagir para o teste passar.

## Roteiro de testes

| Teste | Como | Esperado |
|---|---|---|
| Boot normal | ligar tudo | diagnóstico no OLED, depois o painel |
| OLED desconectado | ligar sem OLED | firmware segue; erro no Serial |
| DHT11 desconectado | ligar sem ele | DHT11 FALHA; luz continua |
| Noite | cobrir o LDR ambiente | NOITE após o limite; LEDs em 25% |
| Dia | iluminar o LDR | DIA; LEDs apagam |
| Histerese | luz perto do limite | estado não oscila |
| Presença | objeto a 5–120 cm à noite | LEDs em 100%, voltam a 25% após 10 s |
| IR | passar a mão | “Detectou” no dashboard |
| Sensores de presença ausentes | ligar sem os dois, à noite | modo seguro (100%) |
| Wi-Fi / MQTT fora | desligar roteador/broker | controle local continua; reconexão no Serial |
| Uso prolongado | deixar ligado | sem travar ou reiniciar |

## Limitações

- LDR em ADC, não lux; som do KY-038 em dB relativo ao ADC, não dB SPL.
- O áudio é processado em blocos só para calcular RMS; nada é gravado ou transmitido.
- Teste de LED é visual; confirmar eletricamente exigiria realimentação.
- O protótipo não comprova, sozinho, as metas de 35% de economia ou 50% em manutenção.
- O GPIO 14 (LED 2) emite pulsos durante o boot do ESP32; uma piscada breve é normal.

## Estrutura

```
include/  AppConfig.h  PinMap.h  Components.h  SensorManager.h  SensorValidator.h
          Diagnostics.h  LightingController.h
          DisplayManager.h  ConnectivityManager.h  DashboardServer.h  Telemetry.h
          DashboardPage.h (gerado)
src/      main.cpp + um .cpp por módulo
web/      dashboard.html (fonte do painel)
secrets.example.h
```
