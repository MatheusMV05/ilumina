import { useEffect, useState, type ReactNode } from "react";
import type { Icon } from "@phosphor-icons/react";
import {
  CircleHalfIcon, HardDrivesIcon, LightbulbFilamentIcon, LightningIcon, MoonIcon, StorefrontIcon, SunIcon,
  WifiHighIcon, WifiSlashIcon, WrenchIcon,
} from "@phosphor-icons/react";

import { DEMO } from "../demo";
import { EMPTY, fmtDur } from "../format";
import type { Issue, TabId } from "../nav";
import { HEALTH, MODE_TEXT, SENSORS, SENSOR_COLOR, SENSOR_ORDER } from "../sensors";
import type { ByCode, Code, DashboardState } from "../types";
import type { LinkStatus } from "../useTelemetry";
import { Orb } from "./StatusOrb";

// ---------- Tema claro / escuro / do sistema ----------

type ThemePref = "auto" | "light" | "dark";
const THEME_KEY = "tema";
const THEME_NEXT: Record<ThemePref, ThemePref> = { auto: "light", light: "dark", dark: "auto" };
const THEME_ICON: Record<ThemePref, Icon> = { auto: CircleHalfIcon, light: SunIcon, dark: MoonIcon };
const THEME_LABEL: Record<ThemePref, string> = { auto: "Tema do sistema", light: "Tema claro", dark: "Tema escuro" };

function readTheme(): ThemePref {
  try {
    const v = localStorage.getItem(THEME_KEY);
    return v === "light" || v === "dark" ? v : "auto";
  } catch {
    return "auto";
  }
}

function useTheme() {
  const [pref, setPref] = useState<ThemePref>(readTheme);
  useEffect(() => {
    const root = document.documentElement;
    if (pref === "auto") delete root.dataset.theme;
    else root.dataset.theme = pref;
    try { localStorage.setItem(THEME_KEY, pref); } catch { /* sem armazenamento: vale só nesta visita */ }
  }, [pref]);
  return { pref, cycle: () => setPref((p) => THEME_NEXT[p]) };
}

// ---------- Barra de título ----------

export function Titlebar({ state, link }: { state: DashboardState | null; link: LinkStatus }) {
  const { pref, cycle } = useTheme();
  const ThemeIcon = THEME_ICON[pref];
  const L = state?.lighting;
  const live = link.kind === "live";

  return (
    <header className="titlebar">
      <div className="titlebar-start">
        <span className="logo" aria-hidden="true"><LightbulbFilamentIcon weight="fill" /></span>
        <span className="titlebar-name">Ilumina</span>
      </div>
      <div className="titlebar-center" aria-live="polite">
        <div className="pill-field">
          <Orb spec={live ? HEALTH.OK : link.kind === "connecting" ? HEALTH.NOT_TESTED : { tone: "critical", icon: WifiSlashIcon, text: "" }} size="sm" />
          <span className="pill-field-text">
            <strong>{state?.device ?? "Procurando o poste"}</strong>
            {L && <span>{MODE_TEXT[L.mode]}, luz em <span className="num">{L.zones[0]}%</span></span>}
            {!live && link.kind === "stale" && <span>sem resposta há {fmtDur(link.ageMs)}</span>}
          </span>
        </div>
      </div>
      <div className="titlebar-end">
        {DEMO && <span className="demo-flag">Simulação</span>}
        <button type="button" className="icon-btn" onClick={cycle} aria-label={`${THEME_LABEL[pref]}. Trocar tema`} title={THEME_LABEL[pref]}>
          <ThemeIcon weight="bold" aria-hidden="true" />
        </button>
      </div>
    </header>
  );
}

// ---------- Barra lateral ----------

export const MODULES: { id: TabId; label: string; icon: Icon; color: string }[] = [
  { id: "rua", label: "Rua", icon: StorefrontIcon, color: "#16adc2" },
  { id: "energia", label: "Energia", icon: LightningIcon, color: "#e9a10f" },
  { id: "manutencao", label: "Manutenção", icon: WrenchIcon, color: "#e8781e" },
];

interface SidebarProps {
  tab: TabId;
  go: (t: TabId, c?: Code) => void;
  issues: Issue[];
  byCode: ByCode | null;
}

// No computador é a coluna de vidro à esquerda; no celular vira a barra de abas embaixo
export function Sidebar({ tab, go, issues, byCode }: SidebarProps) {
  const installed = byCode ? SENSOR_ORDER.filter((code) => byCode[code].health !== "NOT_AVAILABLE") : [];
  return (
    <nav className="sidebar" aria-label="Seções do painel">
      <span className="section-label">Painel</span>
      <div className="nav-list">
        {MODULES.map(({ id, label, icon: ModIcon, color }) => {
          const count = issues.filter((i) => i.tab === id).length;
          const critical = issues.some((i) => i.tab === id && i.critical);
          return (
            <a
              key={id}
              href={`#${id}`}
              className="nav-item"
              style={{ "--nav-color": color } as React.CSSProperties}
              aria-current={tab === id ? "page" : undefined}
              onClick={(e) => { e.preventDefault(); go(id); }}
            >
              <span className="chip" aria-hidden="true"><ModIcon weight="fill" /></span>
              <span className="nav-label">{label}</span>
              {count > 0 && (
                <span className={`nav-count${critical ? " is-critical" : ""}`}>
                  {count}<span className="sr-only"> pendência{count === 1 ? "" : "s"}</span>
                </span>
              )}
            </a>
          );
        })}
      </div>

      {installed.length > 0 && byCode && (
        <div className="nav-sensors">
          <span className="section-label">Sensores instalados</span>
          {installed.map((code) => (
            <button
              key={code} type="button" className="nav-mini"
              style={{ "--nav-color": SENSOR_COLOR[code] } as React.CSSProperties}
              onClick={() => go("manutencao", code)}
            >
              <span className="nav-dot" aria-hidden="true" />
              <span className="nav-label">{SENSORS[code].title}</span>
              <Orb spec={HEALTH[byCode[code].health] ?? HEALTH.NOT_TESTED} size="sm" />
              <span className="sr-only">{(HEALTH[byCode[code].health] ?? HEALTH.NOT_TESTED).text}</span>
            </button>
          ))}
        </div>
      )}
    </nav>
  );
}

// ---------- Título da tela, posto sobre o céu ----------

export function ScreenHeader({ title, subtitle, actions }: { title: string; subtitle: ReactNode; actions?: ReactNode }) {
  return (
    <div className="screen-header">
      <div>
        <h1 className="screen-title">{title}</h1>
        <p className="screen-subtitle">{subtitle}</p>
      </div>
      {actions && <div className="screen-actions">{actions}</div>}
    </div>
  );
}

// ---------- Barra de estado flutuante ----------

export function StatusBar({ state, link }: { state: DashboardState | null; link: LinkStatus }) {
  const n = state?.network;
  const net = !n ? EMPTY : !n.wifi_configured ? "Modo local, sem roteador" : n.wifi ? (n.mqtt ? "Wi-Fi e MQTT conectados" : "Wi-Fi conectado") : "Wi-Fi do roteador offline";
  return (
    <footer className="statusbar">
      <span className="status-item"><HardDrivesIcon weight="bold" aria-hidden="true" /> Servido pelo ESP32, sem internet</span>
      <span className="status-sep" aria-hidden="true" />
      <span className="status-item"><WifiHighIcon weight="bold" aria-hidden="true" /> Rede Iluminacao-IoT</span>
      <span className="status-spacer" />
      <span className="status-item">{n?.wifi_configured && !n.wifi ? <WifiSlashIcon weight="bold" aria-hidden="true" /> : <WifiHighIcon weight="bold" aria-hidden="true" />} {net}</span>
      <span className="status-sep" aria-hidden="true" />
      <span className="status-item num">
        {link.kind === "live" ? "Atualizado agora" : link.kind === "connecting" ? "Conectando" : `Sem resposta há ${fmtDur(link.ageMs)}`}
      </span>
      {state && (
        <>
          <span className="status-sep" aria-hidden="true" />
          <span className="status-item num">Ligado há {fmtDur(state.uptime_ms)}</span>
        </>
      )}
    </footer>
  );
}
