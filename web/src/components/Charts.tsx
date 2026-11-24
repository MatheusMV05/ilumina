import { useRef } from "react";

import { clock, cssVar } from "../format";
import { HEALTH, type Tone } from "../sensors";
import type { Code, HistoryPoint, SparkPoint } from "../types";
import { HISTORY_SECONDS, SPARK_SECONDS } from "../useTelemetry";
import { useCanvas } from "./useCanvas";

// Os tokens de série são hex (#rrggbb); o canvas não aceita color-mix em todos os navegadores
function withAlpha(hex: string, alpha: number) {
  const n = parseInt(hex.slice(1), 16);
  return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${alpha})`;
}

// Linha com preenchimento em degradê até a base: o "brilho" Aero sem esconder o dado
function strokeSeries<T extends { t: number }>(
  ctx: CanvasRenderingContext2D, points: T[], value: (p: T) => number | null | undefined,
  x: (t: number) => number, y: (v: number) => number, baseY: number, color: string, step = false,
) {
  const runs: [number, number][][] = [];
  let run: [number, number][] = [];
  let prevY = 0;
  for (const p of points) {
    const v = value(p);
    if (v === null || v === undefined) {
      if (run.length) runs.push(run);
      run = [];
      continue;
    }
    if (step && run.length) run.push([x(p.t), prevY]);
    run.push([x(p.t), y(v)]);
    prevY = y(v);
  }
  if (run.length) runs.push(run);

  const fill = ctx.createLinearGradient(0, 0, 0, baseY);
  fill.addColorStop(0, withAlpha(color, 0.34));
  fill.addColorStop(1, withAlpha(color, 0));
  for (const r of runs) {
    ctx.beginPath();
    ctx.moveTo(r[0][0], baseY);
    for (const [px, py] of r) ctx.lineTo(px, py);
    ctx.lineTo(r[r.length - 1][0], baseY);
    ctx.closePath();
    ctx.fillStyle = fill;
    ctx.fill();
  }

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  for (const r of runs) {
    ctx.beginPath();
    r.forEach(([px, py], i) => (i ? ctx.lineTo(px, py) : ctx.moveTo(px, py)));
    ctx.stroke();
  }
}

export function Sparkline({ series, step, label }: { series: SparkPoint[]; step: boolean; label: string }) {
  const { ref } = useCanvas((ctx, w, h) => {
    ctx.strokeStyle = cssVar("--grid");
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, h - 0.5); ctx.lineTo(w, h - 0.5); ctx.stroke();

    const vals = series.map((p) => p.v).filter((v): v is number => v !== null);
    if (!vals.length) return;
    let lo = step ? 0 : Math.min(...vals);
    let hi = step ? 1 : Math.max(...vals);
    if (hi - lo < 1e-6) { lo -= 1; hi += 1; }
    const now = Date.now();
    const x = (t: number) => w - ((now - t) / (SPARK_SECONDS * 1000)) * w;
    const y = (v: number) => 4 + (h - 8) * (1 - (v - lo) / (hi - lo));
    strokeSeries(ctx, series, (p) => p.v, x, y, h, cssVar("--series-1"), step);
  });
  return <canvas ref={ref} className="spark" role="img" aria-label={label} />;
}

export interface LineChartProps {
  history: HistoryPoint[];
  min: number;
  max: number;
  ticks: number[];
  color: string;
  value: (p: HistoryPoint) => number | null;
  thresholds?: { v: number; label: string; above: boolean }[];
  tooltip: (p: HistoryPoint) => string;
  label: string;
  // Faixa colorida acima do gráfico (ex.: modo da luz), alinhada ao mesmo eixo de tempo
  band?: (p: HistoryPoint) => string;
  size?: "sm";
}

const BAND_H = 12;

const PAD = { l: 40, r: 8, t: 10, b: 10 };

// Um eixo por gráfico; o ponteiro mostra o valor mais próximo sem re-renderizar o React
export function LineChart(props: LineChartProps) {
  const hover = useRef<number | null>(null);
  const tip = useRef<HTMLDivElement>(null);

  const { ref, paint } = useCanvas((ctx, w, h) => {
    const { history, min, max, ticks, value, thresholds = [], tooltip, band } = props;
    const padT = band ? PAD.t + BAND_H + 8 : PAD.t;
    const pw = w - PAD.l - PAD.r;
    const ph = h - padT - PAD.b;
    const y = (v: number) => padT + ph - ((v - min) / (max - min)) * ph;
    const now = history.length ? history[history.length - 1].t : Date.now();
    const x = (t: number) => PAD.l + pw - ((now - t) / (HISTORY_SECONDS * 1000)) * pw;

    // Cada amostra pinta até a próxima; 2px de respiro entre trocas de cor
    if (band) {
      for (let i = 0; i < history.length; i++) {
        // Pixels inteiros: com frações o canvas deixa costuras claras entre amostras vizinhas
        const x0 = Math.round(Math.max(PAD.l, x(history[i].t)));
        const x1 = Math.round(i + 1 < history.length ? x(history[i + 1].t) : w - PAD.r);
        const next = history[i + 1];
        const gap = next && band(next) !== band(history[i]) ? 2 : 0;
        ctx.fillStyle = cssVar(band(history[i]));
        ctx.fillRect(x0, PAD.t, Math.max(0, x1 - x0 - gap), BAND_H);
      }
    }

    ctx.font = "11px 'Segoe UI', system-ui, sans-serif";
    ctx.fillStyle = cssVar("--text-muted");
    ctx.strokeStyle = cssVar("--grid");
    ctx.lineWidth = 1;
    ctx.textAlign = "right";
    ctx.textBaseline = "middle";
    for (const v of ticks) {
      ctx.beginPath(); ctx.moveTo(PAD.l, y(v)); ctx.lineTo(w - PAD.r, y(v)); ctx.stroke();
      ctx.fillText(String(v), PAD.l - 6, y(v));
    }
    for (const th of thresholds) {
      ctx.save();
      ctx.setLineDash([4, 4]);
      ctx.strokeStyle = cssVar("--text-muted");
      ctx.beginPath(); ctx.moveTo(PAD.l, y(th.v)); ctx.lineTo(w - PAD.r, y(th.v)); ctx.stroke();
      ctx.restore();
      ctx.textAlign = "left";
      ctx.textBaseline = th.above ? "bottom" : "top";
      ctx.fillStyle = cssVar("--text-secondary");
      ctx.fillText(th.label, PAD.l + 4, y(th.v) + (th.above ? -2 : 2));
    }

    const color = cssVar(props.color);
    strokeSeries(ctx, history, value, x, y, padT + ph, color);

    const tipEl = tip.current;
    if (!tipEl) return;
    const i = hover.current;
    if (i === null || !history[i]) {
      tipEl.hidden = true;
      return;
    }
    const p = history[i];
    const v = value(p);
    const px = x(p.t);
    ctx.strokeStyle = cssVar("--text-muted");
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(px, PAD.t); ctx.lineTo(px, padT + ph); ctx.stroke();
    if (v !== null) {
      ctx.fillStyle = color;
      ctx.strokeStyle = cssVar("--surface-solid");
      ctx.lineWidth = 2;
      ctx.beginPath(); ctx.arc(px, y(v), 4.5, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
    }
    tipEl.textContent = `${clock(p.t)}: ${tooltip(p)}`;
    tipEl.hidden = false;
    tipEl.style.left = `${Math.max(4, Math.min(px + 10, w - tipEl.offsetWidth - 4))}px`;
  });

  const onMove = (ev: React.PointerEvent<HTMLCanvasElement>) => {
    const { history } = props;
    if (!history.length) return;
    const rect = ev.currentTarget.getBoundingClientRect();
    const pw = rect.width - PAD.l - PAD.r;
    const now = history[history.length - 1].t;
    const t = now - ((rect.width - PAD.r - (ev.clientX - rect.left)) / pw) * HISTORY_SECONDS * 1000;
    let best = 0;
    for (let i = 1; i < history.length; i++) if (Math.abs(history[i].t - t) < Math.abs(history[best].t - t)) best = i;
    hover.current = best;
    paint();
  };
  const onLeave = () => {
    hover.current = null;
    paint();
  };

  return (
    <div className="chart-wrap">
      <canvas ref={ref} className={`chart${props.size ? " chart-sm" : ""}`} role="img" aria-label={props.label} onPointerMove={onMove} onPointerLeave={onLeave} />
      <div className="tooltip" ref={tip} hidden />
    </div>
  );
}

// Tempo em segundos atrás → posição; usado pelos gráficos sem linha
function timeX(history: { t: number }[], w: number) {
  const pw = w - PAD.l - PAD.r;
  const now = history.length ? history[history.length - 1].t : Date.now();
  return (t: number) => PAD.l + pw - ((now - t) / (HISTORY_SECONDS * 1000)) * pw;
}

function nearest(history: { t: number }[], rect: DOMRect, clientX: number) {
  if (!history.length) return null;
  const pw = rect.width - PAD.l - PAD.r;
  const now = history[history.length - 1].t;
  const t = now - ((rect.width - PAD.r - (clientX - rect.left)) / pw) * HISTORY_SECONDS * 1000;
  let best = 0;
  for (let i = 1; i < history.length; i++) if (Math.abs(history[i].t - t) < Math.abs(history[best].t - t)) best = i;
  return best;
}

// Distâncias medidas com a faixa que conta como presença sombreada: serve para calibrar o HC-SR04
export function DistanceChart({ history, minCm, maxCm }: { history: HistoryPoint[]; minCm: number; maxCm: number }) {
  const hover = useRef<number | null>(null);
  const tip = useRef<HTMLDivElement>(null);

  const { ref, paint } = useCanvas((ctx, w, h) => {
    const seen = history.map((p) => p.distance).filter((v): v is number => v !== null);
    const top = Math.min(400, Math.max(maxCm * 1.4, ...seen, 50));
    const ticks = [0, Math.round(top / 2 / 10) * 10, Math.round(top / 10) * 10];
    const ph = h - PAD.t - PAD.b;
    const y = (v: number) => PAD.t + ph - (Math.min(v, top) / top) * ph;
    const x = timeX(history, w);

    ctx.font = "11px 'Segoe UI', system-ui, sans-serif";
    ctx.textAlign = "right";
    ctx.textBaseline = "middle";
    ctx.fillStyle = cssVar("--accent-soft");
    ctx.fillRect(PAD.l, y(maxCm), w - PAD.l - PAD.r, y(minCm) - y(maxCm));
    ctx.strokeStyle = cssVar("--grid");
    ctx.lineWidth = 1;
    for (const v of ticks) {
      ctx.beginPath(); ctx.moveTo(PAD.l, y(v)); ctx.lineTo(w - PAD.r, y(v)); ctx.stroke();
      ctx.fillStyle = cssVar("--text-muted");
      ctx.fillText(`${v}`, PAD.l - 6, y(v));
    }
    ctx.textAlign = "left";
    ctx.textBaseline = "top";
    ctx.fillStyle = cssVar("--text-secondary");
    ctx.fillText(`conta como presença: ${minCm} a ${maxCm} cm`, PAD.l + 4, y(maxCm) + 3);

    // Dentro da faixa: ponto cheio na cor de destaque; fora: anel neutro
    const accent = cssVar("--series-1");
    const ring = cssVar("--surface-solid");
    history.forEach((p, i) => {
      if (p.distance === null) return;
      const r = hover.current === i ? 6 : 4;
      ctx.beginPath(); ctx.arc(x(p.t), y(p.distance), r, 0, Math.PI * 2);
      if (p.inRange) {
        ctx.fillStyle = accent; ctx.fill();
        ctx.strokeStyle = ring; ctx.lineWidth = 2; ctx.stroke();
      } else {
        ctx.strokeStyle = cssVar("--text-muted"); ctx.lineWidth = 1.5; ctx.stroke();
      }
    });

    const tipEl = tip.current;
    if (!tipEl) return;
    const i = hover.current;
    const p = i === null ? null : history[i];
    if (!p) { tipEl.hidden = true; return; }
    tipEl.textContent = `${clock(p.t)}: ${p.distance === null ? "nada à frente" : `${p.distance.toFixed(0)} cm${p.inRange ? ", presença" : ", fora da faixa"}`}`;
    tipEl.hidden = false;
    tipEl.style.left = `${Math.max(4, Math.min(x(p.t) + 10, w - tipEl.offsetWidth - 4))}px`;
  });

  return (
    <div className="chart-wrap">
      <canvas
        ref={ref} className="chart" role="img" aria-label="Distâncias medidas pelo HC-SR04 nos últimos 5 minutos"
        onPointerMove={(e) => { hover.current = nearest(history, e.currentTarget.getBoundingClientRect(), e.clientX); paint(); }}
        onPointerLeave={() => { hover.current = null; paint(); }}
      />
      <div className="tooltip" ref={tip} hidden />
    </div>
  );
}

export interface Lane {
  code: Code;
  title: string;
}

const TONE_VAR: Record<Tone, string> = {
  good: "--good", warning: "--warning", serious: "--serious", critical: "--critical", neutral: "--neutral", busy: "--busy",
};

// Uma faixa por componente, colorida pelo estado a cada segundo: mau contato aparece como listras
export function StatusLanes({ history, lanes }: { history: HistoryPoint[]; lanes: Lane[] }) {
  const hover = useRef<{ i: number; lane: number } | null>(null);
  const tip = useRef<HTMLDivElement>(null);
  const LANE = 16;
  const GAP = 6;
  // Coluna dos nomes medida a partir do texto, para nenhum nome ser cortado
  const leftRef = useRef(120);

  const { ref, paint } = useCanvas((ctx, w) => {
    ctx.font = "12px 'Segoe UI', system-ui, sans-serif";
    ctx.textBaseline = "middle";
    const left = Math.ceil(Math.max(...lanes.map((l) => ctx.measureText(l.title).width))) + 16;
    leftRef.current = left;
    const pw = w - left - PAD.r;
    const now = history.length ? history[history.length - 1].t : Date.now();
    const x = (t: number) => left + pw - ((now - t) / (HISTORY_SECONDS * 1000)) * pw;

    lanes.forEach((lane, li) => {
      const top = li * (LANE + GAP);
      ctx.textAlign = "right";
      ctx.fillStyle = cssVar("--text-secondary");
      ctx.fillText(lane.title, left - 10, top + LANE / 2);
      ctx.fillStyle = cssVar("--grid");
      ctx.fillRect(left, top, pw, LANE);
      for (let i = 0; i < history.length; i++) {
        const health = history[i].health[lane.code];
        if (!health) continue;
        const x0 = Math.round(Math.max(left, x(history[i].t)));
        const x1 = Math.round(i + 1 < history.length ? x(history[i + 1].t) : w - PAD.r);
        const next = history[i + 1]?.health[lane.code];
        const gap = next && next !== health ? 2 : 0;
        ctx.fillStyle = cssVar(TONE_VAR[(HEALTH[health] ?? HEALTH.NOT_TESTED).tone]);
        ctx.fillRect(x0, top, Math.max(0, x1 - x0 - gap), LANE);
      }
    });

    const tipEl = tip.current;
    if (!tipEl) return;
    const hv = hover.current;
    const p = hv ? history[hv.i] : null;
    if (!hv || !p) { tipEl.hidden = true; return; }
    const lane = lanes[hv.lane];
    const health = p.health[lane.code];
    tipEl.textContent = `${clock(p.t)}, ${lane.title}: ${health ? (HEALTH[health] ?? HEALTH.NOT_TESTED).text : "sem dado"}`;
    tipEl.hidden = false;
    tipEl.style.top = `${hv.lane * (LANE + GAP) - 30}px`;
    tipEl.style.left = `${Math.max(4, Math.min(x(p.t) + 10, w - tipEl.offsetWidth - 4))}px`;
  });

  const height = lanes.length * (LANE + GAP) - GAP;
  const onMove = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const lane = Math.floor((e.clientY - rect.top) / (LANE + GAP));
    const left = leftRef.current;
    const pw = rect.width - left - PAD.r;
    if (!history.length || lane < 0 || lane >= lanes.length || e.clientX - rect.left < left) { hover.current = null; paint(); return; }
    const now = history[history.length - 1].t;
    const t = now - ((rect.width - PAD.r - (e.clientX - rect.left)) / pw) * HISTORY_SECONDS * 1000;
    let best = 0;
    for (let i = 1; i < history.length; i++) if (Math.abs(history[i].t - t) < Math.abs(history[best].t - t)) best = i;
    hover.current = { i: best, lane };
    paint();
  };

  return (
    <div className="lanes-wrap">
      <canvas
        ref={ref} className="lanes" style={{ height }} role="img"
        aria-label={`Estado de ${lanes.length} componentes nos últimos 5 minutos`}
        onPointerMove={onMove} onPointerLeave={() => { hover.current = null; paint(); }}
      />
      <div className="tooltip" ref={tip} hidden />
    </div>
  );
}

export interface HourBarsProps {
  // 24 valores, um por hora do dia; null = hora ainda sem observação suficiente
  values: (number | null)[];
  current: number | null;
  color: string;
  max?: number;
  unit: string;
  tooltip: (hour: number, v: number | null) => string;
  label: string;
  size?: "sm";
}

const BARS_PAD = { l: 36, r: 6, t: 10, b: 20 };

function niceMax(v: number) {
  if (v <= 0) return 1;
  const p = 10 ** Math.floor(Math.log10(v));
  const m = v / p;
  return (m <= 1 ? 1 : m <= 2 ? 2 : m <= 5 ? 5 : 10) * p;
}

// Um dia em 24 barras: a hora atual fica destacada e horas sem dado aparecem só como contorno
export function HourBars(props: HourBarsProps) {
  const hover = useRef<number | null>(null);
  const tip = useRef<HTMLDivElement>(null);

  const geometry = (w: number, h: number) => {
    const pw = w - BARS_PAD.l - BARS_PAD.r;
    const ph = h - BARS_PAD.t - BARS_PAD.b;
    const slot = pw / 24;
    return { pw, ph, slot };
  };

  const { ref, paint } = useCanvas((ctx, w, h) => {
    const { values, current, max, unit } = props;
    const { ph, slot } = geometry(w, h);
    const known = values.filter((v): v is number => v !== null);
    const top = max ?? niceMax(Math.max(0, ...known) * 1.1);
    const y = (v: number) => BARS_PAD.t + ph - (Math.min(v, top) / top) * ph;
    const base = BARS_PAD.t + ph;
    const color = cssVar(props.color);

    ctx.font = "11px 'Segoe UI', system-ui, sans-serif";
    ctx.strokeStyle = cssVar("--grid");
    ctx.fillStyle = cssVar("--text-muted");
    ctx.lineWidth = 1;
    ctx.textAlign = "right";
    ctx.textBaseline = "middle";
    for (const v of [0, top / 2, top]) {
      ctx.beginPath(); ctx.moveTo(BARS_PAD.l, y(v)); ctx.lineTo(w - BARS_PAD.r, y(v)); ctx.stroke();
      ctx.fillText(`${Number.isInteger(v) ? v : v.toFixed(1)}${v === top ? unit : ""}`, BARS_PAD.l - 6, y(v));
    }

    ctx.textAlign = "center";
    ctx.textBaseline = "top";
    const barW = Math.max(2, slot * 0.68);
    for (let hr = 0; hr < 24; hr++) {
      const cx = BARS_PAD.l + slot * (hr + 0.5);
      const v = values[hr];
      const isNow = hr === current;
      const isHover = hr === hover.current;
      if (v === null) {
        ctx.save();
        ctx.setLineDash([3, 3]);
        ctx.strokeStyle = cssVar("--text-muted");
        ctx.globalAlpha = 0.45;
        ctx.strokeRect(cx - barW / 2 + 0.5, base - ph * 0.18, barW - 1, ph * 0.18);
        ctx.restore();
      } else {
        const top_ = y(v);
        const grad = ctx.createLinearGradient(0, top_, 0, base);
        grad.addColorStop(0, withAlpha(color, isNow || isHover ? 1 : 0.85));
        grad.addColorStop(1, withAlpha(color, isNow || isHover ? 0.55 : 0.35));
        ctx.fillStyle = grad;
        const r = Math.min(4, barW / 2);
        ctx.beginPath();
        ctx.roundRect(cx - barW / 2, top_, barW, Math.max(1, base - top_), [r, r, 0, 0]);
        ctx.fill();
      }
      if (isNow) {
        ctx.fillStyle = cssVar("--text-primary");
        ctx.beginPath(); ctx.arc(cx, base + 4, 2.5, 0, Math.PI * 2); ctx.fill();
      }
      if (hr % 3 === 0) {
        ctx.fillStyle = cssVar("--text-muted");
        ctx.fillText(`${hr}h`, cx, base + 8);
      }
    }

    const tipEl = tip.current;
    if (!tipEl) return;
    const i = hover.current;
    if (i === null) {
      tipEl.hidden = true;
      return;
    }
    tipEl.textContent = props.tooltip(i, values[i]);
    tipEl.hidden = false;
    const cx = BARS_PAD.l + slot * (i + 0.5);
    tipEl.style.left = `${Math.max(4, Math.min(cx + 10, w - tipEl.offsetWidth - 4))}px`;
  });

  const onMove = (ev: React.PointerEvent<HTMLCanvasElement>) => {
    const rect = ev.currentTarget.getBoundingClientRect();
    const { slot } = geometry(rect.width, rect.height);
    const hr = Math.floor((ev.clientX - rect.left - BARS_PAD.l) / slot);
    hover.current = hr >= 0 && hr < 24 ? hr : null;
    paint();
  };
  const onLeave = () => {
    hover.current = null;
    paint();
  };

  return (
    <div className="chart-wrap">
      <canvas ref={ref} className={`chart${props.size ? " chart-sm" : ""}`} role="img" aria-label={props.label} onPointerMove={onMove} onPointerLeave={onLeave} />
      <div className="tooltip" ref={tip} hidden />
    </div>
  );
}
