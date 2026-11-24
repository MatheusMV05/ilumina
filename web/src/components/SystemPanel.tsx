import type { CSSProperties } from "react";
import type { Icon } from "@phosphor-icons/react";
import { CloudArrowUpIcon, CpuIcon, EyeIcon, LightbulbIcon, MonitorIcon, PowerIcon, SlidersHorizontalIcon, WifiHighIcon } from "@phosphor-icons/react";

import { HEALTH } from "../sensors";
import type { ByCode, ComponentState, DashboardState, SystemCode } from "../types";
import { confirmVisual } from "../useTelemetry";
import { Badge } from "./StatusOrb";
import { Widget } from "./Widget";

interface SystemMeta {
  title: string;
  color: string;
  icon: Icon;
  confirm?: string;
  text: (c: ComponentState, d: DashboardState) => string;
}

const SYSTEM: Record<SystemCode, SystemMeta> = {
  OLED: {
    title: "Tela OLED", color: "#16adc2", icon: MonitorIcon,
    text: (c) => c.health === "OK" ? `Encontrada no I2C (${c.note.replace("endereco ", "")})` : "Não encontrada: confira SDA 21, SCL 22, VCC e GND.",
  },
  LEDS: {
    title: "Luminárias (4 LEDs)", color: "#e9a10f", icon: LightbulbIcon, confirm: "Vi os 4 LEDs acenderem",
    text: (c) => {
      if (c.health === "OK") return c.note.includes("usuario") ? "Confirmado por você." : "Os 4 LEDs responderam à verificação elétrica.";
      if (c.health === "WARNING" || c.health === "FAILURE") {
        return `${c.note}. Aceso, ele não puxou corrente: confira se está no lugar, a polaridade (perna maior no GPIO) e o resistor. Use "Verificar LEDs" em Energia depois de ajustar.`;
      }
      const data = (c as ComponentState<"LEDS">).data;
      const delta = data.lamp1_optical_delta;
      // Pino de saída não sente se há LED ligado: só o LDR encostado no LED 1 mede algo
      const optical = delta === null ? " O ESP32 não consegue medir os LEDs agora: o LDR do poste (GPIO 32) não está ligado." : delta >= data.lamp1_optical_min
        ? ` O LED 1 foi confirmado por medição: o LDR do poste subiu ${delta} ADC quando ele acendeu.`
        : ` O LDR do poste não percebeu o LED 1 acender (variação de ${delta} ADC): ele está encostado no LED?`;
      return "No boot, os LEDs acendem um por vez (GPIO 25, 26, 27 e 13). Você viu os quatro acenderem?" + optical;
    },
  },
  STATUS_LED: {
    title: "LED de status (GPIO 19)", color: "#3cb54a", icon: PowerIcon, confirm: "O LED de status está aceso",
    text: (c) => c.health === "OK" ? "Confirmado por você." : "Aceso = firmware rodando. Confirme olhando o LED.",
  },
  WIFI: {
    title: "Wi-Fi do roteador", color: "#1e90e0", icon: WifiHighIcon,
    text: (_c, d) => !d.network.wifi_configured ? "Não configurado: o poste funciona em modo local." : d.network.wifi ? "Conectado." : "Desconectado, tentando de novo.",
  },
  MQTT: {
    title: "MQTT (telemetria)", color: "#7a5bd6", icon: CloudArrowUpIcon,
    text: (_c, d) => !d.network.mqtt_configured ? "Desativado." : d.network.mqtt ? "Publicando no broker." : "Broker indisponível.",
  },
};

// Regras de iluminação em blocos legíveis, sem precisar abrir o AppConfig.h
function Rules({ cfg }: { cfg: DashboardState["config"] }) {
  const rules: [string, string][] = [
    ["Vira noite", `LDR abaixo de ${cfg.night_threshold}`],
    ["Vira dia", `LDR acima de ${cfg.day_threshold}`],
    ["Noite sem ninguém", `${cfg.dusk_percent}% no anoitecer até ${cfg.night_percent}% abaixo de ${cfg.dark_threshold}`],
    ["Noite com presença", `${cfg.active_percent}% de luz`],
    ["Depois da presença", `mantém por ${cfg.hold_ms / 1000} s`],
    ["Faixa de presença", `${cfg.min_distance_cm} a ${cfg.max_distance_cm} cm`],
  ];
  return (
    <Widget color="#1e90e0" icon={SlidersHorizontalIcon} title="Regras de iluminação" className="span-2">
      <p className="widget-note">Configuradas no firmware, em AppConfig.h. Sem leitura de luz ou sem sensor de presença, a luz fica no máximo por segurança.</p>
      <dl className="rules">
        {rules.map(([k, v]) => (
          <div className="rule" key={k}><dt>{k}</dt><dd className="num">{v}</dd></div>
        ))}
      </dl>
    </Widget>
  );
}

export function SystemPanel({ state, byCode }: { state: DashboardState; byCode: ByCode }) {
  return (
    <>
      <Widget color="#e8781e" icon={CpuIcon} title="Componentes do poste" waves={false} className="span-2">
        <div className="mini-cards">
          {(Object.keys(SYSTEM) as SystemCode[]).map((code, i) => {
            const meta = SYSTEM[code];
            const c = byCode[code] as ComponentState;
            const MetaIcon = meta.icon;
            return (
              <div className="mini" id={`c-${code}`} key={code} style={{ "--i": i, "--chip-color": meta.color } as CSSProperties}>
                <div className="mini-head">
                  <span className="chip chip-sm" aria-hidden="true"><MetaIcon weight="fill" /></span>
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
      </Widget>
      <Rules cfg={state.config} />
    </>
  );
}
