import type { Icon } from "@phosphor-icons/react";
import { CloudArrowUpIcon, EyeIcon, LightbulbIcon, MonitorIcon, PowerIcon, WifiHighIcon } from "@phosphor-icons/react";

import { HEALTH } from "../sensors";
import type { ByCode, ComponentState, DashboardState, SystemCode } from "../types";
import { confirmVisual } from "../useTelemetry";
import { Badge } from "./StatusOrb";

interface SystemMeta {
  title: string;
  icon: Icon;
  confirm?: string;
  text: (c: ComponentState, d: DashboardState) => string;
}

const SYSTEM: Record<SystemCode, SystemMeta> = {
  OLED: {
    title: "Tela OLED", icon: MonitorIcon,
    text: (c) => c.health === "OK" ? `Encontrada no I2C (${c.note.replace("endereco ", "")})` : "Não encontrada: confira SDA 21, SCL 22, VCC e GND.",
  },
  LEDS: {
    title: "Luminárias (4 LEDs)", icon: LightbulbIcon, confirm: "Vi os 4 LEDs acenderem",
    text: (c) => {
      if (c.health === "OK") return "Confirmado por você.";
      const data = (c as ComponentState<"LEDS">).data;
      const delta = data.lamp1_optical_delta;
      const optical = delta === null ? "" : delta >= data.lamp1_optical_min
        ? ` O LED 1 foi confirmado por medição: o LDR do poste subiu ${delta} ADC quando ele acendeu.`
        : ` O LDR do poste não percebeu o LED 1 acender (variação de ${delta} ADC): ele está encostado no LED?`;
      return "No boot, os LEDs acendem um por vez (GPIO 25, 26, 27 e 13). Você viu os quatro acenderem?" + optical;
    },
  },
  STATUS_LED: {
    title: "LED de status (GPIO 19)", icon: PowerIcon, confirm: "O LED de status está aceso",
    text: (c) => c.health === "OK" ? "Confirmado por você." : "Aceso = firmware rodando. Confirme olhando o LED.",
  },
  WIFI: {
    title: "Wi-Fi do roteador", icon: WifiHighIcon,
    text: (_c, d) => !d.network.wifi_configured ? "Não configurado: o poste funciona em modo local." : d.network.wifi ? "Conectado." : "Desconectado, tentando de novo.",
  },
  MQTT: {
    title: "MQTT (telemetria)", icon: CloudArrowUpIcon,
    text: (_c, d) => !d.network.mqtt_configured ? "Desativado." : d.network.mqtt ? "Publicando no broker." : "Broker indisponível.",
  },
};

export function SystemPanel({ state, byCode }: { state: DashboardState; byCode: ByCode }) {
  return (
    <section className="glass panel reveal" aria-labelledby="t-system">
      <h2 id="t-system">Sistema</h2>
      <p className="section-sub">Tela, luminárias, indicador e conexões.</p>
      <div className="mini-cards">
        {(Object.keys(SYSTEM) as SystemCode[]).map((code) => {
          const meta = SYSTEM[code];
          const c = byCode[code] as ComponentState;
          const MetaIcon = meta.icon;
          return (
            <div className="mini" key={code}>
              <div className="mini-head">
                <span className="card-icon sm" aria-hidden="true"><MetaIcon weight="duotone" /></span>
                <h3>{meta.title}</h3>
              </div>
              <Badge spec={HEALTH[c.health] ?? HEALTH.NOT_TESTED} />
              <p className="note">{meta.text(c, state)}</p>
              {meta.confirm && c.health !== "OK" && (
                <button type="button" className="btn btn-aqua" onClick={() => confirmVisual(code)}>
                  <EyeIcon weight="bold" aria-hidden="true" />
                  {meta.confirm}
                </button>
              )}
            </div>
          );
        })}
      </div>
    </section>
  );
}
