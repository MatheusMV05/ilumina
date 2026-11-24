import { useState } from "react";
import { CaretDownIcon, DotsThreeIcon, FlaskIcon } from "@phosphor-icons/react";
import type { CSSProperties } from "react";

import { EMPTY, fmtDur, fmtN } from "../format";
import { HEALTH, SENSORS, SENSOR_COLOR, reasonText, type SensorMeta, type StatusSpec } from "../sensors";
import type { ComponentState, DashboardState, SensorCode, SparkPoint } from "../types";
import { startTest } from "../useTelemetry";
import { Sparkline } from "./Charts";
import { Badge, Orb } from "./StatusOrb";

const RUNNING: StatusSpec = { tone: "busy", icon: DotsThreeIcon, text: "Em andamento" };

interface Props<C extends SensorCode> {
  code: C;
  c: ComponentState<C>;
  d: DashboardState;
  series: SparkPoint[];
  testRunning: boolean;
}

export function SensorCard<C extends SensorCode>({ code, c, d, series, testRunning, index }: Props<C> & { index: number }) {
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
    : [HEALTH.NOT_TESTED, connected ? "Falta o teste prático: toque no botão e siga a instrução." : "O teste fica disponível quando o sensor estiver conectado."];

  const details = connected ? m.details(c) : [];
  details.push(["Estado do firmware", c.note]);

  const vals = series.map((p) => p.v).filter((v): v is number => v !== null);
  const range = !vals.length ? "sem dados" : code === "IR"
    ? `${vals.filter((v) => v === 1).length} s detectando`
    : `mín ${fmtN(Math.min(...vals), 1)}, máx ${fmtN(Math.max(...vals), 1)} ${m.unit}`;

  // Os três passos viram uma régua; a mensagem mostrada é a do primeiro passo que ainda pede algo
  const steps: [string, [StatusSpec, string]][] = [["Conectado", s1], ["Medindo", s2], ["Testado", s3]];
  const pending = steps.find(([, [spec]]) => spec.tone !== "good");
  const message = pending ? pending[1][1] : s3[1];
  const primary = connected && t.state !== "PASSED";

  return (
    <article id={`c-${code}`} className="widget card has-waves" style={{ "--i": index, "--widget-color": SENSOR_COLOR[code] } as CSSProperties}>
      <div className="widget-head card-head">
        <span className="chip chip-round" aria-hidden="true"><SensorIcon weight="fill" /></span>
        <div className="card-titles">
          <h3 className="widget-title">{m.title}</h3>
          <p className="role">{m.role}</p>
        </div>
        <Badge spec={HEALTH[c.health] ?? HEALTH.NOT_TESTED} />
      </div>

      <div className={`reading${value === null ? " off" : ""}`}>
        {/* A chave muda junto com a leitura, então o valor novo entra com um brilho rápido */}
        <span className="value num" key={String(value)}>{value === null ? EMPTY : `${value}${unit ? " " + unit : ""}`}</span>
        <span className="meaning">{connected ? meaning : "sem sinal do sensor"}</span>
      </div>

      <div className="spark-wrap">
        <Sparkline series={series} step={code === "IR"} label={`Últimos 2 minutos de ${m.title}`} />
        <div className="spark-note"><span>{range}</span><span>últimos 2 min</span></div>
      </div>

      <ol className="stepper" aria-label="Verificações">
        {steps.map(([label, [spec, text]]) => (
          <li key={label} className={`tone-${spec.tone}`} title={text}>
            <Orb spec={spec} size="sm" />
            <span>{label}</span>
          </li>
        ))}
      </ol>
      <p className={`step-msg${pending ? ` tone-${pending[1][0].tone}` : ""}`}>{message}</p>

      {t.state === "RUNNING" ? (
        <p className="test-note">Teste em andamento. Siga a instrução no painel flutuante.</p>
      ) : (
        <button type="button" className={`btn ${primary ? "btn-aqua" : "btn-glass"} card-action`} disabled={!connected || testRunning} onClick={() => startTest(code)}>
          <FlaskIcon weight="duotone" aria-hidden="true" />
          {t.state === "PASSED" ? "Testar de novo" : "Fazer teste prático"}
        </button>
      )}

      <details open={open}>
        <summary onClick={(e) => { e.preventDefault(); setUserOpen(!open); }}>
          <CaretDownIcon weight="bold" aria-hidden="true" className="caret" />
          Detalhes e ligação
        </summary>
        <dl className="detail-rows">
          {steps.map(([label, [, text]]) => (
            <div className="row" key={label}><dt>{label}</dt><dd>{text}</dd></div>
          ))}
          <div className="row"><dt>Componente</dt><dd>{m.part}, {m.pins}</dd></div>
          {details.map(([k, v]) => (
            <div className="row" key={k}><dt>{k}</dt><dd className="num">{String(v)}</dd></div>
          ))}
        </dl>
        <p className="wiring"><strong>Ligação:</strong> {m.wiring}</p>
      </details>
    </article>
  );
}
