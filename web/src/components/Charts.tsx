import { useRef } from "react";

import { clock, cssVar } from "../format";
import type { HistoryPoint, SparkPoint } from "../types";
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
}

const PAD = { l: 40, r: 8, t: 10, b: 10 };

// Um eixo por gráfico; o ponteiro mostra o valor mais próximo sem re-renderizar o React
export function LineChart(props: LineChartProps) {
  const hover = useRef<number | null>(null);
  const tip = useRef<HTMLDivElement>(null);

  const { ref, paint } = useCanvas((ctx, w, h) => {
    const { history, min, max, ticks, value, thresholds = [], tooltip } = props;
    const pw = w - PAD.l - PAD.r;
    const ph = h - PAD.t - PAD.b;
    const y = (v: number) => PAD.t + ph - ((v - min) / (max - min)) * ph;
    const now = history.length ? history[history.length - 1].t : Date.now();
    const x = (t: number) => PAD.l + pw - ((now - t) / (HISTORY_SECONDS * 1000)) * pw;

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
    strokeSeries(ctx, history, value, x, y, PAD.t + ph, color);

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
    ctx.beginPath(); ctx.moveTo(px, PAD.t); ctx.lineTo(px, PAD.t + ph); ctx.stroke();
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
      <canvas ref={ref} className="chart" role="img" aria-label={props.label} onPointerMove={onMove} onPointerLeave={onLeave} />
      <div className="tooltip" ref={tip} hidden />
    </div>
  );
}
