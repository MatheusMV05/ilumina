import { useState } from "react";
import { CaretDownIcon, DotsThreeIcon, FlaskIcon } from "@phosphor-icons/react";

import { EMPTY, fmtDur, fmtN } from "../format";
import { HEALTH, SENSORS, reasonText, type SensorMeta, type StatusSpec } from "../sensors";
import type { ComponentState, DashboardState, SensorCode, SparkPoint } from "../types";
import { startTest } from "../useTelemetry";
import { Sparkline } from "./Charts";
import { Badge, Orb } from "./StatusOrb";

const RUNNING: StatusSpec = { tone: "busy", icon: DotsThreeIcon, text: "Em andamento" };

function CheckRow({ spec, label, children }: { spec: StatusSpec; label: string; children: React.ReactNode }) {
  return (
    <div className="check">
      <Orb spec={spec} size="sm" />
      <div>
        <strong>{label}</strong>
        {children}
      </div>
    </div>
  );
}

interface Props<C extends SensorCode> {
  code: C;
  c: ComponentState<C>;
  d: DashboardState;
  series: SparkPoint[];
  testRunning: boolean;
}

export function SensorCard<C extends SensorCode>({ code, c, d, series, testRunning }: Props<C>) {
  const m = SENSORS[code] as SensorMeta<C>;
  const SensorIcon = m.icon;
  const connected = c.link?.state === "CONNECTED";
  const [value, unit, meaning] = connected ? m.main(c, d) : [null, "", ""];

  // A ligação esperada abre sozinha enquanto o sensor não for detectado, até o usuário mexer
  const [userOpen, setUserOpen] = useState<boolean | null>(null);
  const open = userOpen ?? !connected;

  // Passo 1: conexão física
  const link = c.link?.state;
  const s1: [StatusSpec, string] = link === "CONNECTED" ? [HEALTH.OK, "Hardware detectado no pino."]
    : link === "UNKNOWN" ? [HEALTH.NOT_TESTED, "Verificando…"]
    : [link === "SUSPECT" ? HEALTH.WARNING : HEALTH.FAILURE, reasonText(code, c.link?.reason)];

  // Passo 2: sinal coerente
  const r = connected ? m.measuring(c) : null;
  const s2: [StatusSpec, string] = !r ? [HEALTH.NOT_AVAILABLE, "Aguardando conexão."]
    : [r.ok === true ? HEALTH.OK : r.ok === false ? HEALTH.WARNING : HEALTH.NOT_TESTED, r.text];

  // Passo 3: teste prático
  const t = c.test;
  const s3: [StatusSpec, string] = t.state === "RUNNING" ? [RUNNING, "Em andamento. Siga a instrução acima."]
    : t.state === "PASSED" ? [HEALTH.OK, `Passou${t.finished_ago_ms !== null ? " há " + fmtDur(t.finished_ago_ms) : ""}: o sensor reagiu ao estímulo.`]
    : t.state === "FAILED" ? [HEALTH.WARNING, `Não reagiu no tempo (${m.reaction(t)}). Tente de novo.`]
    : [HEALTH.NOT_TESTED, connected ? "Ainda não feito." : "Disponível quando o sensor estiver conectado."];

  const details = connected ? m.details(c) : [];
  details.push(["Estado do firmware", c.note]);

  const vals = series.map((p) => p.v).filter((v): v is number => v !== null);
  const range = !vals.length ? "sem dados" : code === "IR"
    ? `${vals.filter((v) => v === 1).length} s detectando`
    : `mín ${fmtN(Math.min(...vals), 1)}, máx ${fmtN(Math.max(...vals), 1)} ${m.unit}`;

  return (
    <article className={`card${m.lighting ? " lighting-input" : ""}`}>
      <div className="card-head">
        <span className="card-icon" aria-hidden="true"><SensorIcon weight="duotone" /></span>
        <div className="card-titles">
          <h3>{m.title}</h3>
          <p className="role">{m.role}</p>
          <p className="part">
            <span className="chip">{m.part}</span>
            <span className="chip">{m.pins}</span>
            {m.lighting && <span className="chip chip-accent">decisão de luz</span>}
          </p>
        </div>
        <Badge spec={HEALTH[c.health] ?? HEALTH.NOT_TESTED} />
      </div>

      <div className={`reading${value === null ? " off" : ""}`}>
        <span className="value num">{value === null ? EMPTY : `${value}${unit ? " " + unit : ""}`}</span>
        <span className="meaning">{connected ? meaning : "sem sinal do sensor"}</span>
      </div>

      <div className="spark-wrap">
        <Sparkline series={series} step={code === "IR"} label={`Últimos 2 minutos de ${m.title}`} />
        <div className="spark-note"><span>{range}</span><span>últimos 2 min</span></div>
      </div>

      {t.state === "RUNNING" && (
        <div className="test-box" aria-live="polite">
          <div className="test-title">Teste em andamento</div>
          <div className="instruction">{m.instruction}</div>
          <div className="progress"><div style={{ transform: `scaleX(${t.remaining_ms / t.window_ms})` }} /></div>
          <div className="test-live">{m.reaction(t)}, {Math.ceil(t.remaining_ms / 1000)} s restantes</div>
        </div>
      )}

      <div className="checks">
        <CheckRow spec={s1[0]} label="Conectado"><span>{s1[1]}</span></CheckRow>
        <CheckRow spec={s2[0]} label="Medindo"><span>{s2[1]}</span></CheckRow>
        <CheckRow spec={s3[0]} label="Teste prático">
          <span>{s3[1]}</span>
          {t.state !== "RUNNING" && (
            <button type="button" className="btn btn-glass" disabled={!connected || testRunning} onClick={() => startTest(code)}>
              <FlaskIcon weight="duotone" aria-hidden="true" />
              {t.state === "PASSED" ? "Testar de novo" : "Fazer teste prático"}
            </button>
          )}
        </CheckRow>
      </div>

      <details open={open}>
        <summary onClick={(e) => { e.preventDefault(); setUserOpen(!open); }}>
          <CaretDownIcon weight="bold" aria-hidden="true" className="caret" />
          Detalhes técnicos e ligação
        </summary>
        <dl className="detail-rows">
          {details.map(([k, v]) => (
            <div className="row" key={k}><dt>{k}</dt><dd className="num">{String(v)}</dd></div>
          ))}
        </dl>
        <p className="wiring"><strong>Ligação:</strong> {m.wiring}</p>
      </details>
    </article>
  );
}
