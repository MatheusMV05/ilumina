import type { Icon } from "@phosphor-icons/react";
import {
  CheckIcon,
  CircleHalfIcon,
  EyeIcon,
  LampIcon,
  MicrophoneIcon,
  MinusIcon,
  QuestionIcon,
  RulerIcon,
  ScanIcon,
  SunHorizonIcon,
  ThermometerSimpleIcon,
  WarningIcon,
  WindIcon,
  XIcon,
} from "@phosphor-icons/react";

import { EMPTY, fmtDur, fmtN } from "./format";
import type { ByCode, ComponentState, DashboardState, Health, Mode, SensorCode, TestState } from "./types";

export type Tone = "good" | "warning" | "serious" | "critical" | "neutral" | "busy";
export interface StatusSpec {
  tone: Tone;
  icon: Icon;
  text: string;
}

// Selo de estado: ícone + texto, nunca só cor
export const HEALTH: Record<Health, StatusSpec> = {
  OK: { tone: "good", icon: CheckIcon, text: "Funcionando" },
  WARNING: { tone: "warning", icon: WarningIcon, text: "Atenção" },
  FAILURE: { tone: "critical", icon: XIcon, text: "Não conectado" },
  WARMING_UP: { tone: "serious", icon: CircleHalfIcon, text: "Aquecendo" },
  NOT_TESTED: { tone: "neutral", icon: QuestionIcon, text: "Falta testar" },
  NOT_AVAILABLE: { tone: "neutral", icon: MinusIcon, text: "Desativado" },
  VISUAL_CHECK: { tone: "neutral", icon: EyeIcon, text: "Confirme" },
};

export const MODE_TEXT: Record<Mode, string> = {
  DAY_OFF: "Desligado (dia)",
  ECONOMY: "Economia",
  ACTIVE: "Presença",
  SAFE: "Modo seguro",
};

type Main = [value: string | number | null, unit: string, meaning: string];
interface Check {
  ok: boolean | null;
  text: string;
}

export interface SensorMeta<C extends SensorCode> {
  title: string;
  part: string;
  pins: string;
  role: string;
  lighting: boolean;
  icon: Icon;
  wiring: string;
  instruction: string;
  unit: string;
  // Valor do mini-gráfico; IR vira degrau 0/1
  spark: (c: ComponentState<C>) => number | null;
  main: (c: ComponentState<C>, d: DashboardState) => Main;
  measuring: (c: ComponentState<C>) => Check;
  reaction: (t: TestState) => string;
  details: (c: ComponentState<C>) => [string, string | number][];
}

const ldrMeaning = (v: number | null, d: DashboardState) =>
  v === null ? "" : v > d.config.day_threshold ? "claro" : v < d.config.night_threshold ? "escuro" : "penumbra";

const ldr = <C extends "LDR_AMBIENT" | "LDR_LOCAL">(
  title: string, pins: string, role: string, lighting: boolean, icon: Icon, wiring: string,
): SensorMeta<C> => ({
  title, part: "LDR", pins, role, lighting, icon, wiring,
  instruction: "Cubra o LDR com o dedo (ou ilumine com a lanterna do celular).",
  unit: "ADC",
  spark: (c) => c.data.value,
  main: (c, d) => [c.data.value, "ADC", ldrMeaning(c.data.value, d)],
  measuring: (c) => ({ ok: true, text: `Leitura estável (variação de ${fmtN(c.data.noise, 1)} ADC)` }),
  reaction: (t) => `mudou ${fmtN(t.best)} de ${fmtN(t.target)} ADC necessários`,
  details: (c) => [["Leitura bruta", `${c.data.raw} ADC`], ["Ruído (desvio padrão)", `${fmtN(c.data.noise, 1)} ADC`]],
});

// Textos amigáveis por sensor
export const SENSORS: { [C in SensorCode]: SensorMeta<C> } = {
  LDR_AMBIENT: ldr("Luz ambiente", "GPIO 33", "Decide se é dia ou noite", true, SunHorizonIcon,
    "3V3 → LDR → GPIO 33 → resistor 10 kΩ → GND"),
  LDR_LOCAL: ldr("Luz no poste", "GPIO 32", "Encostado no LED 1: confere se ele acende", false, LampIcon,
    "3V3 → LDR → GPIO 32 → resistor 10 kΩ → GND, com o LDR encostado no LED do poste 1"),
  ULTRASONIC: {
    title: "Distância", part: "HC-SR04", pins: "TRIG 5, ECHO 18", role: "Detecta alguém à frente", lighting: true, icon: RulerIcon,
    wiring: "VCC no Vin (5 V), GND, TRIG → GPIO 5, ECHO → divisor (1 kΩ + 2 kΩ) → GPIO 18",
    instruction: "Coloque a mão a uns 20 cm na frente do sensor.",
    unit: "cm",
    spark: (c) => c.data.distance_cm,
    main: (c, d) => c.data.distance_cm === null ? [null, "", "nada à frente"] :
      [fmtN(c.data.distance_cm), "cm", c.data.in_range ? "presença na faixa" : `fora da faixa (${d.config.min_distance_cm} a ${d.config.max_distance_cm} cm)`],
    measuring: (c) => ({ ok: c.data.response_pct >= 50,
      text: `Respondeu em ${c.data.response_pct}% dos disparos, eco em ${c.data.echo_pct}%` }),
    reaction: (t) => t.best >= 9999 ? "nenhum objeto visto ainda" : `mais perto: ${fmtN(t.best)} cm (precisa de menos de ${fmtN(t.target)} cm)`,
    details: (c) => [
      ["Última medição", ({ VALID: "eco recebido", NO_ECHO: "sem objeto", NO_RESPONSE: "sem resposta", BUSY: "ECHO ocupado" } as Record<string, string>)[c.data.last] ?? c.data.last],
      ["Módulo respondeu", `${c.data.response_pct}%`],
      ["Com eco", `${c.data.echo_pct}%`],
    ],
  },
  IR: {
    title: "Obstáculo", part: "Sensor IR", pins: "GPIO 23", role: "Detecta alguém bem perto", lighting: true, icon: ScanIcon,
    wiring: "VCC 3V3, GND, OUT → GPIO 23 (ajuste a distância no parafuso azul)",
    instruction: "Passe a mão na frente do sensor, a poucos centímetros.",
    unit: "",
    spark: (c) => (c.link?.state === "CONNECTED" ? (c.data.detected ? 1 : 0) : null),
    main: (c) => [c.data.detected ? "Detectando" : "Livre", "",
      c.data.last_change_ago_ms === null ? "sem detecções ainda" : `última mudança há ${fmtDur(c.data.last_change_ago_ms)}`],
    measuring: (c) => c.data.changes > 0 ? { ok: true, text: `Já reagiu ${c.data.changes} vez(es)` }
      : { ok: null, text: "Ligado, mas ainda não detectou nada" },
    reaction: (t) => `${fmtN(t.best)} mudança(s) detectada(s)`,
    details: (c) => [["Mudanças desde que ligou", c.data.changes], ["Polaridade", c.data.active_low ? "LOW = detectou" : "HIGH = detectou"]],
  },
  DHT11: {
    title: "Temperatura e umidade", part: "DHT11", pins: "GPIO 4", role: "Monitora o ambiente", lighting: false, icon: ThermometerSimpleIcon,
    wiring: "VCC 3V3, DATA → GPIO 4, GND (sensor sem placa, 4 pernas: resistor 10 kΩ entre DATA e 3V3; a 3ª perna fica livre)",
    instruction: "Segure o sensor entre os dedos ou sopre de leve nele.",
    unit: "°C",
    spark: (c) => c.data.temperature_c,
    main: (c) => c.data.temperature_c === null ? [null, "", ""] :
      [fmtN(c.data.temperature_c, 1), "°C", `${fmtN(c.data.humidity_percent)}% de umidade, sensação de ${fmtN(c.data.heat_index_c, 1)} °C`],
    measuring: (c) => ({ ok: c.data.success_pct >= 70, text: `${c.data.success_pct}% das leituras válidas` }),
    reaction: (t) => `umidade subiu ${fmtN(t.best, 1)} de ${fmtN(t.target, 1)} pontos`,
    details: (c) => [["Leituras válidas", `${c.data.success_pct}%`], ["Última leitura", c.data.age_ms === null ? "nenhuma" : `há ${fmtDur(c.data.age_ms)}`]],
  },
  MQ135: {
    title: "Qualidade do ar", part: "MQ-135", pins: "GPIO 32", role: "Indicador relativo de gases (não é ppm)", lighting: false, icon: WindIcon,
    wiring: "VCC 5 V, GND, AO → divisor (10k + 10k) → GPIO 32",
    instruction: "Aproxime, sem encostar, um pouco de álcool em gel do sensor.",
    unit: "ADC",
    spark: (c) => c.data.adc,
    main: (c) => [c.data.adc, "ADC", c.data.warming ? `aquecendo, faltam ${fmtDur(c.data.warmup_left_ms)}` :
      ({ low: "ar: baixo", medium: "ar: médio", high: "ar: alto" }[c.data.level ?? "low"] ?? "")],
    measuring: (c) => c.data.warming ? { ok: null, text: `Aquecendo. Faltam ${fmtDur(c.data.warmup_left_ms)}` }
      : { ok: true, text: "Leitura estável" },
    reaction: (t) => `mudou ${fmtN(t.best)} de ${fmtN(t.target)} ADC necessários`,
    details: (c) => [["Leitura bruta", `${c.data.raw} ADC`]],
  },
  MIC: {
    title: "Som ambiente", part: "KY-038", pins: "AO 34, DO 35", role: "Nível de ruído relativo", lighting: false, icon: MicrophoneIcon,
    wiring: "+ → 3V3, G → GND, AO → GPIO 34, DO → GPIO 35 (o trimpot azul ajusta o limiar do DO)",
    instruction: "Bata palmas ou fale alto perto do microfone.",
    unit: "dB",
    spark: (c) => c.data.dbfs,
    main: (c) => {
      if (c.data.dbfs === null) return [null, "", ""];
      const above = c.data.dbfs - (c.data.floor_dbfs ?? c.data.dbfs);
      return [fmtN(c.data.dbfs), "dB rel.", above < 4 ? "silêncio" : above < 12 ? "som moderado" : "som alto"];
    },
    measuring: (c) => ({ ok: true, text: `Microfone polarizado em ${c.data.bias} ADC, comparador disparou ${c.data.do_events} vez(es)` }),
    reaction: (t) => `pico de ${fmtN(t.best)} dB acima do silêncio (precisa de ${fmtN(t.target)}) ou disparo do comparador`,
    details: (c) => [
      ["Pico recente", `${fmtN(c.data.peak_dbfs) ?? EMPTY} dB rel.`],
      ["Piso de ruído", `${fmtN(c.data.floor_dbfs) ?? EMPTY} dB rel.`],
      ["Variação (RMS)", `${fmtN(c.data.rms, 1) ?? EMPTY} ADC`],
      ["Nível DC da saída AO", `${c.data.bias} ADC`],
      ["Comparador (DO) agora", c.data.do_active ? "acima do limiar" : "abaixo do limiar"],
      ["Disparos do DO desde que ligou", c.data.do_events],
    ],
  },
};

export const SENSOR_ORDER: SensorCode[] = ["LDR_AMBIENT", "ULTRASONIC", "IR", "LDR_LOCAL", "DHT11", "MQ135", "MIC"];

// Por que o firmware acha que o sensor não está ligado
export function reasonText(code: SensorCode, reason: string | undefined): string {
  const map: Record<string, string> = {
    floating: code === "MIC" ? "A saída AO oscila como pino solto: o KY-038 parece desconectado."
      : "Nenhum sinal no pino: o sensor parece desconectado.",
    stuck_low: code === "MQ135" ? "Sem tensão na saída: módulo sem 5 V ou desligado."
      : code === "MIC" ? "Saída AO em 0: o KY-038 está sem 3V3?"
      : "Leitura quase 0: o pino só enxerga o resistor até o GND. O LDR pode estar fora da fileira do GPIO, sem 3V3 (trilho cortado no meio da protoboard?) ou no escuro total.",
    stuck_high: code === "MQ135" ? "Leitura no máximo: confira o divisor de tensão."
      : "Leitura no máximo: o pino recebe 3V3 direto. Falta o resistor de 10 kΩ até o GND na mesma fileira?",
    no_response: code === "DHT11" ? "O sensor não respondeu: confira DATA e alimentação."
      : "O módulo não respondeu ao disparo: confira 5 V, GND e TRIG.",
    echo_stuck: "O pino ECHO ficou preso em nível alto.",
    intermittent: "Responde só às vezes: mau contato ou fio no pino errado.",
  };
  return (reason && map[reason]) || "Verificando…";
}

export function indexByCode(d: DashboardState) {
  return Object.fromEntries(d.components.map((c) => [c.code, c])) as ByCode;
}
