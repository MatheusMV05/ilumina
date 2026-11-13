export const EMPTY = "-";

export const fmtN = (v: number | null | undefined, digits = 0): string | null =>
  v === null || v === undefined ? null : Number(v).toFixed(digits).replace(".", ",");

export function fmtDur(ms: number): string {
  const s = Math.max(0, Math.floor(ms / 1000));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return h > 0 ? `${h}h ${m}min` : m > 0 ? `${m}min ${s % 60}s` : `${s}s`;
}

export const clock = (t: number) => new Date(t).toLocaleTimeString("pt-BR", { hour12: false });

export const cssVar = (name: string) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
