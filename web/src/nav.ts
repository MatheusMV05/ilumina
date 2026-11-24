import { useCallback, useEffect, useState } from "react";

import { SENSORS, SENSOR_ORDER, reasonText } from "./sensors";
import type { ByCode, Code } from "./types";

export type TabId = "rua" | "energia" | "manutencao";
export const TABS: TabId[] = ["rua", "energia", "manutencao"];
// Endereços da versão anterior do painel (favoritos e links salvos continuam funcionando)
const LEGACY: Record<string, TabId> = { agora: "energia", historico: "energia", sensores: "manutencao", sistema: "manutencao" };

export interface Issue {
  code: Code;
  tab: TabId;
  critical: boolean;
  title: string;
  text: string;
}

// Pendências que pedem uma ação de quem está montando o poste, cada uma ligada ao lugar onde se resolve
export function collectIssues(byCode: ByCode): Issue[] {
  const issues: Issue[] = [];
  for (const code of SENSOR_ORDER) {
    const c = byCode[code];
    const m = SENSORS[code];
    if (c.health === "FAILURE") {
      issues.push({ code, tab: "manutencao", critical: true, title: `${m.title} não conectado`, text: reasonText(code, c.link?.reason) });
    } else if (c.health === "WARNING") {
      issues.push({
        code, tab: "manutencao", critical: false, title: `${m.title} precisa de atenção`,
        text: c.link?.state === "CONNECTED" ? "Não reagiu ao teste prático. Tente de novo." : reasonText(code, c.link?.reason),
      });
    } else if (c.health === "NOT_TESTED" && code === "IR") {
      issues.push({ code, tab: "manutencao", critical: false, title: "Obstáculo (IR) sem teste", text: "Faça o teste prático para ele contar como presença." });
    }
  }
  if (byCode.LEDS.health === "WARNING" || byCode.LEDS.health === "FAILURE") {
    const missing = byCode.LEDS.note.replace(/^LEDs? sem resposta: /, "");
    issues.push({
      code: "LEDS", tab: "energia", critical: byCode.LEDS.health === "FAILURE",
      title: missing.includes(",") ? `LEDs ${missing} sem resposta` : `LED ${missing} sem resposta`,
      text: "Aceso, não puxou corrente: LED ausente, invertido, queimado ou fio solto.",
    });
  } else if (byCode.LEDS.health === "VISUAL_CHECK") {
    issues.push({ code: "LEDS", tab: "manutencao", critical: false, title: "Confirme as luminárias", text: "Diga se os 4 LEDs acenderam no teste de inicialização." });
  }
  if (byCode.STATUS_LED.health === "VISUAL_CHECK") {
    issues.push({ code: "STATUS_LED", tab: "manutencao", critical: false, title: "Confirme o LED de status", text: "Olhe se o LED do GPIO 19 está aceso." });
  }
  if (byCode.OLED.health === "FAILURE") {
    issues.push({ code: "OLED", tab: "manutencao", critical: true, title: "Tela OLED não encontrada", text: "Confira SDA (21), SCL (22), VCC e GND." });
  }
  return issues;
}

const readHash = (): TabId => {
  const h = location.hash.slice(1);
  return TABS.includes(h as TabId) ? (h as TabId) : LEGACY[h] ?? "rua";
};

// Aba na URL (#sensores...) para o voltar do navegador e o recarregar manterem o lugar
export function useNav() {
  const [tab, setTab] = useState<TabId>(readHash);
  const [target, setTarget] = useState<Code | null>(null);

  useEffect(() => {
    const onHash = () => setTab(readHash());
    addEventListener("hashchange", onHash);
    return () => removeEventListener("hashchange", onHash);
  }, []);

  // Depois de trocar de aba, leva o item pedido até a tela e o destaca por um instante
  useEffect(() => {
    if (!target) return;
    const el = document.getElementById(`c-${target}`);
    if (!el) return;
    el.scrollIntoView({ behavior: matchMedia("(prefers-reduced-motion: reduce)").matches ? "auto" : "smooth", block: "center" });
    el.classList.remove("flash");
    void el.offsetWidth;
    el.classList.add("flash");
    setTarget(null);
  }, [tab, target]);

  const go = useCallback((next: TabId, code?: Code) => {
    if (next !== readHash()) location.hash = next;
    setTab(next);
    if (code) setTarget(code);
    else scrollTo({ top: 0 });
  }, []);

  return { tab, go };
}
