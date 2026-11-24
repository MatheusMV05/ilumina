import { useEffect, useRef, useState } from "react";

import { DEMO, demoCheckLamps, demoConfirm, demoSetLamp, demoStartTest, demoState, demoStreet } from "./demo";
import { SENSORS, SENSOR_ORDER, indexByCode } from "./sensors";
import type { ByCode, Code, DashboardState, EnergyTally, Health, HistoryPoint, SensorCode, LampControl, SparkPoint, StreetStats } from "./types";

export const POLL_MS = 1000;
export const HISTORY_SECONDS = 300;
export const SPARK_SECONDS = 120;

export type LinkStatus = { kind: "connecting" } | { kind: "live" } | { kind: "stale"; ageMs: number };

export interface Telemetry {
  state: DashboardState | null;
  byCode: ByCode | null;
  link: LinkStatus;
  // Séries mantidas fora do estado React: mudam a cada amostra junto com `state`
  history: HistoryPoint[];
  spark: Partial<Record<SensorCode, SparkPoint[]>>;
  energy: EnergyTally;
}

async function fetchState(): Promise<DashboardState> {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 2500);
  try {
    const res = await fetch("/api/state", { signal: ctrl.signal, cache: "no-store" });
    if (!res.ok) throw new Error(String(res.status));
    return (await res.json()) as DashboardState;
  } finally {
    clearTimeout(timer);
  }
}

function trim<T extends { t: number }>(series: T[], seconds: number, now: number) {
  const cutoff = now - seconds * 1000;
  while (series.length && series[0].t < cutoff) series.shift();
}

export function useTelemetry(): Telemetry {
  const history = useRef<HistoryPoint[]>([]);
  const spark = useRef<Partial<Record<SensorCode, SparkPoint[]>>>({});
  const lastOk = useRef(0);
  const energy = useRef<EnergyTally>({ since: Date.now(), used: 0, baseline: 0 });
  const [snapshot, setSnapshot] = useState<{ state: DashboardState | null; byCode: ByCode | null; link: LinkStatus }>({
    state: null, byCode: null, link: { kind: "connecting" },
  });

  useEffect(() => {
    let timer: ReturnType<typeof setTimeout>;
    let cancelled = false;

    const record = (d: DashboardState, byCode: ByCode) => {
      const now = Date.now();
      history.current.push({
        t: now,
        ambient: byCode.LDR_AMBIENT.data.value,
        period: d.lighting.is_night === null ? "sem leitura" : d.lighting.is_night ? "noite" : "dia",
        presence: d.lighting.presence_hc || d.lighting.presence_ir,
        light: d.lighting.zones[0],
        mode: d.lighting.mode,
        lampLdr: byCode.LDR_LOCAL.link?.state === "CONNECTED" ? byCode.LDR_LOCAL.data.value : null,
        distance: byCode.ULTRASONIC.link?.state === "CONNECTED" ? byCode.ULTRASONIC.data.distance_cm : null,
        inRange: d.lighting.presence_hc,
        health: Object.fromEntries(d.components.map((c) => [c.code, c.health])) as Partial<Record<Code, Health>>,
      });

      // Base de comparação: poste comum com fotocélula, 100% à noite e apagado de dia.
      // Sem leitura de luz o modo seguro também acende tudo, então conta como noite
      const avgZone = d.lighting.zones.reduce((a, b) => a + b, 0) / d.lighting.zones.length;
      energy.current.used += avgZone;
      energy.current.baseline += d.lighting.is_night === false ? 0 : 100;
      trim(history.current, HISTORY_SECONDS, now);

      for (const code of SENSOR_ORDER) {
        const c = byCode[code];
        const series = (spark.current[code] ??= []);
        const connected = c.link?.state === "CONNECTED";
        // Cada sensor tem seu próprio tipo de dado; o meta correspondente sabe extrair o valor
        const value = connected ? (SENSORS[code].spark as (x: typeof c) => number | null)(c) : null;
        series.push({ t: now, v: value });
        trim(series, SPARK_SECONDS, now);
      }
    };

    const poll = async () => {
      let next: DashboardState | null = null;
      try {
        next = DEMO ? demoState() : await fetchState();
        lastOk.current = Date.now();
      } catch {
        // Mantém a última leitura na tela; o selo de conexão indica o problema
      }
      if (cancelled) return;

      const age = Date.now() - lastOk.current;
      const link: LinkStatus = !lastOk.current ? { kind: "connecting" }
        : age < 3 * POLL_MS ? { kind: "live" } : { kind: "stale", ageMs: age };

      if (next) {
        const byCode = indexByCode(next);
        record(next, byCode);
        setSnapshot({ state: next, byCode, link });
      } else {
        setSnapshot((prev) => ({ ...prev, link }));
      }
      timer = setTimeout(poll, POLL_MS);
    };

    poll();
    return () => {
      cancelled = true;
      clearTimeout(timer);
    };
  }, []);

  return { ...snapshot, history: history.current, spark: spark.current, energy: energy.current };
}

export async function startTest(code: Code) {
  if (DEMO) return demoStartTest(code);
  await fetch(`/api/test?c=${encodeURIComponent(code)}`, { method: "POST" }).catch(() => {});
}

export async function confirmVisual(code: Code) {
  if (DEMO) return demoConfirm(code);
  await fetch(`/api/confirm?c=${encodeURIComponent(code)}`, { method: "POST" }).catch(() => {});
}

export const STREET_POLL_MS = 10000;
// Diferença tolerada entre o relógio do poste e o do navegador antes de corrigir
const CLOCK_DRIFT_MS = 120000;

async function syncClock() {
  const tz = -new Date().getTimezoneOffset();
  await fetch(`/api/time?ms=${Date.now()}&tz=${tz}`, { method: "POST" }).catch(() => {});
}

// Estatísticas por hora do dia, guardadas no ESP32. Mudam devagar, então a consulta é espaçada.
// O poste não tem relógio próprio: se a hora dele estiver errada, o painel manda a do navegador
export function useStreet() {
  const [street, setStreet] = useState<StreetStats | null>(null);
  const [tick, setTick] = useState(0);

  useEffect(() => {
    let timer: ReturnType<typeof setTimeout>;
    let cancelled = false;
    const poll = async () => {
      let delay = STREET_POLL_MS;
      try {
        const d = DEMO ? demoStreet() : ((await (await fetch("/api/street", { cache: "no-store" })).json()) as StreetStats);
        if (cancelled) return;
        // Logo após o boot o firmware ainda não montou o JSON ("{}")
        if (Array.isArray(d.hours)) {
          setStreet(d);
          const tz = -new Date().getTimezoneOffset();
          const drift = d.epoch === null ? Infinity : Math.abs(d.epoch * 1000 - Date.now());
          if (!DEMO && (!d.clock_valid || drift > CLOCK_DRIFT_MS || d.tz_min !== tz)) {
            await syncClock();
            delay = 6000;  // o firmware remonta o JSON a cada 5 s
          }
        }
      } catch {
        // Sem resposta: tenta de novo no próximo ciclo
      }
      if (!cancelled) timer = setTimeout(poll, delay);
    };
    poll();
    return () => {
      cancelled = true;
      clearTimeout(timer);
    };
  }, [tick]);

  const reset = async () => {
    if (!DEMO) await fetch("/api/street/reset", { method: "POST" }).catch(() => {});
    setTimeout(() => setTick((t) => t + 1), 6000);
  };

  return { street, reset };
}

// Comando de uma luminária (zone de 0 a 3). Ligada usa o brilho escolhido; AUTO volta à lógica
export async function setLamp(zone: number, control: LampControl, percent?: number) {
  if (DEMO) return demoSetLamp(zone, control, percent);
  const pct = percent === undefined ? "" : `&pct=${Math.round(percent)}`;
  await fetch(`/api/lamp?z=${zone + 1}&mode=${control.toLowerCase()}${pct}`, { method: "POST" }).catch(() => {});
}

// Pisca um LED por vez (~3 s no total) para ver quais estão de fato no circuito
export async function checkLamps() {
  if (DEMO) return demoCheckLamps();
  await fetch("/api/lamps/check", { method: "POST" }).catch(() => {});
}
