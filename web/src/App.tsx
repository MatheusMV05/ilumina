import { useMemo } from "react";
import {
  ArrowRightIcon, ChartLineIcon, FlaskIcon, LampIcon, PersonSimpleWalkIcon, PulseIcon, SunHorizonIcon, TableIcon,
} from "@phosphor-icons/react";

import { DistanceChart, LineChart, StatusLanes } from "./components/Charts";
import { EnergyWidget, IssuesWidget, LampsWidget, MODE_COLOR, MODE_ORDER, WhyWidget } from "./components/NowView";
import { StreetView } from "./components/StreetView";
import { SensorCard } from "./components/SensorCard";
import { ScreenHeader, Sidebar, StatusBar, Titlebar } from "./components/Shell";
import { SystemPanel } from "./components/SystemPanel";
import { TestDock } from "./components/TestDock";
import { Widget } from "./components/Widget";
import { EMPTY, clock, fmtDur, fmtN } from "./format";
import { collectIssues, useNav, type Issue, type TabId } from "./nav";
import { HEALTH, MODE_TEXT, SENSORS, SENSOR_ORDER } from "./sensors";
import type { ByCode, Code, DashboardState, EnergyTally, HistoryPoint, SensorCode, SparkPoint } from "./types";
import { startTest, useStreet, useTelemetry } from "./useTelemetry";

// Esqueleto com a forma da tela "Agora" enquanto o primeiro JSON não chega
function Loading() {
  return (
    <div className="loading" aria-busy="true" aria-label="Carregando dados do poste">
      <div className="home-grid">
        {[0, 1, 2].map((i) => (
          <div key={i} className="widget skeleton">
            <div className="sk-head"><span className="sk-orb" /><span className="sk-line w40" /></div>
            <span className="sk-block" />
            <span className="sk-line w70" />
          </div>
        ))}
      </div>
      <p className="loading-note">Aguardando dados do poste. Conecte-se à rede Wi-Fi <strong>Iluminacao-IoT</strong> para ver o estado do sistema.</p>
    </div>
  );
}

const SYSTEM_TITLES: Partial<Record<Code, string>> = { OLED: "Tela OLED", LEDS: "Luminárias", STATUS_LED: "LED de status" };
const LANE_LEGEND = [HEALTH.OK, HEALTH.WARNING, HEALTH.FAILURE, HEALTH.NOT_TESTED];

// Mau contato aparece como listras: o selo de agora não mostra um sensor que cai e volta
function Stability({ byCode, history }: { byCode: ByCode; history: HistoryPoint[] }) {
  const lanes = [...SENSOR_ORDER, "OLED", "LEDS", "STATUS_LED"]
    .filter((code) => byCode[code as Code].health !== "NOT_AVAILABLE")
    .map((code) => ({ code: code as Code, title: SENSORS[code as SensorCode]?.title ?? SYSTEM_TITLES[code as Code] ?? code }));
  return (
    <Widget color="#22a861" icon={PulseIcon} title="Estabilidade nos últimos 5 minutos" waves={false} className="span-all">
      <p className="widget-note">Uma faixa por componente. Trocas rápidas de cor indicam mau contato ou fio frouxo.</p>
      <StatusLanes history={history} lanes={lanes} />
      <ul className="legend" aria-label="Legenda">
        {LANE_LEGEND.map((spec) => {
          const Icon = spec.icon;
          return <li key={spec.text}><span className={`swatch tone-${spec.tone}`}><Icon weight="bold" aria-hidden="true" /></span>{spec.text}</li>;
        })}
      </ul>
    </Widget>
  );
}

interface SensorsProps {
  state: DashboardState;
  byCode: ByCode;
  spark: Partial<Record<SensorCode, SparkPoint[]>>;
  history: HistoryPoint[];
}

// Separados pelo papel: o que decide a luz vem antes do que só monitora
function SensorsView({ state, byCode, spark, history }: SensorsProps) {
  const testRunning = state.components.some((c) => c.test.state === "RUNNING");
  const installed = SENSOR_ORDER.filter((code) => byCode[code].health !== "NOT_AVAILABLE");
  const missing = SENSOR_ORDER.filter((code) => byCode[code].health === "NOT_AVAILABLE")
    .map((code) => `${SENSORS[code].title} (${SENSORS[code].part})`);
  const groups: [string, SensorCode[]][] = [
    ["Decidem a luz", installed.filter((code) => SENSORS[code].lighting)],
    ["Monitoram o ambiente", installed.filter((code) => !SENSORS[code].lighting)],
  ];

  return (
    <>
      {groups.filter(([, codes]) => codes.length).map(([title, codes]) => (
        <section key={title} className="group" aria-label={title}>
          <h2 className="group-title">{title}</h2>
          <div className="cards">
            {codes.map((code, i) => (
              <SensorCard key={code} index={i} code={code} c={byCode[code] as never} d={state} series={spark[code] ?? []} testRunning={testRunning} />
            ))}
          </div>
        </section>
      ))}
      <div className="home-grid">
        <Stability byCode={byCode} history={history} />
      </div>
      {missing.length > 0 && (
        <p className="sky-note">Não instalados nesta montagem: {missing.join(", ")}. Para usar, habilite em AppConfig.h e defina os pinos em PinMap.h.</p>
      )}
    </>
  );
}

const axisNote = <div className="axis-note"><span>-5 min</span><span>agora</span></div>;

// Energia: o que a luz está fazendo, por quê, quanto economiza e como variou nos últimos minutos
function EnergyView({ state, byCode, history, energy }: { state: DashboardState; byCode: ByCode; history: HistoryPoint[]; energy: EnergyTally }) {
  const cfg = state.config;
  return (
    <div className="home-grid history-grid">
      <LampsWidget state={state} />
      <WhyWidget state={state} byCode={byCode} />
      <EnergyWidget energy={energy} />

      <Widget color="#7a5bd6" icon={ChartLineIcon} title="Intensidade e modo" waves={false} className="span-2">
        <LineChart
          history={history} min={0} max={100} ticks={[0, 25, 50, 75, 100]} color="--series-2"
          value={(p) => p.light} band={(p) => MODE_COLOR[p.mode]}
          tooltip={(p) => `${p.light}%, ${MODE_TEXT[p.mode] ?? p.mode}${p.presence ? ", presença" : ""}`}
          label="Gráfico da intensidade das luminárias com a faixa do modo de operação"
        />
        {axisNote}
        <ul className="legend" aria-label="Legenda dos modos">
          {MODE_ORDER.map((m) => <li key={m}><span className="swatch" style={{ background: `var(${MODE_COLOR[m]})` }} />{MODE_TEXT[m]}</li>)}
        </ul>
      </Widget>

      <Widget color="#e9a10f" icon={SunHorizonIcon} title="Luz ambiente (LDR)" waves={false}>
        <LineChart
          history={history} min={0} max={4095} ticks={[0, 1000, 2000, 3000, 4000]} color="--series-1"
          value={(p) => p.ambient}
          thresholds={[
            { v: cfg.day_threshold, label: `apaga > ${cfg.day_threshold}`, above: true },
            { v: cfg.night_threshold, label: `acende < ${cfg.night_threshold}`, above: false },
            { v: cfg.dark_threshold, label: `noite fechada < ${cfg.dark_threshold}`, above: false },
          ]}
          tooltip={(p) => `LDR ${p.ambient ?? "sem leitura"}, ${p.period}`}
          label="Gráfico da leitura do LDR ambiente"
        />
        {axisNote}
      </Widget>

      {byCode.LDR_LOCAL.health !== "NOT_AVAILABLE" && (
        <Widget color="#e8781e" icon={LampIcon} title="A luminária 1 acende de verdade?" waves={false} className="span-all">
          <p className="widget-note">Em cima, o que o ESP32 manda; embaixo, o que o LDR encostado no LED mede. Se um sobe e o outro não, o LED ou o fio falhou.</p>
          <h3 className="chart-title">Comando enviado ao LED 1 (%)</h3>
          <LineChart
            history={history} min={0} max={100} ticks={[0, 50, 100]} color="--series-2" size="sm"
            value={(p) => p.light} tooltip={(p) => `comando ${p.light}%`} label="Comando enviado ao LED 1"
          />
          <h3 className="chart-title">Luz medida no poste (LDR, ADC)</h3>
          <LineChart
            history={history} min={0} max={4095} ticks={[0, 2000, 4000]} color="--series-1" size="sm"
            value={(p) => p.lampLdr} tooltip={(p) => `LDR do poste ${p.lampLdr ?? "sem leitura"}`} label="Luz medida pelo LDR encostado no LED 1"
          />
          {axisNote}
        </Widget>
      )}
    </div>
  );
}

// Manutenção: pendências, sensores com testes práticos, estabilidade dos fios e o sistema
function MaintenanceView({ state, byCode, spark, history, issues, go }: SensorsProps & { issues: Issue[]; go: (t: TabId, c?: Code) => void }) {
  const cfg = state.config;
  return (
    <>
      <div className="home-grid">
        <IssuesWidget state={state} issues={issues} go={go} />
      </div>
      <SensorsView state={state} byCode={byCode} spark={spark} history={history} />
      <div className="home-grid history-grid">
        {byCode.ULTRASONIC.health !== "NOT_AVAILABLE" && (
          <Widget color="#1e90e0" icon={PersonSimpleWalkIcon} title="Onde a presença foi detectada" waves={false} className="span-2">
            <p className="widget-note">Cada ponto é uma medição do HC-SR04. Pontos cheios contaram como presença; anéis ficaram fora da faixa. Use para ajustar a faixa no AppConfig.h.</p>
            <DistanceChart history={history} minCm={cfg.min_distance_cm} maxCm={cfg.max_distance_cm} />
            {axisNote}
          </Widget>
        )}
        <Widget color="#16adc2" icon={TableIcon} title="Amostras" waves={false}>
          <details className="table-details">
            <summary>Ver como tabela (últimas 20 amostras)</summary>
            <div className="table-scroll">
              <table>
                <thead><tr><th>Horário</th><th>LDR ambiente</th><th>Período</th><th>Presença</th><th>Luminárias</th><th>Modo</th></tr></thead>
                <tbody>
                  {history.slice(-20).reverse().map((p) => (
                    <tr key={p.t}>
                      <td>{clock(p.t)}</td><td>{p.ambient ?? EMPTY}</td><td>{p.period}</td>
                      <td>{p.presence ? "sim" : "não"}</td><td>{p.light}%</td><td>{MODE_TEXT[p.mode] ?? p.mode}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </details>
          <p className="widget-note">Leituras de LDR e do microfone são relativas (ADC e dB do conversor) e não equivalem a lux ou dB SPL sem calibração.</p>
        </Widget>
        <SystemPanel state={state} byCode={byCode} />
      </div>
    </>
  );
}

function greeting() {
  const h = new Date().getHours();
  return h < 5 ? "Boa noite" : h < 12 ? "Bom dia" : h < 18 ? "Boa tarde" : "Boa noite";
}

const today = () => new Date().toLocaleDateString("pt-BR", { weekday: "long", day: "numeric", month: "long" });

// Título e ação principal de cada tela, postos sobre o céu como nas janelas Frutiger
function Header({ tab, state, byCode, issues, history, go }: {
  tab: TabId; state: DashboardState; byCode: ByCode; issues: Issue[]; history: HistoryPoint[]; go: (t: TabId, c?: Code) => void;
}) {
  const L = state.lighting;
  const pend = issues.length ? `${issues.length} pendência${issues.length === 1 ? "" : "s"}` : "tudo funcionando";
  // Pendências aparecem em qualquer tela; o botão leva direto ao item em Manutenção
  const first = issues[0];
  const resolve = first && (
    <button type="button" className="btn btn-gel" onClick={() => go(first.tab, first.code)}>
      Resolver {pend} <ArrowRightIcon weight="bold" aria-hidden="true" />
    </button>
  );
  if (tab === "rua") {
    const dht = byCode.DHT11.data;
    const weather = byCode.DHT11.link?.state === "CONNECTED" && dht.temperature_c !== null
      ? <><span className="num">{fmtN(dht.temperature_c, 1)} °C</span> e <span className="num">{fmtN(dht.humidity_percent)}%</span> de umidade</>
      : "sem leitura de clima";
    return <ScreenHeader title={`${greeting()}, ${state.device}`} subtitle={<>{today()}, {weather}</>} actions={resolve} />;
  }
  if (tab === "energia") {
    const span = history.length ? history[history.length - 1].t - history[0].t : 0;
    return (
      <ScreenHeader
        title="Energia"
        subtitle={<>Luz em <span className="num">{L.zones[0]}%</span>, {MODE_TEXT[L.mode]?.toLowerCase()}. Gráficos dos últimos 5 minutos (há {fmtDur(span)})</>}
        actions={resolve}
      />
    );
  }
  const installed = SENSOR_ORDER.filter((code) => byCode[code].health !== "NOT_AVAILABLE");
  const tested = installed.filter((code) => byCode[code].test.state === "PASSED").length;
  const running = installed.some((code) => byCode[code].test.state === "RUNNING");
  const next = installed.find((code) => byCode[code].link?.state === "CONNECTED" && byCode[code].test.state !== "PASSED");
  return (
    <ScreenHeader
      title="Manutenção"
      subtitle={<>{pend}, {installed.length} sensores instalados, {tested} com teste prático aprovado</>}
      actions={next && (
        <button type="button" className="btn btn-gel" disabled={running} onClick={() => startTest(next)}>
          <FlaskIcon weight="fill" aria-hidden="true" /> Testar {SENSORS[next].title.toLowerCase()}
        </button>
      )}
    />
  );
}

export function App() {
  const { state, byCode, link, history, spark, energy } = useTelemetry();
  const { tab, go } = useNav();
  const { street, reset: resetStreet } = useStreet();
  const issues = useMemo(() => (byCode ? collectIssues(byCode) : []), [byCode]);

  return (
    <div className="app">
      <Titlebar state={state} link={link} />
      <div className="workspace">
        <Sidebar tab={tab} go={go} issues={issues} byCode={byCode} />
        <main className="main">
          {!state || !byCode ? (
            <>
              <ScreenHeader title="Conectando ao poste" subtitle="O painel aparece assim que chegar a primeira leitura." />
              <Loading />
            </>
          ) : (
            // A chave reinicia a entrada em cascata a cada troca de tela
            <div className="view" key={tab}>
              <Header tab={tab} state={state} byCode={byCode} issues={issues} history={history} go={go} />
              {tab === "rua" && <StreetView state={state} byCode={byCode} street={street} reset={resetStreet} />}
              {tab === "energia" && <EnergyView state={state} byCode={byCode} history={history} energy={energy} />}
              {tab === "manutencao" && <MaintenanceView state={state} byCode={byCode} spark={spark} history={history} issues={issues} go={go} />}
            </div>
          )}
          <StatusBar state={state} link={link} />
        </main>
      </div>
      {byCode && <TestDock byCode={byCode} />}
    </div>
  );
}
