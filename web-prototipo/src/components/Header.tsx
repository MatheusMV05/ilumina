import { FlaskIcon, LightbulbFilamentIcon } from "@phosphor-icons/react";

import { EMPTY, fmtDur } from "../format";
import { HEALTH } from "../sensors";
import type { DashboardState } from "../types";
import { Badge } from "./StatusOrb";

function networkBadge(n: DashboardState["network"] | undefined) {
  if (!n) return <Badge spec={HEALTH.NOT_AVAILABLE} label="Rede" />;
  if (!n.wifi_configured) return <Badge spec={HEALTH.NOT_AVAILABLE} label="Modo local" />;
  if (n.wifi) return <Badge spec={HEALTH.OK} label={n.mqtt ? "Wi-Fi + MQTT" : "Wi-Fi conectado"} />;
  return <Badge spec={HEALTH.WARNING} label="Wi-Fi offline" />;
}

export function Header({ state }: { state: DashboardState | null }) {
  return (
    <header className="topbar glass">
      <div className="brand">
        <span className="brand-mark" aria-hidden="true">
          <LightbulbFilamentIcon weight="duotone" />
        </span>
        <div>
          <h1>Iluminação Pública IoT</h1>
          <p className="brand-sub">
            Poste <strong>{state?.device ?? EMPTY}</strong>, ligado há{" "}
            <span className="num">{state ? fmtDur(state.uptime_ms) : EMPTY}</span>
          </p>
        </div>
      </div>
      <div className="topbar-meta" aria-live="polite">
        <Badge spec={{ tone: "busy", icon: FlaskIcon, text: "Protótipo" }} />
        {networkBadge(state?.network)}
      </div>
    </header>
  );
}
