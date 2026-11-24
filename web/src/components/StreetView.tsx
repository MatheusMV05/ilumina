import { useState, type ReactNode } from "react";
import type { Icon } from "@phosphor-icons/react";
import {
  ClockIcon, DropIcon, PersonSimpleWalkIcon, SpeakerHighIcon, StorefrontIcon, ThermometerSimpleIcon, TrashIcon,
} from "@phosphor-icons/react";

import { fmtDur, fmtN } from "../format";
import type { ByCode, DashboardState, StreetHour, StreetStats } from "../types";
import { HourBars } from "./Charts";
import { Widget } from "./Widget";

// Hora com menos de 5 minutos observados ainda não diz nada sobre a rua
const MIN_COVERAGE_S = 300;

const covered = (h: StreetHour) => h.s >= MIN_COVERAGE_S;
// Passagens por hora: normaliza pelo tempo observado, porque nem toda hora foi vista o mesmo número de vezes
const passagesPerHour = (h: StreetHour) => (covered(h) ? h.p / (h.s / 3600) : null);
const noiseOf = (h: StreetHour) => (covered(h) ? h.n : null);
const hourLabel = (h: number) => `${h}h`;
const span = (h: number) => `${h}h às ${(h + 1) % 24}h`;

function busyWord(v: number, max: number) {
  if (max <= 0) return "calma";
  const r = v / max;
  return r >= 0.66 ? "movimentada" : r >= 0.33 ? "moderada" : "calma";
}

function noiseWord(db: number) {
  return db < 4 ? "silêncio" : db < 12 ? "som moderado" : "som alto";
}

function extreme(values: (number | null)[], pick: "max" | "min") {
  let best: number | null = null;
  values.forEach((v, h) => {
    if (v === null) return;
    if (best === null || (pick === "max" ? v > values[best]! : v < values[best]!)) best = h;
  });
  return best;
}

function Tile({ icon: TileIcon, color, label, value, unit, note }: {
  icon: Icon; color: string; label: string; value: ReactNode; unit?: string; note: ReactNode;
}) {
  return (
    <div className="street-tile" style={{ "--chip-color": color } as React.CSSProperties}>
      <span className="chip chip-sm" aria-hidden="true"><TileIcon weight="fill" /></span>
      <span className="street-tile-label">{label}</span>
      <span className="street-tile-value num">{value}{unit && <small>{unit}</small>}</span>
      <span className="street-tile-note">{note}</span>
    </div>
  );
}

// O que os sensores dizem da rua neste instante
function NowWidget({ state, byCode, street }: { state: DashboardState; byCode: ByCode; street: StreetStats | null }) {
  const dht = byCode.DHT11;
  const mic = byCode.MIC;
  const L = state.lighting;
  const dhtOk = dht.link?.state === "CONNECTED" && dht.data.temperature_c !== null;
  const micOk = mic.link?.state === "CONNECTED" && mic.data.dbfs !== null;
  const above = micOk ? Math.max(0, mic.data.dbfs! - (mic.data.floor_dbfs ?? mic.data.dbfs!)) : null;
  const moving = L.presence_hc || L.presence_ir;
  const presenceOk = L.presence_available;
  const hourNow = street?.hour ?? null;
  const typical = hourNow !== null && street ? passagesPerHour(street.hours[hourNow]) : null;

  return (
    <Widget color="#16adc2" icon={StorefrontIcon} title="Agora na rua" className="span-2">
      <div className="street-tiles">
        <Tile
          icon={ThermometerSimpleIcon} color="#e8781e" label="Temperatura"
          value={dhtOk ? fmtN(dht.data.temperature_c, 1) : "sem sinal"} unit={dhtOk ? " °C" : undefined}
          note={dhtOk && dht.data.heat_index_c !== null ? `sensação de ${fmtN(dht.data.heat_index_c, 1)} °C` : "DHT11 sem leitura"}
        />
        <Tile
          icon={DropIcon} color="#1e90e0" label="Umidade"
          value={dhtOk ? fmtN(dht.data.humidity_percent) : "sem sinal"} unit={dhtOk ? "%" : undefined}
          note={!dhtOk ? "DHT11 sem leitura" : (dht.data.humidity_percent ?? 0) >= 80 ? "ar muito úmido" : (dht.data.humidity_percent ?? 0) <= 30 ? "ar seco" : "confortável"}
        />
        <Tile
          icon={SpeakerHighIcon} color="#7a5bd6" label="Barulho"
          value={above === null ? "sem sinal" : fmtN(above)} unit={above === null ? undefined : " dB"}
          note={above === null ? "microfone sem leitura" : `${noiseWord(above)}, acima do silêncio`}
        />
        <Tile
          icon={PersonSimpleWalkIcon} color="#22a861" label="Movimento"
          value={!presenceOk ? "sem sensor" : moving ? "alguém passando" : "rua vazia"}
          note={typical !== null ? `nesta hora passam ~${fmtN(typical)} por hora` : "ainda sem média para esta hora"}
        />
      </div>
      <p className="widget-note">Barulho é medido em relação ao silêncio da própria rua (o KY-038 não mede dB absolutos) e não conta como presença.</p>
    </Widget>
  );
}

function SummaryWidget({ street, reset }: { street: StreetStats | null; reset: () => void }) {
  const [confirming, setConfirming] = useState(false);
  if (!street) {
    return (
      <Widget color="#1e90e0" icon={ClockIcon} title="Um dia na rua">
        <p className="widget-note">Buscando as estatísticas guardadas no poste.</p>
      </Widget>
    );
  }
  const rate = street.hours.map(passagesPerHour);
  const noise = street.hours.map(noiseOf);
  const busiest = extreme(rate, "max");
  const calmest = extreme(rate, "min");
  const loudest = extreme(noise, "max");
  const observed = street.hours.reduce((a, h) => a + h.s, 0);
  const coveredHours = street.hours.filter(covered).length;
  const since = street.since ? new Date(street.since * 1000).toLocaleDateString("pt-BR", { day: "numeric", month: "long" }) : null;

  return (
    <Widget color="#1e90e0" icon={ClockIcon} title="Um dia na rua">
      {!street.clock_valid ? (
        <p className="widget-note">Acertando o relógio do poste com a hora deste aparelho. As estatísticas começam em seguida.</p>
      ) : (
        <ul className="rows">
          <li className="row-pill">
            <span className="row-label">Mais movimentada</span>
            <span className="tag num">{busiest === null ? "sem dados" : span(busiest)}</span>
          </li>
          <li className="row-pill">
            <span className="row-label">Mais calma</span>
            <span className="tag num">{calmest === null ? "sem dados" : span(calmest)}</span>
          </li>
          <li className="row-pill">
            <span className="row-label">Mais barulhenta</span>
            <span className="tag num">{loudest === null ? "sem dados" : span(loudest)}</span>
          </li>
        </ul>
      )}
      <p className="widget-note">
        {observed > 0
          ? <>{fmtDur(observed * 1000)} de observação{since ? ` desde ${since}` : ""}, cobrindo {coveredHours} de 24 horários. Quanto mais dias, mais confiável.</>
          : "Ainda sem observações. O poste registra sozinho, mesmo com o painel fechado."}
      </p>
      <div className="street-reset">
        {!confirming ? (
          <button type="button" className="link-btn" onClick={() => setConfirming(true)}>
            <TrashIcon weight="bold" aria-hidden="true" /> Zerar estatísticas
          </button>
        ) : (
          <>
            <span>Apagar todo o histórico da rua?</span>
            <button type="button" className="btn btn-small" onClick={() => setConfirming(false)}>Cancelar</button>
            <button type="button" className="btn btn-small btn-danger" onClick={() => { setConfirming(false); reset(); }}>Apagar</button>
          </>
        )}
      </div>
    </Widget>
  );
}

const noData = "ainda sem dados nesta hora";

export function StreetView({ state, byCode, street, reset }: {
  state: DashboardState; byCode: ByCode; street: StreetStats | null; reset: () => void;
}) {
  const hours = street?.hours ?? [];
  const current = street?.hour ?? null;
  const rate = hours.map(passagesPerHour);
  const maxRate = Math.max(0, ...rate.filter((v): v is number => v !== null));
  const noise = hours.map(noiseOf);
  const temp = hours.map((h) => (covered(h) ? h.t : null));
  const hum = hours.map((h) => (covered(h) ? h.u : null));
  const ready = hours.length === 24;

  return (
    <div className="home-grid">
      <NowWidget state={state} byCode={byCode} street={street} />
      <SummaryWidget street={street} reset={reset} />

      {ready && (
        <>
          <Widget color="#22a861" icon={PersonSimpleWalkIcon} title="Movimento por horário" waves={false} className="span-all">
            <p className="widget-note">Média de passagens por hora em frente ao sensor, juntando todos os dias observados. Horas tracejadas ainda não foram vistas.</p>
            <HourBars
              values={rate} current={current} color="--series-3" unit="/h"
              tooltip={(h, v) => v === null ? `${span(h)}: ${noData}` : `${span(h)}: ~${fmtN(v, 1)} passagens por hora, rua ${busyWord(v, maxRate)}, movimento em ${fmtN((hours[h].m / hours[h].s) * 100)}% do tempo`}
              label="Média de passagens por hora em cada horário do dia"
            />
          </Widget>

          <Widget color="#7a5bd6" icon={SpeakerHighIcon} title="Barulho por horário" waves={false} className="span-2">
            <p className="widget-note">Nível médio acima do silêncio da rua. Acima de {street?.loud_db ?? 12} dB conta como som alto.</p>
            <HourBars
              values={noise} current={current} color="--series-4" unit=" dB"
              tooltip={(h, v) => v === null ? `${span(h)}: ${noData}` : `${span(h)}: média de ${fmtN(v, 1)} dB, som alto em ${fmtN((hours[h].l ?? 0) * 100)}% do tempo, pico de ${fmtN(hours[h].pk)} dB`}
              label="Barulho médio acima do silêncio em cada horário do dia"
            />
          </Widget>

          <Widget color="#e8781e" icon={ThermometerSimpleIcon} title="Clima por horário" waves={false}>
            <h3 className="chart-title">Temperatura média (°C)</h3>
            <HourBars
              values={temp} current={current} color="--series-2" unit="°" size="sm"
              tooltip={(h, v) => v === null ? `${hourLabel(h)}: ${noData}` : `${span(h)}: ${fmtN(v, 1)} °C`}
              label="Temperatura média em cada horário do dia"
            />
            <h3 className="chart-title">Umidade média (%)</h3>
            <HourBars
              values={hum} current={current} color="--series-1" unit="%" max={100} size="sm"
              tooltip={(h, v) => v === null ? `${hourLabel(h)}: ${noData}` : `${span(h)}: ${fmtN(v)}% de umidade`}
              label="Umidade média em cada horário do dia"
            />
          </Widget>
        </>
      )}
    </div>
  );
}
