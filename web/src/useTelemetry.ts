import { useEffect, useRef, useState } from "react";

import { DEMO, demoConfirm, demoStartTest, demoState } from "./demo";
import { SENSORS, SENSOR_ORDER, indexByCode } from "./sensors";
import type { ByCode, Code, DashboardState, HistoryPoint, SensorCode, SparkPoint } from "./types";

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
      });
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

  return { ...snapshot, history: history.current, spark: spark.current };
}

export async function startTest(code: Code) {
  if (DEMO) return demoStartTest(code);
  await fetch(`/api/test?c=${encodeURIComponent(code)}`, { method: "POST" }).catch(() => {});
}

export async function confirmVisual(code: Code) {
  if (DEMO) return demoConfirm(code);
  await fetch(`/api/confirm?c=${encodeURIComponent(code)}`, { method: "POST" }).catch(() => {});
}
