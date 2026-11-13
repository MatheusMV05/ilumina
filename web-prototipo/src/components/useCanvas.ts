import { useEffect, useRef } from "react";

export type Draw = (ctx: CanvasRenderingContext2D, w: number, h: number) => void;

// Canvas nítido em telas de alta densidade; redesenha ao redimensionar e ao trocar claro/escuro
export function useCanvas(draw: Draw) {
  const ref = useRef<HTMLCanvasElement>(null);
  const drawRef = useRef(draw);
  drawRef.current = draw;

  const paint = () => {
    const canvas = ref.current;
    if (!canvas) return;
    const dpr = window.devicePixelRatio || 1;
    const w = canvas.clientWidth;
    const h = canvas.clientHeight;
    if (!w) return;
    if (canvas.width !== w * dpr || canvas.height !== h * dpr) {
      canvas.width = w * dpr;
      canvas.height = h * dpr;
    }
    const ctx = canvas.getContext("2d");
    if (!ctx) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);
    drawRef.current(ctx, w, h);
  };

  useEffect(() => {
    const canvas = ref.current;
    if (!canvas) return;
    const observer = new ResizeObserver(paint);
    observer.observe(canvas);
    const scheme = matchMedia("(prefers-color-scheme: dark)");
    scheme.addEventListener("change", paint);
    // Tema escolhido manualmente pelo visualizador (data-theme no <html>)
    const theme = new MutationObserver(paint);
    theme.observe(document.documentElement, { attributes: true, attributeFilter: ["data-theme"] });
    return () => {
      observer.disconnect();
      theme.disconnect();
      scheme.removeEventListener("change", paint);
    };
  }, []);

  // Dados novos chegam a cada segundo junto com um novo render
  useEffect(paint);

  return { ref, paint };
}
