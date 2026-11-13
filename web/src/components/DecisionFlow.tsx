import type { CSSProperties, ReactNode } from "react";
import type { Icon } from "@phosphor-icons/react";
import { CaretRightIcon, LightbulbIcon, MoonIcon, PersonSimpleWalkIcon, SlidersHorizontalIcon, SunIcon } from "@phosphor-icons/react";

import { fmtN } from "../format";
import { MODE_TEXT } from "../sensors";
import type { ByCode, DashboardState } from "../types";

const ADC_MAX = 4095;
const pct = (v: number) => Math.max(0, Math.min(100, (v / ADC_MAX) * 100));

function Step({ icon: StepIcon, label, result, children }: { icon: Icon; label: string; result: ReactNode; children: ReactNode }) {
  return (
    <div className="step">
      <div className="step-head">
        <span className="step-icon" aria-hidden="true"><StepIcon weight="duotone" /></span>
        <span className="step-label">{label}</span>
      </div>
      <div className="step-result">{result}</div>
      {children}
    </div>
  );
}

const Arrow = () => <CaretRightIcon className="flow-arrow" weight="bold" aria-hidden="true" />;

export function DecisionFlow({ state, byCode }: { state: DashboardState; byCode: ByCode }) {
  const L = state.lighting;
  const C = state.config;
  const amb = byCode.LDR_AMBIENT.data;
  const hc = byCode.ULTRASONIC;
  const ir = byCode.IR;
  const anyNow = L.presence_hc || L.presence_ir;

  const hcText = hc.link?.state !== "CONNECTED" ? "não conectado"
    : hc.data.distance_cm === null ? "nada à frente" : `${fmtN(hc.data.distance_cm)} cm${L.presence_hc ? ", presença" : ""}`;
  const irText = ir.health === "NOT_AVAILABLE" ? "não instalado"
    : ir.link?.state !== "CONNECTED" ? "não conectado"
    : !L.ir_usable ? "falta testar" : ir.data.detected ? "detectando" : "livre";
  const holdText = !L.presence_available
    ? "Nenhum dos dois foi confirmado ainda: à noite a luz fica no máximo por segurança."
    : L.hold_remaining_ms > 0
      ? `Mantendo a luz alta por mais ${Math.ceil(L.hold_remaining_ms / 1000)} s após a última detecção.`
      : `Presença conta entre ${C.min_distance_cm} e ${C.max_distance_cm} cm do HC-SR04.`;
  const reason = {
    DAY_OFF: "Está claro: luminárias apagadas para economizar energia.",
    ECONOMY: `Está escuro e não há ninguém: luz reduzida a ${C.standby_percent}%.`,
    ACTIVE: anyNow ? "Presença detectada: luz no máximo." : "Presença recente: mantendo a luz no máximo.",
    SAFE: L.is_night === null ? "Sem leitura confiável da luz ambiente: luz no máximo por segurança."
      : "Nenhum sensor de presença confirmado: luz no máximo por segurança.",
  }[L.mode] ?? "";

  return (
    <section className="glass panel reveal" aria-labelledby="t-decision">
      <h2 id="t-decision">Como a luz está sendo decidida agora</h2>
      <p className="section-sub">Os sensores marcados como “decisão de luz” alimentam estas etapas, da esquerda para a direita.</p>

      <div className="flow">
        <Step icon={L.is_night ? MoonIcon : SunIcon} label="Está claro ou escuro?"
          result={L.is_night === null ? "Sem leitura" : L.is_night ? "Escuro (noite)" : "Claro (dia)"}>
          <div className="scale" aria-hidden="true">
            <div className="scale-band" style={{ left: `${pct(C.night_threshold)}%`, width: `${pct(C.day_threshold) - pct(C.night_threshold)}%` }} />
            <div className="scale-tick" style={{ left: `${pct(C.night_threshold)}%` }} />
            <div className="scale-tick" style={{ left: `${pct(C.day_threshold)}%` }} />
            <span className="scale-label is-night" style={{ left: `${pct(C.night_threshold)}%` }}>{C.night_threshold}</span>
            <span className="scale-label is-day" style={{ left: `${pct(C.day_threshold)}%` }}>{C.day_threshold}</span>
            <div className="scale-marker" style={{ left: `${pct(amb.raw)}%` }} />
          </div>
          <div className="row"><span>Luz ambiente (LDR)</span><span className="num">{amb.value === null ? `inválida (${amb.raw})` : `${amb.value} ADC`}</span></div>
          <p className="step-detail">
            Vira dia acima de {C.day_threshold} e noite abaixo de {C.night_threshold}. Entre os dois, mantém o estado anterior para não piscar.
          </p>
        </Step>
        <Arrow />
        <Step icon={PersonSimpleWalkIcon} label="Tem alguém por perto?" result={anyNow ? "Sim" : "Ninguém"}>
          <div className="or-box">
            <div className={`sensor-line${L.presence_hc ? " hit" : ""}`}><span>Distância (HC-SR04)</span><span className="num">{hcText}</span></div>
            <div className="or">ou</div>
            <div className={`sensor-line${L.presence_ir ? " hit" : ""}`}><span>Obstáculo (IR)</span><span>{irText}</span></div>
          </div>
          <p className="step-detail">{holdText}</p>
        </Step>
        <Arrow />
        <Step icon={SlidersHorizontalIcon} label="Modo escolhido" result={MODE_TEXT[L.mode] ?? L.mode}>
          <p className="step-detail">{reason}</p>
        </Step>
        <Arrow />
        <Step icon={LightbulbIcon} label="Luminárias" result={<span className="num">{L.zones[0]}%</span>}>
          <div className="lamps">
            {L.zones.map((z, i) => (
              <div className="lamp" key={i} title={`Luminária ${i + 1}: ${z}%`}>
                <span className={`bulb${z > 0 ? " on" : ""}`} style={{ "--level": z / 100 } as CSSProperties} />
                L{i + 1}
              </div>
            ))}
          </div>
          <p className="step-detail">
            Dia 0%, noite sem ninguém {C.standby_percent}%, com presença {C.active_percent}% por {C.hold_ms / 1000} s.
          </p>
        </Step>
      </div>
    </section>
  );
}
