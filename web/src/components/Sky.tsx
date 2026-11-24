import type { CSSProperties } from "react";

// Bolhas: posição horizontal, tamanho, duração da subida e atraso (valores em %, px e s)
const BUBBLES: [number, number, number, number][] = [
  [4, 150, 30, -4], [90, 110, 34, -12], [80, 56, 22, -6], [14, 44, 19, -3], [46, 30, 17, -9], [62, 40, 24, -15], [30, 24, 15, -5],
];

// Céu Frutiger: brilho do sol, colinas verdes no rodapé e bolhas subindo. Camada fixa atrás de tudo
export function Sky() {
  return (
    <div className="sky" aria-hidden="true">
      <span className="sky-sun" />
      <svg className="sky-hills" viewBox="0 0 1440 360" preserveAspectRatio="none">
        <defs>
          <linearGradient id="hill-back" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0" style={{ stopColor: "var(--hill-1)" }} />
            <stop offset="1" style={{ stopColor: "var(--hill-2)" }} />
          </linearGradient>
          <linearGradient id="hill-front" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0" style={{ stopColor: "var(--hill-3)" }} />
            <stop offset="1" style={{ stopColor: "var(--hill-4)" }} />
          </linearGradient>
          <radialGradient id="hill-shine" cx=".5" cy=".5" r=".5">
            <stop offset="0" stopColor="#fff" stopOpacity=".5" />
            <stop offset="1" stopColor="#fff" stopOpacity="0" />
          </radialGradient>
        </defs>
        <path d="M0 170 Q 300 70 640 150 T 1440 110 V360 H0 Z" fill="url(#hill-back)" opacity=".85" />
        <ellipse cx="1120" cy="150" rx="230" ry="28" fill="url(#hill-shine)" />
        <path d="M0 270 Q 380 170 760 240 T 1440 230 V360 H0 Z" fill="url(#hill-front)" />
        <path d="M0 270 Q 380 170 760 240 T 1440 230" fill="none" stroke="#fff" strokeOpacity=".5" strokeWidth="2" vectorEffect="non-scaling-stroke" />
        <ellipse cx="380" cy="232" rx="260" ry="30" fill="url(#hill-shine)" />
      </svg>
      {BUBBLES.map(([x, size, dur, delay], i) => (
        <span
          key={i}
          className="bubble"
          style={{ "--x": `${x}%`, "--s": `${size}px`, "--d": `${dur}s`, "--delay": `${delay}s` } as CSSProperties}
        />
      ))}
    </div>
  );
}
