import { Banner } from "./components/Banner";
import { LineChart } from "./components/Charts";
import { DecisionFlow } from "./components/DecisionFlow";
import { Header } from "./components/Header";
import { SensorCard } from "./components/SensorCard";
import { SystemPanel } from "./components/SystemPanel";
import { EMPTY, clock, fmtDur } from "./format";
import { MODE_TEXT, SENSORS, SENSOR_ORDER } from "./sensors";
import { useTelemetry } from "./useTelemetry";

// Esqueleto com a mesma forma do painel enquanto o primeiro JSON não chega
function Loading() {
  return (
    <div className="loading" aria-busy="true" aria-label="Carregando dados do poste">
      <div className="glass skeleton sk-banner"><span className="sk-orb" /><div><span className="sk-line w40" /><span className="sk-line w70" /></div></div>
      <div className="glass skeleton sk-flow">{[0, 1, 2, 3].map((i) => <span key={i} className="sk-block" />)}</div>
      <div className="sk-cards">{[0, 1, 2].map((i) => <div key={i} className="glass skeleton sk-card" />)}</div>
      <p className="loading-note">Aguardando dados do poste. Conecte-se à rede Wi-Fi <strong>Iluminacao-IoT</strong> para ver o estado do sistema.</p>
    </div>
  );
}

export function App() {
  const { state, byCode, link, history, spark } = useTelemetry();

  if (!state || !byCode) {
    return (
      <main>
        <Header state={null} link={link} />
        <Loading />
      </main>
    );
  }

  const testRunning = state.components.some((c) => c.test.state === "RUNNING");
  const installed = SENSOR_ORDER.filter((code) => byCode[code].health !== "NOT_AVAILABLE");
  // Sensores fora desta montagem não ganham cartão, só uma menção
  const missing = SENSOR_ORDER.filter((code) => byCode[code].health === "NOT_AVAILABLE")
    .map((code) => `${SENSORS[code].title} (${SENSORS[code].part})`);
  const span = history.length ? history[history.length - 1].t - history[0].t : 0;
  const cfg = state.config;

  return (
    <main>
      <Header state={state} link={link} />
      <Banner state={state} byCode={byCode} />
      <DecisionFlow state={state} byCode={byCode} />

      <section className="glass panel reveal" aria-labelledby="t-sensors">
        <h2 id="t-sensors">Sensores</h2>
        <p className="section-sub">
          Cada sensor passa por três verificações: <strong>conectado</strong> (o firmware detecta o hardware no pino),{" "}
          <strong>medindo</strong> (o sinal é coerente) e <strong>teste prático</strong> (você provoca uma reação e o sensor responde).
        </p>
        <div className="cards">
          {installed.map((code) => (
            <SensorCard key={code} code={code} c={byCode[code] as never} d={state} series={spark[code] ?? []} testRunning={testRunning} />
          ))}
        </div>
        {missing.length > 0 && (
          <p className="not-installed">
            Não instalados nesta montagem: {missing.join(", ")}. Para usar, habilite em AppConfig.h e defina os pinos em PinMap.h.
          </p>
        )}
      </section>

      <section className="glass panel reveal" aria-labelledby="t-history">
        <h2 id="t-history">Histórico da iluminação (últimos 5 minutos)</h2>
        <div className="charts">
          <div>
            <h3>Luz ambiente, leitura do LDR (relativa, não lux)</h3>
            <LineChart
              history={history} min={0} max={4095} ticks={[0, 1000, 2000, 3000, 4000]} color="--series-1"
              value={(p) => p.ambient}
              thresholds={[
                { v: cfg.day_threshold, label: `vira dia > ${cfg.day_threshold}`, above: true },
                { v: cfg.night_threshold, label: `vira noite < ${cfg.night_threshold}`, above: false },
              ]}
              tooltip={(p) => `LDR ${p.ambient ?? "sem leitura"}, ${p.period}`}
              label="Gráfico da leitura do LDR ambiente"
            />
            <div className="axis-note"><span>-{fmtDur(span)}</span><span>agora</span></div>
          </div>
          <div>
            <h3>Intensidade das luminárias (%)</h3>
            <LineChart
              history={history} min={0} max={100} ticks={[0, 25, 50, 75, 100]} color="--series-2"
              value={(p) => p.light}
              tooltip={(p) => `${p.light}%, ${MODE_TEXT[p.mode] ?? p.mode}${p.presence ? ", presença" : ""}`}
              label="Gráfico da intensidade das luminárias"
            />
            <div className="axis-note"><span>-{fmtDur(span)}</span><span>agora</span></div>
          </div>
        </div>
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
      </section>

      <SystemPanel state={state} byCode={byCode} />

      <p className="footnote">
        Leituras de LDR e do microfone são relativas (ADC/dB do conversor) e não equivalem a lux ou dB SPL sem calibração.
        O ESP32 só consegue medir o LED 1, pelo LDR do poste encostado nele; os demais LEDs são confirmados por você.
      </p>
    </main>
  );
}
