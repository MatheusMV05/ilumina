# Especificação do Projeto --- Gestão Inteligente de Iluminação Pública com IoT

**Versão:** 1.0\
**Tipo:** Protótipo acadêmico com ESP32 e sensores IoT\
**Plataforma-alvo:** ESP32 DevKit com USB-C\
**Montagem:** duas protoboards, sem maquete física de rua

------------------------------------------------------------------------

## 1. Visão geral

O projeto consiste em um protótipo de gestão inteligente de iluminação
pública. Um ESP32 coleta informações de luminosidade, presença,
temperatura, umidade, qualidade relativa do ar e nível sonoro. Com base
principalmente na luminosidade ambiente e na detecção de presença, o
sistema controla quatro LEDs amarelos que representam luminárias
públicas.

O objetivo é demonstrar como uma infraestrutura de iluminação pode:

-   reduzir o consumo de energia mantendo a iluminação em nível reduzido
    quando não há movimento;
-   aumentar temporariamente a intensidade quando um objeto ou pessoa é
    detectado;
-   monitorar condições ambientais;
-   apresentar o estado dos sensores e do sistema em um display OLED;
-   disponibilizar telemetria pela rede Wi-Fi, com integração MQTT
    opcional.

**Importante:** a primeira versão deve funcionar localmente mesmo sem
Wi-Fi ou broker MQTT. A conectividade não pode impedir o funcionamento
da iluminação.

### 1.1 Contexto do problema

A iluminação pública convencional pode consumir energia excessiva e
depender de manutenção reativa. A falta de monitoramento em tempo real
dificulta identificar falhas e otimizar o consumo.

O protótipo propõe uma demonstração em pequena escala de uma solução
conectada, capaz de monitorar pontos de iluminação e ajustar seu
funcionamento de acordo com as condições observadas.

### 1.2 Objetivos do projeto

-   Desenvolver um sistema IoT de monitoramento e controle de
    iluminação.
-   Demonstrar o acionamento automático conforme a luminosidade
    ambiente.
-   Detectar presença para elevar temporariamente a intensidade
    luminosa.
-   Monitorar variáveis ambientais e apresentar seus valores.
-   Identificar falhas ou leituras indisponíveis durante a
    inicialização.
-   Preparar a aplicação para transmitir telemetria pela rede.

### 1.3 Relação com os ODS

-   **ODS 7 --- Energia limpa e acessível:** uso mais eficiente da
    energia.
-   **ODS 11 --- Cidades e comunidades sustentáveis:** infraestrutura
    urbana mais eficiente e monitorável.
-   **ODS 13 --- Ação contra a mudança global do clima:** potencial
    redução do consumo energético e dos impactos associados.

As metas de redução de 35% no consumo e de melhoria de 50% na eficiência
da manutenção são objetivos de longo prazo do problema proposto. **Não
devem ser apresentados como resultados comprovados pelo protótipo
acadêmico.** Para comprovar esses percentuais seria necessário um estudo
de campo com medições antes e depois.

------------------------------------------------------------------------

## 2. Escopo da primeira versão

A primeira versão será uma prova de conceito montada somente em
protoboards. Não haverá rua, calçadas, postes físicos ou base de MDF.

O conjunto terá:

1.  ESP32 como unidade central de processamento.
2.  Display OLED para diagnóstico e acompanhamento.
3.  Quatro LEDs amarelos representando as luminárias.
4.  Um LED RGB utilizado inicialmente como indicador visual de
    funcionamento.
5.  Dois LDRs para leitura de luminosidade.
6.  HC-SR04 e sensor infravermelho para detecção de presença/obstáculo.
7.  DHT22, MQ-135 e INMP441 para monitoramento ambiental.
8.  Duas protoboards e jumpers organizados.
9.  Wi-Fi integrado ao ESP32 e MQTT opcional para telemetria.

### 2.1 Componentes do protótipo

  ------------------------------------------------------------------------
  Componente                              Quantidade Função
  --------------------- ---------------------------- ---------------------
  ESP32 DevKit USB-C                               1 Unidade central e
                                                     conectividade Wi-Fi

  OLED 0,96" I2C,                                  1 Interface e
  preferencialmente                                  diagnóstico
  SSD1306 128×64                                     

  LED amarelo de 5 mm                              4 Representação das
                                                     luminárias

  LED RGB de 4                                     1 Indicador de estado
  terminais                                          

  LDR                                              2 Luminosidade ambiente
                                                     e leitura local

  HC-SR04                                          1 Medição de distância

  Sensor IR de                                     1 Detecção próxima de
  obstáculo com                                      obstáculos
  comparador/trimpot                                 

  INMP441                                          1 Captura de amostras
                                                     de áudio para estimar
                                                     nível sonoro relativo

  DHT22                                            1 Temperatura e umidade

  MQ-135                                           1 Indicador relativo de
                                                     gases/qualidade do ar

  Protoboard                                       2 Montagem do circuito

  Jumpers                        Conforme necessário Interligações
  ------------------------------------------------------------------------

### 2.2 Componentes passivos e alimentação necessários

A ilustração anterior era uma referência visual, não uma lista elétrica
completa. Para montar o circuito com segurança, considere também:

  ------------------------------------------------------------------------
  Item                        Quantidade recomendada Uso
  --------------------- ---------------------------- ---------------------
  Resistores de 220 Ω                              5 Um para cada um dos
                                                     quatro LEDs amarelos
                                                     e um para o canal
                                                     verde do RGB

  Resistores de 10 kΩ                   Pelo menos 6 Divisores dos dois
                                                     LDRs, proteção do
                                                     Echo do HC-SR04 e
                                                     proteção da saída
                                                     analógica do MQ-135

  Resistor de 4,7 kΩ a              1, se necessário Pull-up do DHT22 caso
  10 kΩ                                              o módulo não possua
                                                     resistor integrado

  Fonte regulada de 5 V                            1 Alimentação do
  / 1 A ou superior                                  HC-SR04 e MQ-135,
                                                     conforme a montagem

  Cabo USB-C                                       1 Alimentação e
                                                     programação do ESP32
  ------------------------------------------------------------------------

Se o LED RGB for utilizado nas três cores, serão necessários três
resistores para seus canais e três GPIOs. Nesta especificação, para
preservar os pinos e reproduzir o protótipo visual, somente o canal
verde será utilizado. Os canais vermelho e azul ficarão desconectados.

**Atenção:** a quantidade de resistores deve ser conferida antes da
montagem. Não ligue LEDs diretamente aos GPIOs e nunca aplique 5 V
diretamente a uma entrada do ESP32.

------------------------------------------------------------------------

## 3. Requisitos funcionais

  -----------------------------------------------------------------------
  ID                                  Requisito
  ----------------------------------- -----------------------------------
  RF-01                               Ao iniciar, o firmware deve
                                      executar uma rotina de diagnóstico
                                      dos periféricos.

  RF-02                               O resultado do diagnóstico deve ser
                                      apresentado no OLED, com estados
                                      como `OK`, `ATENÇÃO`, `FALHA`,
                                      `AQUECENDO` ou `NÃO TESTADO`.

  RF-03                               O sistema deve ler o LDR ambiente e
                                      determinar se está claro ou escuro.

  RF-04                               A decisão dia/noite deve utilizar
                                      histerese para evitar oscilações
                                      quando a luminosidade estiver
                                      próxima do limite.

  RF-05                               Durante o dia, os quatro LEDs
                                      amarelos devem permanecer apagados.

  RF-06                               Durante a noite, os LEDs devem
                                      permanecer em intensidade reduzida
                                      quando não houver presença
                                      detectada.

  RF-07                               Quando HC-SR04 ou sensor IR
                                      detectar presença, os quatro LEDs
                                      devem aumentar temporariamente a
                                      intensidade.

  RF-08                               Após o período sem novas detecções,
                                      a iluminação deve retornar
                                      gradualmente ou diretamente ao
                                      nível reduzido configurado.

  RF-09                               O DHT22 deve fornecer temperatura e
                                      umidade.

  RF-10                               O MQ-135 deve fornecer uma leitura
                                      analógica relativa, sem afirmar ppm
                                      sem calibração.

  RF-11                               O INMP441 deve fornecer uma
                                      estimativa de amplitude sonora
                                      relativa, sem afirmar decibéis SPL
                                      calibrados.

  RF-12                               O segundo LDR deve ser monitorado
                                      separadamente do LDR ambiente.

  RF-13                               A desconexão da rede não deve
                                      interromper o controle local.

  RF-14                               Quando configurado, o sistema deve
                                      publicar telemetria por MQTT.

  RF-15                               Falhas em sensores ambientais não
                                      críticos não devem bloquear a
                                      iluminação.
  -----------------------------------------------------------------------

------------------------------------------------------------------------

## 4. Arquitetura de funcionamento

O ESP32 será responsável por ler os sensores, tomar decisões e atualizar
as saídas.

``` text
                 ┌──────────────────────┐
                 │       ESP32          │
                 │  Controle principal  │
                 └──────────┬───────────┘
                            │
        ┌───────────────────┼────────────────────┐
        │                   │                    │
        ▼                   ▼                    ▼
  Luminosidade          Presença             Ambiente
  LDR ambiente          HC-SR04              DHT22
  LDR local             Sensor IR            MQ-135
                                              INMP441
        │                   │                    │
        └───────────────────┼────────────────────┘
                            ▼
                  Lógica de iluminação
                            │
                 ┌──────────┴──────────┐
                 ▼                     ▼
           4 LEDs amarelos          OLED + RGB
           (luminárias)             (interface)
                            │
                            ▼
                    Wi-Fi / MQTT
                     (opcional)
```

### 4.1 Interpretação dos sensores

-   **LDR ambiente:** determina o estado claro/escuro e é a referência
    principal do controle.
-   **LDR local:** fornece uma segunda leitura de luminosidade, útil
    para comparação e diagnóstico. Não deve substituir automaticamente o
    LDR ambiente.
-   **HC-SR04:** estima a distância de um objeto à frente do sensor.
-   **Sensor IR:** identifica obstáculos próximos. A polaridade do sinal
    deve ser configurável, pois muitos módulos acionam a saída em nível
    LOW.
-   **DHT22:** monitora temperatura e umidade.
-   **MQ-135:** fornece um valor analógico relativo associado à resposta
    do sensor a gases. Não identifica sozinho um gás específico.
-   **INMP441:** fornece amostras de áudio para calcular uma
    amplitude/RMS relativa. O projeto não deve gravar nem armazenar
    conversas.

### 4.2 Regra de detecção de presença

A presença será considerada detectada quando pelo menos uma das
condições ocorrer:

``` text
presenca = hc_sr04_detectou OR sensor_ir_detectou
```

Configuração inicial sugerida:

-   HC-SR04: considerar detecção entre 5 cm e 120 cm.
-   Sensor IR: utilizar a saída digital do módulo, com polaridade
    configurável.
-   Qualquer detecção reinicia o temporizador de iluminação intensa.
-   A ausência de eco do HC-SR04 significa apenas que não houve uma
    medição válida naquele ciclo. Não prova, isoladamente, que o sensor
    está defeituoso.

Esses limites são parâmetros de protótipo e deverão ser ajustados
durante os testes.

------------------------------------------------------------------------

## 5. Lógica de controle da iluminação

### 5.1 Estados de luminosidade

O sistema terá dois estados:

-   `DAY`: ambiente claro; luminárias desligadas.
-   `NIGHT`: ambiente escuro; luminárias habilitadas.

A mudança deve usar histerese. Valores iniciais sugeridos para ADC de 12
bits:

``` cpp
DAY_THRESHOLD   = 2200;
NIGHT_THRESHOLD = 1800;
```

Com o divisor descrito neste documento (3,3 V → LDR → entrada ADC →
resistor de 10 kΩ → GND), a leitura tende a aumentar com a luminosidade.
Se a montagem física produzir comportamento inverso, a interpretação
deverá ser invertida na configuração.

Regra:

-   Se a leitura ultrapassar `DAY_THRESHOLD`, mudar para `DAY`.
-   Se a leitura ficar abaixo de `NIGHT_THRESHOLD`, mudar para `NIGHT`.
-   Entre os dois limites, manter o estado anterior.

Os valores acima são apenas ponto de partida. Devem ser calibrados
observando as leituras reais no Serial Monitor e no OLED.

### 5.2 Níveis de iluminação

Parâmetros iniciais sugeridos:

``` cpp
LIGHT_OFF_PERCENT       = 0;
LIGHT_STANDBY_PERCENT   = 25;
LIGHT_ACTIVE_PERCENT    = 100;
PRESENCE_HOLD_SECONDS   = 10;
```

Comportamento esperado:

  Condição                                        LEDs amarelos
  ----------------------------------------------- ------------------------------
  Dia                                             0% --- apagados
  Noite, sem presença                             25% --- intensidade reduzida
  Noite, com presença                             100% --- intensidade máxima
  Noite, presença cessou e temporizador expirou   Retorna a 25%

Os quatro LEDs devem ser controlados individualmente por PWM, mas podem
receber o mesmo nível na primeira versão. A arquitetura deve permitir
futuramente controlar zonas separadas.

### 5.3 Falhas e modo seguro

-   Se o LDR ambiente falhar, o sistema não deve assumir que é dia. Deve
    entrar em modo seguro, acender as luminárias em intensidade alta e
    exibir o alerta.
-   Se os dois sensores de presença falharem e o ambiente estiver
    escuro, manter as luminárias em intensidade alta, evitando depender
    de uma detecção indisponível.
-   Se apenas um sensor de presença falhar, continuar utilizando o
    sensor disponível e exibir o alerta.
-   Falhas no DHT22, MQ-135 ou INMP441 não devem alterar a lógica de
    iluminação.
-   Se o Wi-Fi ou MQTT estiver indisponível, continuar operando
    localmente.

------------------------------------------------------------------------

## 6. Mapa de pinos sugerido

O mapa abaixo considera uma placa ESP32 DevKit baseada no módulo
ESP32-WROOM-32. Antes de implementar, confirme a identificação exata da
placa e os nomes impressos nos pinos.

  Componente      Terminal                     GPIO ESP32 Observação
  --------------- -------------------------- ------------ ------------------------------
  OLED I2C        SDA                             GPIO 21 Barramento I2C
  OLED I2C        SCL                             GPIO 22 Barramento I2C
  DHT22           DATA                             GPIO 4 Entrada digital
  LDR ambiente    Saída do divisor                GPIO 34 ADC1, somente entrada
  LDR local       Saída do divisor                GPIO 35 ADC1, somente entrada
  MQ-135          AO após divisor                 GPIO 32 ADC1, somente entrada lógica
  HC-SR04         TRIG                            GPIO 18 Saída digital
  HC-SR04         ECHO após divisor               GPIO 19 Entrada protegida contra 5 V
  Sensor IR       OUT                             GPIO 23 Confirmar polaridade
  LED amarelo 1   Anodo via resistor              GPIO 13 PWM
  LED amarelo 2   Anodo via resistor              GPIO 14 PWM
  LED amarelo 3   Anodo via resistor              GPIO 16 PWM
  LED amarelo 4   Anodo via resistor              GPIO 17 PWM
  LED RGB         Canal verde via resistor        GPIO 27 Indicador verde
  INMP441         SCK/BCLK                        GPIO 26 I2S
  INMP441         WS/LRCL                         GPIO 25 I2S
  INMP441         SD                              GPIO 33 I2S, entrada
  INMP441         L/R                                 GND Seleciona canal esquerdo

**Observações sobre o mapa:**

1.  GPIO 34 e GPIO 35 são somente entradas. Não podem controlar LEDs.
2.  GPIO 16 e GPIO 17 são adequados em placas ESP32-WROOM comuns, mas
    devem ser conferidos se a placa utilizar outro módulo, como uma
    variante com PSRAM.
3.  Não utilizar GPIOs de inicialização (*strapping pins*) sem
    necessidade.
4.  O pino `AO` do MQ-135 e o `ECHO` do HC-SR04 não podem ser ligados
    diretamente ao ESP32 quando puderem atingir 5 V.
5.  Se a placa ou algum módulo tiver pinagem diferente, adapte o mapa
    antes de energizar.

------------------------------------------------------------------------

## 7. Ligações elétricas importantes

### 7.1 Alimentação e terra

-   ESP32: alimentação e programação pelo USB-C.
-   OLED, DHT22, INMP441, LDRs e sensor IR: utilizar 3,3 V, respeitando
    a especificação do módulo.
-   HC-SR04: normalmente utiliza 5 V.
-   MQ-135: módulo normalmente utiliza 5 V devido ao aquecedor interno.
-   Todos os GNDs devem estar em comum.
-   Para o MQ-135 e o HC-SR04, recomenda-se uma fonte regulada de 5 V
    com corrente suficiente. Se utilizar uma fonte externa para esses
    módulos, una o GND dela ao GND do ESP32.
-   Não conecte a saída positiva de uma fonte externa de 5 V ao pino 3V3
    do ESP32.
-   Não una duas fontes de 5 V em paralelo. Se o ESP32 estiver ligado
    pelo USB-C, mantenha a alimentação externa dos sensores separada,
    compartilhando apenas o GND, salvo se a alimentação da placa tiver
    sido projetada e verificada para essa configuração.

### 7.2 LDRs

Cada LDR precisa de um divisor de tensão próprio:

``` text
3V3 ── LDR ──┬── GPIO ADC
             │
            10 kΩ
             │
            GND
```

-   LDR ambiente: GPIO 34.
-   LDR local: GPIO 35.
-   Não conecte o LDR diretamente ao GPIO sem o resistor do divisor.
-   A leitura ADC é um valor relativo, não lux. Para apresentar lux
    seria necessária calibração com instrumento de referência.

### 7.3 HC-SR04

-   VCC → 5 V.
-   GND → GND comum.
-   TRIG → GPIO 18.
-   ECHO → divisor resistivo → GPIO 19.

Divisor recomendado com dois resistores de 10 kΩ:

``` text
ECHO do HC-SR04 ── 10 kΩ ──┬── GPIO 19
                            │
                           10 kΩ
                            │
                           GND
```

O divisor reduz aproximadamente pela metade um sinal de 5 V. Nunca
conecte o ECHO de 5 V diretamente ao ESP32.

### 7.4 Sensor IR de obstáculo

-   VCC → começar com 3,3 V, se o módulo aceitar essa tensão.
-   GND → GND.
-   OUT → GPIO 23.
-   Ajustar o trimpot para definir a distância de detecção.
-   Confirmar com teste físico se a saída fica em LOW ou HIGH quando
    detecta um obstáculo.

Se o módulo precisar ser alimentado em 5 V, verifique a tensão de sua
saída. Caso OUT possa chegar a 5 V, utilize divisor ou conversor de
nível antes do GPIO.

### 7.5 LEDs amarelos

Cada LED deve possuir seu próprio resistor de 220 Ω:

``` text
GPIO ── 220 Ω ── Anodo do LED
                  Catodo ── GND
```

Um LED por GPIO, conforme o mapa de pinos. O firmware utilizará PWM para
variar a intensidade.

### 7.6 LED RGB

Para esta primeira versão:

-   Identificar se o RGB é de cátodo comum ou ânodo comum.
-   Utilizar somente o canal verde.
-   Inserir um resistor de 220 Ω em série com o canal verde.
-   Se for cátodo comum, ligar o terminal comum ao GND e controlar o
    canal verde em nível ativo HIGH.
-   Se for ânodo comum, ligar o terminal comum a 3,3 V e inverter a
    lógica do GPIO.
-   Deixar os canais vermelho e azul desconectados.

O RGB será um indicador de que o firmware está em execução. O OLED será
a fonte principal de informação sobre alertas.

### 7.7 DHT22

-   VCC → 3,3 V.
-   GND → GND.
-   DATA → GPIO 4.
-   Se o módulo não possuir pull-up integrado, adicionar um resistor de
    4,7 kΩ a 10 kΩ entre DATA e 3,3 V.

O DHT22 não deve ser consultado continuamente. Respeitar intervalo
mínimo de aproximadamente 2 segundos entre leituras.

### 7.8 MQ-135

-   VCC → 5 V.
-   GND → GND comum.
-   AO → divisor resistivo → GPIO 32.
-   DO não será utilizado nesta primeira versão.

Divisor de proteção com dois resistores de 10 kΩ:

``` text
AO do MQ-135 ── 10 kΩ ──┬── GPIO 32
                         │
                        10 kΩ
                         │
                        GND
```

O MQ-135 possui aquecedor e pode consumir corrente significativa. Não o
alimente pelo pino 3V3. Considere ventilação e evite tocar no
encapsulamento metálico durante o funcionamento.

**Limitação técnica:** o valor ADC do MQ-135 não representa diretamente
ppm. O sensor responde a diferentes gases e sofre influência de
temperatura, umidade, envelhecimento e calibração. Na primeira versão,
mostrar somente valor bruto e uma classificação relativa configurável.
Não afirmar que o protótipo mede concentrações precisas ou identifica um
gás específico.

### 7.9 INMP441

-   VDD → 3,3 V.
-   GND → GND.
-   SCK/BCLK → GPIO 26.
-   WS/LRCL → GPIO 25.
-   SD → GPIO 33.
-   L/R → GND para selecionar o canal esquerdo.

O firmware deverá inicializar I2S, receber amostras e calcular RMS ou
uma métrica equivalente de amplitude. Não armazenar nem transmitir áudio
bruto.

**Limitação técnica:** sem microfone e cadeia de medição calibrados,
apresentar nível relativo ou dBFS estimado, não dB SPL de precisão.

------------------------------------------------------------------------

## 8. Diagnóstico obrigatório na inicialização

Este é um requisito central do projeto.

Ao ligar ou reiniciar, o firmware deve executar uma sequência de
diagnóstico antes de apresentar o painel normal. O resultado deve ser
exibido no OLED e também enviado ao Serial Monitor.

### 8.1 Estados de diagnóstico

  -----------------------------------------------------------------------
  Estado                              Significado
  ----------------------------------- -----------------------------------
  `OK`                                Inicialização ou leitura válida
                                      confirmada

  `ATENÇÃO`                           Leitura suspeita, incompleta ou
                                      fora da faixa esperada

  `FALHA`                             Erro de inicialização ou
                                      comunicação confirmado

  `AQUECENDO`                         Sensor ainda em período de
                                      estabilização

  `NÃO TESTADO`                       O funcionamento físico depende de
                                      uma ação do usuário

  `N/D`                               Não disponível nesta configuração
  -----------------------------------------------------------------------

### 8.2 Testes por componente

  -----------------------------------------------------------------------
  Componente              Verificação no boot     Limitação do
                                                  diagnóstico
  ----------------------- ----------------------- -----------------------
  OLED                    Inicializar I2C e       Se o OLED falhar,
                          localizar o endereço do informar o erro pelo
                          display, normalmente    Serial Monitor
                          0x3C ou 0x3D            

  DHT22                   Tentar obter leitura    Uma leitura inválida
                          válida de temperatura e pode indicar ligação,
                          umidade                 temporização ou sensor

  LDR ambiente            Ler várias amostras ADC Não confirma calibração
                          e verificar se não está nem mede lux
                          permanentemente em um   
                          extremo                 

  LDR local               Ler várias amostras ADC Não confirma calibração
                          e verificar             
                          plausibilidade          

  HC-SR04                 Executar tentativas de  Sem objeto à frente,
                          medição com timeout     ausência de eco não
                                                  prova defeito

  Sensor IR               Ler a saída e solicitar Um nível estático não
                          teste manual passando   confirma funcionamento
                          um objeto à frente      

  INMP441                 Inicializar I2S e       Silêncio não significa
                          confirmar recebimento   defeito; validar
                          de amostras             comunicação, não
                                                  presença de som

  MQ-135                  Confirmar leitura ADC   Não comprova calibração
                          plausível e iniciar     ou precisão química
                          temporizador de         
                          aquecimento             

  LEDs amarelos           Fazer teste visual      O ESP32 não consegue
                          sequencial, acendendo   confirmar eletricamente
                          um por vez              se o LED acendeu sem
                                                  circuito de
                                                  realimentação

  RGB                     Acender o canal verde   Verificação visual, sem
                          por curto período       retorno elétrico

  Wi-Fi                   Tentar conexão por      Falha de rede não deve
                          tempo limitado, sem     ser tratada como falha
                          bloquear o sistema      da iluminação

  MQTT                    Tentar conexão somente  Broker indisponível não
                          se configurado          deve impedir o modo
                                                  local
  -----------------------------------------------------------------------

### 8.3 Testes manuais orientados pelo OLED

Alguns componentes não permitem autodiagnóstico conclusivo somente por
software. Para melhorar a verificação, o OLED deve orientar o usuário:

1.  **HC-SR04:** exibir `Aproxime objeto 10-50cm` durante uma janela de
    teste. Se houver eco válido, marcar `OK`; caso contrário,
    `ATENÇÃO: SEM ECO`.
2.  **Sensor IR:** exibir `Passe objeto no IR`. Se a entrada mudar
    durante a janela, marcar `OK`; caso contrário, `NÃO TESTADO` ou
    `ATENÇÃO`.
3.  **LEDs:** acender cada LED em sequência e exibir `Verifique LEDs`. O
    resultado deve ser identificado como teste visual, não como
    confirmação elétrica.

A rotina não deve ficar presa esperando indefinidamente uma ação. Após
um tempo limite, deve continuar a inicialização e manter o alerta
correspondente.

### 8.4 Regras para não inventar diagnósticos

O firmware não deve marcar automaticamente como `OK` um componente cuja
presença física não possa ser comprovada.

Exemplos:

-   Um HC-SR04 sem eco pode estar sem objeto à frente.
-   Um microfone sem variação de amplitude pode estar em ambiente
    silencioso.
-   Um sensor IR com saída estática pode estar sem obstáculo.
-   Um LED comandado por GPIO pode estar queimado ou desconectado, mesmo
    que o comando tenha sido enviado.

Nesses casos, utilizar `NÃO TESTADO`, `ATENÇÃO` ou uma descrição
equivalente. Diferenciar sempre **comunicação/inicialização confirmada**
de **funcionamento físico comprovado**.

### 8.5 Resultado do diagnóstico

Ao final, apresentar um resumo:

-   `SISTEMA NORMAL`: periféricos essenciais disponíveis.
-   `SISTEMA COM ALERTAS`: um ou mais sensores não críticos estão
    indisponíveis ou precisam de teste.
-   `MODO SEGURO`: falha no LDR ambiente ou nos dois sensores de
    presença, com iluminação ajustada conforme as regras de segurança.

A inicialização deve continuar mesmo com falhas não críticas.

------------------------------------------------------------------------

## 9. Interface do OLED

O display OLED de 0,96" deve ser simples, legível e organizado. Evitar
textos longos.

### 9.1 Tela de inicialização

``` text
ILUMINACAO IOT
Inicializando...
ESP32: OK
```

### 9.2 Tela de diagnóstico

Como o display é pequeno, dividir os resultados em páginas:

``` text
DIAGNOSTICO 1/2
OLED       OK
DHT22      OK
LDR AMB.   OK
LDR LOCAL  OK
HC-SR04    TESTE
```

``` text
DIAGNOSTICO 2/2
IR         ATENCAO
INMP441    OK
MQ-135     AQUEC.
LEDS       TESTE VISUAL
WIFI       LOCAL
```

Os valores são exemplos. O firmware deve mostrar os resultados reais.

### 9.3 Painel de operação

Alternar páginas a cada poucos segundos, sem bloquear o loop:

``` text
ILUMINACAO: NOITE
LEDS: 25%
PRESENCA: NAO
MODO: ECONOMIA
```

``` text
AMBIENTE
TEMP: 25.4 C
UMID: 61%
GAS ADC: 1420
```

``` text
MONITORAMENTO
SOM RMS: 128
LDR AMB: 1830
LDR LOCAL: 1710
REDE: OFFLINE
```

O painel deve exibir alertas relevantes sem ocultar permanentemente as
informações de operação.

------------------------------------------------------------------------

## 10. Indicador RGB

Na montagem inicial, somente o canal verde estará conectado.

-   Verde aceso: firmware em execução.
-   Verde piscando brevemente: atividade ou inicialização, se desejado.
-   Verde apagado: sistema não sinaliza funcionamento.

Como somente o canal verde será utilizado, alertas e falhas devem ser
comunicados pelo OLED e pelo Serial Monitor. Não simular estados
vermelho/amarelo no código enquanto esses canais não estiverem ligados.

------------------------------------------------------------------------

## 11. Conectividade IoT

O ESP32 possui Wi-Fi integrado. A conectividade deve ser opcional e não
pode bloquear as funções locais.

### 11.1 Wi-Fi

-   SSID e senha devem ficar em arquivo local de configuração, fora do
    controle de versão.
-   O firmware deve tentar conectar por um período limitado.
-   Se não conectar, mudar para `MODO LOCAL`.
-   Tentar reconectar periodicamente sem bloquear o loop.
-   Nunca imprimir senha no Serial Monitor.

### 11.2 MQTT opcional

Preparar uma camada MQTT configurável. Não assumir um broker específico.

Configurações necessárias:

``` cpp
WIFI_SSID
WIFI_PASSWORD
MQTT_ENABLED
MQTT_HOST
MQTT_PORT
MQTT_USERNAME
MQTT_PASSWORD
DEVICE_ID
```

Se `MQTT_ENABLED` estiver falso ou o host não estiver configurado, o
sistema deve funcionar sem MQTT.

Tópico sugerido:

``` text
public-lighting/{DEVICE_ID}/telemetry
public-lighting/{DEVICE_ID}/status
public-lighting/{DEVICE_ID}/availability
```

Exemplo de telemetria JSON:

``` json
{
  "uptime_ms": 125000,
  "ambient_adc": 1830,
  "local_ldr_adc": 1710,
  "is_night": true,
  "presence": false,
  "distance_cm": null,
  "ir_detected": false,
  "lighting_percent": 25,
  "temperature_c": 25.4,
  "humidity_percent": 61.0,
  "mq135_adc": 1420,
  "noise_rms": 128,
  "mode": "ECONOMY"
}
```

Os campos devem refletir os valores realmente disponíveis. Se um sensor
estiver indisponível, enviar `null` ou um estado de saúde
correspondente, nunca inventar um valor.

Não implementar comandos remotos de acionamento nesta primeira etapa.
Primeiro estabilizar leitura, controle local e publicação de telemetria.

------------------------------------------------------------------------

## 12. Arquitetura do firmware

### 12.1 Plataforma recomendada

-   Linguagem: C++.
-   Framework: Arduino para ESP32.
-   Build: PlatformIO, caso o repositório ainda não tenha uma estrutura
    definida.
-   Comunicação serial: 115200 baud.
-   Barramento dos sensores: GPIO, ADC, I2C e I2S.
-   Dependências devem ser declaradas e ter versões compatíveis entre
    si.

Se já existir um projeto ou estrutura de firmware no repositório, Claude
Code deve avaliá-lo antes de criar outra estrutura ou substituir
dependências.

### 12.2 Organização sugerida

``` text
project/
├── platformio.ini
├── README.md
├── include/
│   ├── AppConfig.h
│   ├── PinMap.h
│   ├── SensorManager.h
│   ├── Diagnostics.h
│   ├── LightingController.h
│   ├── DisplayManager.h
│   └── ConnectivityManager.h
├── src/
│   ├── main.cpp
│   ├── SensorManager.cpp
│   ├── Diagnostics.cpp
│   ├── LightingController.cpp
│   ├── DisplayManager.cpp
│   └── ConnectivityManager.cpp
└── secrets.example.h
```

O arquivo real de credenciais deve ser ignorado pelo Git. Não versionar
senhas ou tokens.

### 12.3 Responsabilidade dos módulos

**`SensorManager`** - Inicializar sensores. - Fazer leituras com
intervalos apropriados. - Armazenar a última leitura e seu timestamp. -
Informar se a leitura é válida, antiga ou indisponível. - Não tomar
decisões de iluminação.

**`Diagnostics`** - Executar testes de inicialização. - Manter o estado
de saúde de cada componente. - Diferenciar erro confirmado de teste
inconclusivo. - Gerar um resumo geral do sistema.

**`LightingController`** - Receber luminosidade e presença. - Aplicar
histerese. - Controlar PWM dos quatro LEDs. - Aplicar temporizador de
presença. - Aplicar as regras de modo seguro.

**`DisplayManager`** - Exibir inicialização, diagnóstico, alertas e
painel. - Alternar páginas sem bloquear a execução. - Não fazer leitura
direta dos sensores.

**`ConnectivityManager`** - Gerenciar Wi-Fi e MQTT. - Publicar
telemetria. - Reconectar sem bloquear o controle local. - Não controlar
diretamente os LEDs.

**`main.cpp`** - Inicializar os módulos. - Coordenar o ciclo
principal. - Executar tarefas periódicas. - Evitar concentrar toda a
lógica em um único arquivo.

### 12.4 Regras de implementação

-   Preferir temporizadores com `millis()` a chamadas longas de
    `delay()`.
-   Não fazer reconexões de rede bloqueantes.
-   Respeitar o intervalo mínimo de leitura do DHT22.
-   Usar timeout em toda medição do HC-SR04.
-   Filtrar leituras ruidosas com média móvel ou mediana quando
    apropriado.
-   Aplicar histerese aos limites de luminosidade e, se necessário, aos
    sensores de presença.
-   Manter valores de configuração centralizados.
-   Não espalhar números de GPIO pelo código: utilizar `PinMap.h`.
-   Não atualizar o OLED em todas as iterações do loop; atualizar em
    intervalos controlados.
-   Evitar alocação dinâmica frequente e uso excessivo de `String` em
    tarefas contínuas.
-   Registrar erros de forma objetiva no Serial Monitor.
-   O loop principal deve continuar responsivo mesmo quando um sensor
    falhar.

------------------------------------------------------------------------

## 13. Parâmetros iniciais configuráveis

Centralizar os parâmetros em um único arquivo:

``` cpp
// Iluminação
LIGHT_OFF_PERCENT = 0;
LIGHT_STANDBY_PERCENT = 25;
LIGHT_ACTIVE_PERCENT = 100;
PRESENCE_HOLD_MS = 10000;

// Luminosidade — valores iniciais, sujeitos a calibração
DAY_THRESHOLD = 2200;
NIGHT_THRESHOLD = 1800;

// HC-SR04
MIN_DISTANCE_CM = 5;
MAX_DISTANCE_CM = 120;
ULTRASONIC_TIMEOUT_US = 25000;

// Intervalos
DHT_INTERVAL_MS = 2000;
LDR_INTERVAL_MS = 250;
ULTRASONIC_INTERVAL_MS = 100;
MQ135_INTERVAL_MS = 1000;
DISPLAY_INTERVAL_MS = 250;
TELEMETRY_INTERVAL_MS = 15000;

// Rede
WIFI_CONNECT_TIMEOUT_MS = 10000;
```

A sintaxe acima é ilustrativa. Claude Code deve transformar os
parâmetros em constantes ou uma estrutura de configuração C++ válida.

O tempo de estabilização do MQ-135 deve ser configurável. O sensor pode
precisar de um período prolongado de pré-aquecimento, especialmente no
primeiro uso. Durante a estabilização, exibir `AQUECENDO` e não
classificar a leitura como medida confiável.

------------------------------------------------------------------------

## 14. Plano de implementação

Implementar em etapas pequenas. Não desenvolver todos os sensores e a
conectividade simultaneamente.

### Etapa 1 --- Auditoria e estrutura

-   Inspecionar o repositório existente.
-   Identificar placa, framework, bibliotecas e estrutura atual.
-   Confirmar se o projeto compila antes das alterações.
-   Registrar dúvidas de hardware e incompatibilidades.
-   Criar ou ajustar a estrutura modular.

**Entrega:** projeto mínimo compilando e documentação de pinagem.

### Etapa 2 --- ESP32 e OLED

-   Inicializar Serial.
-   Inicializar I2C.
-   Detectar o OLED.
-   Mostrar tela de boot.
-   Implementar atualização de páginas.

**Entrega:** OLED funcionando e diagnóstico básico.

### Etapa 3 --- Diagnóstico e LEDs

-   Configurar quatro LEDs amarelos com PWM.
-   Configurar o canal verde do RGB.
-   Executar teste visual sequencial.
-   Criar estrutura comum de estados de saúde.

**Entrega:** sequência de inicialização e tela de diagnóstico.

### Etapa 4 --- Sensores de luminosidade e iluminação

-   Implementar os dois LDRs.
-   Exibir valores ADC.
-   Calibrar os limites dia/noite.
-   Implementar histerese.
-   Implementar os níveis de iluminação.

**Entrega:** controle automático de iluminação sem sensores de presença.

### Etapa 5 --- Presença

-   Implementar HC-SR04 com timeout.
-   Implementar sensor IR.
-   Confirmar polaridade do IR.
-   Combinar as duas detecções.
-   Implementar temporizador de iluminação intensa.

**Entrega:** iluminação reage à presença.

### Etapa 6 --- Estação ambiental

-   Implementar DHT22.
-   Implementar leitura analógica do MQ-135.
-   Implementar I2S do INMP441.
-   Exibir os dados no OLED e Serial.
-   Aplicar estados de aquecimento e indisponibilidade.

**Entrega:** monitoramento ambiental local.

### Etapa 7 --- Wi-Fi e MQTT

-   Implementar configuração segura de credenciais.
-   Conectar sem bloquear o controle.
-   Publicar telemetria.
-   Tratar desconexões e reconexões.

**Entrega:** telemetria opcional funcionando.

### Etapa 8 --- Estabilização

-   Testar inicializações com sensores desconectados.
-   Testar ausência de eco.
-   Testar leituras fora da faixa.
-   Testar queda de Wi-Fi e MQTT.
-   Testar funcionamento prolongado.
-   Corrigir bloqueios, reinicializações e estados inconsistentes.

**Entrega:** firmware estável e documentação de operação.

------------------------------------------------------------------------

## 15. Plano de testes e critérios de aceite

  -----------------------------------------------------------------------
  Teste                   Procedimento            Resultado esperado
  ----------------------- ----------------------- -----------------------
  Inicialização normal    Ligar com todos os      OLED mostra diagnóstico
                          componentes conectados  e painel

  OLED desconectado       Ligar sem o OLED        Firmware continua; erro
                                                  aparece no Serial

  DHT22 desconectado      Ligar sem o sensor      Estado de falha/alerta;
                                                  iluminação continua

  LDR ambiente coberto    Reduzir a luz sobre o   Sistema muda para noite
                          LDR                     após o limite
                                                  configurado

  LDR ambiente iluminado  Iluminar o LDR          Sistema muda para dia
                                                  após o limite
                                                  configurado

  Histerese               Variar luz próximo ao   Estado não oscila
                          limite                  rapidamente

  Presença HC-SR04        Aproximar objeto dentro LEDs passam para
                          do limite               intensidade alta à
                                                  noite

  Presença IR             Passar objeto na frente Detecção é registrada
                          do módulo               conforme a polaridade
                                                  configurada

  Fim da presença         Aguardar o temporizador LEDs retornam ao nível
                                                  reduzido

  MQ-135                  Ligar e aguardar        Estado `AQUECENDO`,
                          estabilização           depois leitura relativa

  INMP441                 Produzir som próximo ao RMS relativo varia
                          microfone               

  Falha dos sensores de   Desconectar os dois     Modo seguro à noite
  presença                sensores                

  Wi-Fi indisponível      Desligar o roteador     Controle local continua

  MQTT indisponível       Desligar o broker       Controle local continua
                                                  e reconexão é tentada

  Funcionamento contínuo  Deixar ligado por       Sem travamentos ou
                          período prolongado      reinicializações
                                                  inesperadas
  -----------------------------------------------------------------------

### Critérios mínimos para considerar a primeira versão concluída

-   O firmware compila e inicia de forma consistente.
-   O OLED apresenta o resultado do diagnóstico.
-   Falhas de sensores são identificadas sem bloquear o sistema.
-   A luminosidade ambiente controla o estado dia/noite.
-   Os quatro LEDs respondem aos níveis de iluminação.
-   A detecção de presença eleva temporariamente a intensidade.
-   DHT22, MQ-135 e INMP441 apresentam leituras ou estados de
    indisponibilidade honestos.
-   A perda de Wi-Fi não interrompe o controle.
-   O código está organizado em módulos e possui instruções de montagem
    e teste.

------------------------------------------------------------------------

## 16. Segurança e limitações

1.  O protótipo deve operar em baixa tensão. Não conectar diretamente à
    rede elétrica ou a luminárias públicas reais.
2.  Nunca aplicar 5 V a GPIOs do ESP32.
3.  Usar divisor de tensão no ECHO do HC-SR04 e no AO do MQ-135.
4.  Utilizar resistor em série com cada LED.
5.  Compartilhar GND entre os módulos e fontes.
6.  Conferir polaridade, tensão e pinagem de cada módulo antes de
    energizar.
7.  O MQ-135 aquece durante a operação; manter ventilação e evitar
    contato com a parte metálica.
8.  Não utilizar valores brutos de MQ-135 como ppm sem calibração
    apropriada.
9.  Não apresentar o valor do LDR como lux sem calibração.
10. Não apresentar o RMS do INMP441 como dB SPL calibrado.
11. O protótipo não comprova, sozinho, redução de 35% no consumo ou
    melhoria de 50% na manutenção.
12. O teste de GPIO não confirma eletricamente que um LED está aceso;
    isso exigiria circuito de realimentação adicional.

------------------------------------------------------------------------

## 17. Instruções para Claude Code

Use este documento como especificação funcional e técnica do firmware.

**Antes de escrever ou alterar código:**

1.  Inspecione todo o repositório e identifique a estrutura, framework,
    dependências e estado atual da aplicação.
2.  Verifique se já existe firmware ou código reutilizável. Não
    substitua a estrutura existente sem justificar tecnicamente.
3.  Compare a pinagem documentada com a placa ESP32 real e sinalize
    qualquer incompatibilidade antes de assumir que a ligação está
    correta.
4.  Confira as bibliotecas disponíveis e selecione versões compatíveis
    com o framework encontrado.
5.  Identifique os pontos que dependem de confirmação física, como
    controlador do OLED, polaridade do RGB, polaridade do sensor IR e
    pinagem do MQ-135.

**Durante a implementação:**

-   Siga os requisitos funcionais e as regras de segurança deste
    documento.
-   Priorize o diagnóstico de inicialização: o OLED deve mostrar o
    estado dos componentes antes do painel normal.
-   Diferencie falha confirmada, leitura inválida, ausência de evento e
    teste manual inconclusivo.
-   Não marque sensores como saudáveis apenas porque um GPIO retornou
    HIGH ou LOW.
-   Mantenha o funcionamento local independente de Wi-Fi e MQTT.
-   Implemente e compile uma etapa por vez.
-   Não crie funcionalidades que dependam de hardware não listado sem
    registrar a necessidade.
-   Não invente valores de calibração. Exponha os parâmetros e explique
    como calibrá-los.
-   Não grave nem transmita áudio bruto do INMP441.
-   Não declare ppm, lux ou dB SPL como medições precisas sem
    calibração.
-   Adicione logs úteis, tratamento de timeout e estados de erro.
-   Atualize o README com pinagem, alimentação, bibliotecas, compilação,
    gravação e procedimentos de teste.

**Ao finalizar cada etapa, informe:**

1.  Arquivos criados ou alterados.
2.  Funcionalidades implementadas.
3.  Resultado da compilação.
4.  Testes executados e seus resultados.
5.  Limitações ou validações que ainda dependem da montagem física.

**Não considere o projeto concluído apenas porque o código compila.** A
conclusão depende também dos testes com os componentes reais,
especialmente os testes de diagnóstico, proteção das entradas de 5 V,
controle de iluminação e comportamento em caso de falhas.

------------------------------------------------------------------------

## 18. Pendências de confirmação física

Antes da montagem definitiva, confirmar:

-   [ ] Modelo exato do ESP32 DevKit e módulo utilizado.
-   [ ] Controlador e endereço I2C do OLED.
-   [ ] Se o DHT22 é um módulo com pull-up integrado ou sensor sem
    placa.
-   [ ] Polaridade comum do LED RGB: cátodo ou ânodo.
-   [ ] Polaridade de detecção do sensor IR.
-   [ ] Pinagem identificada no módulo MQ-135 (`VCC`, `GND`, `AO`,
    `DO`).
-   [ ] Disponibilidade dos resistores necessários.
-   [ ] Fonte de 5 V com corrente suficiente para os módulos.
-   [ ] GND comum entre ESP32 e alimentação externa dos sensores.

**Regra final:** se uma ligação real divergir deste documento, a
segurança elétrica e a pinagem impressa na placa prevalecem. Atualize a
documentação e o mapa de pinos antes de energizar o circuito.
