import { useEffect, useRef, useState } from "react";
import { ArrowClockwiseIcon, CheckIcon, WarningIcon, XIcon } from "@phosphor-icons/react";

import { SENSORS, SENSOR_ORDER } from "../sensors";
import type { ByCode, SensorCode, TestState } from "../types";
import { startTest } from "../useTelemetry";
import { Orb } from "./StatusOrb";

type Result = { code: SensorCode; passed: boolean };

// Quanto falta para a reação bater a meta (0 a 1). No HC-SR04 a meta é chegar perto: menor é melhor
function reach(code: SensorCode, t: TestState) {
  if (code === "ULTRASONIC") return t.best >= 9999 ? 0 : Math.min(1, t.target / Math.max(t.best, 1));
  return t.target > 0 ? Math.max(0, Math.min(1, t.best / t.target)) : 0;
}

// Teste prático em andamento fica sempre visível, em qualquer aba: a pessoa está com as mãos no circuito
export function TestDock({ byCode }: { byCode: ByCode }) {
  const running = SENSOR_ORDER.find((code) => byCode[code].test.state === "RUNNING") ?? null;
  const last = useRef<SensorCode | null>(null);
  const [result, setResult] = useState<Result | null>(null);

  // Quando o teste termina, o resultado fica alguns segundos no mesmo lugar onde estava a instrução
  useEffect(() => {
    if (running) {
      last.current = running;
      setResult(null);
      return;
    }
    if (!last.current) return;
    const code = last.current;
    last.current = null;
    setResult({ code, passed: byCode[code].test.state === "PASSED" });
  }, [running, byCode]);

  useEffect(() => {
    if (!result) return;
    const timer = setTimeout(() => setResult(null), 6000);
    return () => clearTimeout(timer);
  }, [result]);

  if (running) {
    const m = SENSORS[running];
    const t = byCode[running].test;
    const SensorIcon = m.icon;
    return (
      <aside className="dock glass" aria-live="polite" aria-label="Teste prático em andamento">
        <span className="dock-icon" aria-hidden="true"><SensorIcon weight="duotone" /></span>
        <div className="dock-body">
          <span className="dock-title">Testando {m.title.toLowerCase()}</span>
          <strong className="dock-instruction">{m.instruction}</strong>
          <div className="meter" role="meter" aria-label="Reação do sensor até a meta" aria-valuemin={0} aria-valuemax={100} aria-valuenow={Math.round(reach(running, t) * 100)}>
            <span style={{ transform: `scaleX(${reach(running, t)})` }} />
          </div>
          <span className="dock-live num">{(m.reaction as (x: typeof t) => string)(t)}</span>
          <span className="dock-time num">{Math.ceil(t.remaining_ms / 1000)} s restantes</span>
        </div>
      </aside>
    );
  }

  if (!result) return null;
  const m = SENSORS[result.code];
  return (
    <aside className={`dock glass is-result tone-${result.passed ? "good" : "warning"}`} aria-live="polite">
      <Orb spec={{ tone: result.passed ? "good" : "warning", icon: result.passed ? CheckIcon : WarningIcon }} size="md" />
      <div className="dock-body">
        <strong>{result.passed ? `${m.title}: teste aprovado` : `${m.title}: não reagiu a tempo`}</strong>
        <span className="dock-live">{result.passed ? "O sensor respondeu ao estímulo." : "Confira a instrução e tente de novo."}</span>
      </div>
      {!result.passed && (
        <button type="button" className="btn btn-aqua" onClick={() => startTest(result.code)}>
          <ArrowClockwiseIcon weight="bold" aria-hidden="true" /> Tentar de novo
        </button>
      )}
      <button type="button" className="dock-close" aria-label="Fechar" onClick={() => setResult(null)}>
        <XIcon weight="bold" aria-hidden="true" />
      </button>
    </aside>
  );
}
