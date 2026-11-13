import { CheckIcon, WarningIcon } from "@phosphor-icons/react";

import { HEALTH, SENSORS, SENSOR_ORDER, reasonText, type StatusSpec } from "../sensors";
import type { ByCode, DashboardState } from "../types";
import { Orb } from "./StatusOrb";

function pendingActions(byCode: ByCode): string[] {
  const actions: string[] = [];
  for (const code of SENSOR_ORDER) {
    const c = byCode[code];
    const m = SENSORS[code];
    if (c.health === "FAILURE") actions.push(`${m.title} (${m.part}, ${m.pins}): ${reasonText(code, c.link?.reason)}`);
    else if (c.health === "WARNING") {
      actions.push(`${m.title}: ${c.link?.state === "CONNECTED" ? "não reagiu ao teste prático. Tente de novo." : reasonText(code, c.link?.reason)}`);
    } else if (c.health === "NOT_TESTED" && code === "IR") actions.push("Obstáculo (IR): passe a mão na frente para confirmar que funciona.");
  }
  if (byCode.LEDS.health === "VISUAL_CHECK") actions.push("Luminárias: confirme em “Sistema” se os 4 LEDs acenderam no teste de inicialização.");
  if (byCode.OLED.health === "FAILURE") actions.push("Tela OLED não encontrada: confira SDA (21), SCL (22), VCC e GND.");
  return actions;
}

export function Banner({ state, byCode }: { state: DashboardState; byCode: ByCode }) {
  const actions = pendingActions(byCode);
  let spec: StatusSpec;
  let title: string;
  let text: string;

  if (state.summary === "SAFE_MODE") {
    spec = { tone: "critical", icon: WarningIcon, text: "" };
    title = "Modo seguro: luzes no máximo";
    text = state.lighting.is_night === null
      ? "Sem leitura confiável da luz ambiente. Por segurança, o sistema não assume que é dia."
      : !state.lighting.presence_available
        ? "Nenhum sensor de presença foi confirmado. À noite, a luz fica no máximo até um deles funcionar."
        : "Uma verificação essencial falhou.";
  } else if (state.summary === "ALERTS") {
    spec = { tone: "warning", icon: WarningIcon, text: "" };
    title = `Funcionando, com ${actions.length} pendência${actions.length === 1 ? "" : "s"}`;
    text = "A iluminação está operando normalmente. Os itens abaixo precisam de atenção.";
  } else {
    spec = { ...HEALTH.OK, icon: CheckIcon };
    title = "Tudo funcionando";
    text = "Todos os sensores essenciais estão conectados e respondendo.";
  }

  return (
    <section className={`glass banner tone-${spec.tone} reveal`} aria-live="polite">
      <Orb spec={spec} size="lg" />
      <div className="banner-body">
        <h2>{title}</h2>
        <p>{text}</p>
        {actions.length > 0 && (
          <ul className="actions">
            {actions.slice(0, 6).map((a) => <li key={a}>{a}</li>)}
          </ul>
        )}
      </div>
    </section>
  );
}
