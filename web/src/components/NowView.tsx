import { useRef, useState, type CSSProperties } from "react";
import {
  CheckIcon, LeafIcon, LightbulbIcon, MoonIcon, PersonSimpleWalkIcon, PulseIcon, SunIcon, TimerIcon, WarningIcon,
} from "@phosphor-icons/react";

import { fmtDur, fmtN } from "../format";
import type { Issue, TabId } from "../nav";
import { HEALTH, MODE_TEXT } from "../sensors";
import type { ByCode, Code, DashboardState, EnergyTally, LampControl, LampState, LampStatus, Mode } from "../types";
import { checkLamps, setLamp } from "../useTelemetry";
import { Orb } from "./StatusOrb";
import { Widget } from "./Widget";

type Go = (t: TabId, c?: Code) => void;

const ADC_MAX = 4095;
const pct = (v: number) => Math.max(0, Math.min(100, (v / ADC_MAX) * 100));

export const MODE_COLOR: Record<Mode, string> = { DAY_OFF: "--mode-day", ECONOMY: "--mode-eco", ACTIVE: "--mode-active", SAFE: "--critical" };
export const MODE_ORDER: Mode[] = ["DAY_OFF", "ECONOMY", "ACTIVE", "SAFE"];

export function reasonText(state: DashboardState) {
  const L = state.lighting;
  const C = state.config;
  const anyNow = L.presence_hc || L.presence_ir;
  return {
    DAY_OFF: "Está claro, então as luminárias ficam apagadas para economizar energia.",
    ECONOMY: `Está escuro e não há ninguém por perto: luz em ${L.target_percent}% (de ${C.dusk_percent}% no anoitecer a ${C.night_percent}% na noite fechada).`,
    ACTIVE: anyNow ? "Alguém está por perto: luz no máximo." : "Alguém passou há pouco: a luz segue no máximo por mais um tempo.",
    SAFE: L.is_night === null ? "Sem leitura confiável da luz ambiente. Por segurança, luz no máximo."
      : "Nenhum sensor de presença confirmado. Por segurança, luz no máximo.",
  }[L.mode] ?? "";
}

const CONTROL_TEXT: Record<LampControl, string> = { AUTO: "Auto", OFF: "Desligada", ON: "Ligada" };
const STATUS_TEXT: Record<LampStatus, string> = { OK: "no circuito", MISSING: "sem resposta", UNKNOWN: "não verificado" };

// Uma luminária: lâmpada, resultado da verificação e o comando do painel
function LampCard({ index, lamp }: { index: number; lamp: LampState }) {
  // Mostra o comando na hora; a próxima leitura do poste (1 s) confirma
  const [pending, setPending] = useState<{ control: LampControl; at: number } | null>(null);
  const [draft, setDraft] = useState<number | null>(null);
  const timer = useRef<ReturnType<typeof setTimeout> | undefined>(undefined);
  const control = pending && Date.now() - pending.at < 3000 && pending.control !== lamp.control ? pending.control : lamp.control;
  const manual = draft ?? lamp.manual_percent;

  const choose = (next: LampControl) => {
    setPending({ control: next, at: Date.now() });
    setLamp(index, next, next === "ON" ? manual : undefined);
  };
  const slide = (v: number) => {
    setDraft(v);
    clearTimeout(timer.current);
    timer.current = setTimeout(() => { setLamp(index, "ON", v); setTimeout(() => setDraft(null), 1500); }, 250);
  };

  return (
    <div className={`lamp-card status-${lamp.status}`}>
      <div className="lamp-card-head">
        <span className={`bulb${lamp.percent > 0 ? " on" : ""}${lamp.status === "MISSING" ? " is-missing" : ""}`} style={{ "--level": lamp.percent / 100 } as CSSProperties} aria-hidden="true" />
        <div className="lamp-card-title">
          <strong>L{index + 1}</strong>
          <span className="num">{lamp.percent}%</span>
        </div>
        <span className={`lamp-status status-${lamp.status}`}>
          {lamp.status === "MISSING" && <WarningIcon weight="fill" aria-hidden="true" />}
          {STATUS_TEXT[lamp.status]}
        </span>
      </div>
      <div className="segmented" role="radiogroup" aria-label={`Comando da luminária ${index + 1}`}>
        {(["AUTO", "OFF", "ON"] as LampControl[]).map((c) => (
          <button key={c} type="button" role="radio" aria-checked={control === c} className={control === c ? "is-on" : ""} onClick={() => choose(c)}>
            {CONTROL_TEXT[c]}
          </button>
        ))}
      </div>
      {control === "ON" && (
        <label className="lamp-slider">
          <span>Brilho <span className="num">{manual}%</span></span>
          <input type="range" min={10} max={100} step={10} value={manual} onChange={(e) => slide(Number(e.target.value))} />
        </label>
      )}
    </div>
  );
}

function CheckButton({ L }: { L: DashboardState["lighting"] }) {
  const [clickedAt, setClickedAt] = useState(0);
  const since = Date.now() - clickedAt;
  // Verificando até o firmware informar uma verificação mais nova que o clique
  const checking = clickedAt > 0 && since < 10000 && (L.lamp_check_ago_ms === null || L.lamp_check_ago_ms > since);
  return (
    <div className="lamp-check">
      <button type="button" className="btn btn-glass" disabled={checking} onClick={() => { setClickedAt(Date.now()); checkLamps(); }}>
        <PulseIcon weight="bold" aria-hidden="true" /> {checking ? "Verificando..." : "Verificar LEDs"}
      </button>
      <span>
        {checking ? "Os LEDs piscam um por vez por alguns segundos."
          : L.lamp_check_ago_ms === null ? "Ainda não verificados."
          : `Verificados há ${fmtDur(L.lamp_check_ago_ms)}.${L.lamp_check_auto_ms ? ` Repete sozinho a cada ${fmtDur(L.lamp_check_auto_ms)}.` : ""}`}
      </span>
    </div>
  );
}

// Resultado: quanto a luz está acesa, o comando de cada luminária e se cada LED está no circuito
export function LampsWidget({ state }: { state: DashboardState }) {
  const L = state.lighting;
  const manual = L.lamps.filter((l) => l.control !== "AUTO").length;
  return (
    <Widget color="#e9a10f" icon={LightbulbIcon} title="Luminárias" id="c-LEDS" className={`lamps-widget span-2 mode-${L.mode}`}>
      <div className="hero-level">
        <span className="hero-pct num" key={L.target_percent}>{L.target_percent}<small>%</small></span>
        <span className={`mode-pill mode-${L.mode}`}>{MODE_TEXT[L.mode] ?? L.mode}</span>
      </div>
      <p className="hero-reason">
        {reasonText(state)}
        {manual > 0 && ` ${manual === 1 ? "Uma luminária está" : `${manual} luminárias estão`} em comando manual e não seguem essa regra.`}
      </p>
      <div className="lamp-grid">
        {L.lamps.map((lamp, i) => <LampCard key={i} index={i} lamp={lamp} />)}
      </div>
      <CheckButton L={L} />
      {L.lamp_check_no_signal && (
        <p className="widget-note">Nenhum LED mudou a leitura do LDR ambiente na verificação: ou todos estão fora do circuito (GND comum solto?), ou esta montagem não dá sinal para medir.</p>
      )}
      <p className="widget-note">Comandos manuais valem até voltar para Auto ou o poste reiniciar.</p>
    </Widget>
  );
}

// Causas: uma linha para cada pergunta que o firmware faz antes de decidir
export function WhyWidget({ state, byCode }: { state: DashboardState; byCode: ByCode }) {
  const L = state.lighting;
  const C = state.config;
  const amb = byCode.LDR_AMBIENT.data;
  const hc = byCode.ULTRASONIC;
  const ir = byCode.IR;
  const anyNow = L.presence_hc || L.presence_ir;
  const duration = L.mode === "DAY_OFF" ? "enquanto estiver claro"
    : L.mode === "SAFE" ? "até resolver"
    : L.hold_remaining_ms > 0 && !anyNow ? `mais ${Math.ceil(L.hold_remaining_ms / 1000)} s`
    : anyNow ? "enquanto houver alguém" : "até alguém aparecer";
  const presence = !anyNow ? "ninguém"
    : L.presence_hc && hc.data.distance_cm !== null ? `sim, a ${fmtN(hc.data.distance_cm)} cm` : "sim";
  const irText = ir.health === "NOT_AVAILABLE" ? "não instalado"
    : ir.link?.state !== "CONNECTED" ? "não conectado" : !L.ir_usable ? "falta testar" : ir.data.detected ? "detectando" : "livre";

  return (
    <Widget color="#1e90e0" icon={L.is_night ? MoonIcon : SunIcon} title="Por que está assim">
      <ul className="rows">
        <li className="row-pill">
          <span className="row-icon" aria-hidden="true">{L.is_night ? <MoonIcon weight="duotone" /> : <SunIcon weight="duotone" />}</span>
          <span className="row-label">Claro ou escuro?</span>
          <span className="tag">{L.is_night === null ? "sem leitura" : L.is_night ? "noite" : "dia"}</span>
        </li>
        <li className="row-scale">
          <div className="scale" aria-hidden="true">
            <div className="scale-band" style={{ left: `${pct(C.night_threshold)}%`, width: `${pct(C.day_threshold) - pct(C.night_threshold)}%` }} />
            <div className="scale-marker" style={{ left: `${pct(amb.raw)}%` }} />
          </div>
          <span className="num">LDR em {amb.value ?? amb.raw}. Noite abaixo de {C.night_threshold}, dia acima de {C.day_threshold}.</span>
        </li>
        <li className={`row-pill${anyNow ? " is-hit" : ""}`}>
          <span className="row-icon" aria-hidden="true"><PersonSimpleWalkIcon weight="duotone" /></span>
          <span className="row-label">Alguém por perto?</span>
          <span className="tag num">{presence}</span>
        </li>
        <li className="row-sub">
          <span>Obstáculo (IR)</span><span>{irText}</span>
        </li>
        <li className="row-pill">
          <span className="row-icon" aria-hidden="true"><TimerIcon weight="duotone" /></span>
          <span className="row-label">Fica assim</span>
          <span className="tag num">{duration}</span>
        </li>
      </ul>
      {L.is_night === false && anyNow && <p className="widget-note">De dia a presença não muda a luz.</p>}
    </Widget>
  );
}

export function IssuesWidget({ state, issues, go }: { state: DashboardState; issues: Issue[]; go: Go }) {
  const safe = state.summary === "SAFE_MODE";
  const critical = safe || issues.some((i) => i.critical);
  if (!issues.length && !safe) {
    return (
      <Widget color="#3cb54a" icon={CheckIcon} title="Tudo funcionando">
        <div className="widget-empty">
          <Orb spec={{ ...HEALTH.OK, icon: CheckIcon }} size="lg" />
          <p>Todos os sensores essenciais estão conectados e respondendo.</p>
        </div>
      </Widget>
    );
  }
  return (
    <Widget
      color={critical ? "#de3f34" : "#e8781e"} icon={WarningIcon}
      title={safe ? "Modo seguro ativo" : `${issues.length} pendência${issues.length === 1 ? "" : "s"}`}
    >
      {safe && <p className="widget-note">A luz fica no máximo até o problema abaixo ser resolvido.</p>}
      <ul className="rows">
        {issues.map((i) => (
          <li key={i.code + i.title}>
            <button type="button" className="row-pill row-action row-tall" onClick={() => go(i.tab, i.code)}>
              <span className={`row-dot${i.critical ? " is-critical" : ""}`} aria-hidden="true" />
              <span className="row-text">
                <strong>{i.title}</strong>
                <span>{i.text}</span>
              </span>
              <span className="tag tag-action">Resolver</span>
            </button>
          </li>
        ))}
      </ul>
    </Widget>
  );
}

// Comparação justa: um poste comum com fotocélula também apaga de dia, então só a noite entra na conta
export function EnergyWidget({ energy }: { energy: EnergyTally }) {
  const elapsed = Date.now() - energy.since;
  const usedPct = energy.baseline > 0 ? Math.round((energy.used / energy.baseline) * 100) : null;
  return (
    <Widget color="#3cb54a" icon={LeafIcon} title="Energia economizada">
      {usedPct === null ? (
        <p className="widget-note">Ainda não houve período escuro desde que o painel abriu. De dia, este poste e um poste comum ficam apagados, então não há o que comparar.</p>
      ) : (
        <>
          <div className="energy-hero">
            <span className="energy-pct num" key={usedPct}>{Math.max(0, 100 - usedPct)}%</span>
            <span className="energy-caption">menos energia que um poste comum nas horas escuras, desde que o painel abriu há {fmtDur(elapsed)}</span>
          </div>
          <div className="energy-bars" role="img" aria-label={`Este poste usou ${usedPct}% da energia de um poste comum`}>
            <div className="energy-row">
              <span>Este poste</span>
              <span className="energy-bar"><span style={{ width: `${Math.min(100, usedPct)}%` }} /></span>
              <span className="num">{usedPct}%</span>
            </div>
            <div className="energy-row">
              <span>Poste comum</span>
              <span className="energy-bar is-base"><span style={{ width: "100%" }} /></span>
              <span className="num">100%</span>
            </div>
          </div>
        </>
      )}
      <p className="widget-note">Proporção calculada pela intensidade dos LEDs. Sem sensor de corrente, não é uma medida em watts.</p>
    </Widget>
  );
}
