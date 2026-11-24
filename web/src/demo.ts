// Simulação (somente com ?demo na URL, para ver o painel sem o ESP32)
import type { Code, ComponentState, DashboardState, Health, LampControl, LampState, Link, StreetHour, StreetStats, TestState } from "./types";

export const DEMO = new URLSearchParams(location.search).has("demo");

const demo = {
  t0: Date.now(),
  hold: 0,
  night: undefined as boolean | undefined,
  tests: {} as Partial<Record<Code, { start: number }>>,
  confirmed: {} as Partial<Record<Code, number>>,
  controls: ["AUTO", "AUTO", "AUTO", "AUTO"] as LampControl[],
  manual: [100, 100, 100, 100],
  // Na simulação o LED 3 está fora do circuito, para mostrar a pendência
  checkedAt: Date.now(),
};

export const demoSetLamp = (zone: number, control: LampControl, percent?: number) => {
  demo.controls[zone] = control;
  if (percent !== undefined) demo.manual[zone] = percent;
};
export const demoCheckLamps = () => { demo.checkedAt = Date.now() + 3000; };

export const demoStartTest = (code: Code) => { demo.tests[code] = { start: Date.now() }; };
export const demoConfirm = (code: Code) => { demo.confirmed[code] = Date.now(); };

export function demoState(): DashboardState {
  const now = Date.now();
  const s = (now - demo.t0) / 1000;
  const ambient = Math.round(2000 + 900 * Math.sin(s / 25));
  demo.night = ambient > 2200 ? false : ambient < 1800 ? true : (demo.night ?? ambient < 2000);
  const dist = Math.floor(s / 7) % 3 === 0 ? 40 + 10 * Math.sin(s) : null;
  const presenceHc = dist !== null;
  if (presenceHc) demo.hold = now;
  const holdLeft = Math.max(0, 10000 - (now - demo.hold));
  const mode = !demo.night ? "DAY_OFF" : presenceHc || holdLeft > 0 ? "ACTIVE" : "ECONOMY";
  const dimmed = ambient >= 1800 ? 20 : ambient <= 900 ? 60 : Math.round(20 + ((1800 - ambient) / 900) * 40);
  const pct = { DAY_OFF: 0, ECONOMY: dimmed, ACTIVE: 100 }[mode];
  const zones = demo.controls.map((c, z) => (c === "OFF" ? 0 : c === "ON" ? demo.manual[z] : pct));
  const checking = now < demo.checkedAt;
  const lamps: LampState[] = zones.map((z, i) => ({
    control: demo.controls[i], manual_percent: demo.manual[i], percent: z,
    status: i === 2 ? "MISSING" : "OK", delta: i === 2 ? 2 : 60 + 8 * i,
  }));

  const test = (code: Code, windowMs: number, best: number, target: number): TestState => {
    const t = demo.tests[code];
    const base: TestState = { available: true, confirmable: false, window_ms: windowMs, baseline: 0, best: 0, target,
      remaining_ms: 0, finished_ago_ms: null, state: "IDLE" };
    if (!t) return base;
    const elapsed = now - t.start;
    if (elapsed < 4000) return { ...base, state: "RUNNING", remaining_ms: windowMs - elapsed, best: (best * elapsed) / 4000 };
    return { ...base, state: "PASSED", best, finished_ago_ms: elapsed - 4000 };
  };
  const health = (code: Code, okHealth: Health): Health => {
    const t = demo.tests[code];
    return t && now - t.start > 4000 ? "OK" : okHealth;
  };
  const comp = <C extends Code>(code: C, h: Health, note: string, link: Link | null,
    data: ComponentState<C>["data"], t?: TestState): ComponentState<C> => ({
    code, name: code, health: h, label: h, note, alert: ["WARNING", "FAILURE", "NOT_TESTED"].includes(h), link, data,
    test: t ?? { available: false, confirmable: code === "LEDS" || code === "STATUS_LED", state: "IDLE",
      window_ms: 0, remaining_ms: 0, best: 0, target: 0, finished_ago_ms: null },
  });
  const ok: Link = { state: "CONNECTED", reason: "none" };
  const absent: Link = { state: "UNKNOWN", reason: "none" };

  return {
    device: "poste-01 (demo)",
    uptime_ms: now - demo.t0 + 60000,
    summary: "ALERTS",
    network: { wifi_configured: false, wifi: false, mqtt_configured: false, mqtt: false },
    lighting: {
      lamps, lamp_check_ago_ms: checking ? null : now - demo.checkedAt, lamp_check_no_signal: false,
      mode, is_night: demo.night, target_percent: pct, zones, presence_hc: presenceHc, presence_ir: false,
      presence_available: true, hc_usable: true, ir_usable: false, hold_remaining_ms: mode === "ACTIVE" ? holdLeft : 0, reason: "",
    },
    config: {
      day_threshold: 2200, night_threshold: 1800, dark_threshold: 900, dusk_percent: 20, night_percent: 60, active_percent: 100, hold_ms: 10000,
      min_distance_cm: 5, max_distance_cm: 120, mq135_medium: 1500, mq135_high: 2500,
    },
    components: [
      comp("OLED", "OK", "endereco 0x3C", null, {}),
      comp("DHT11", health("DHT11", "OK"), "respondendo (100% das leituras)", ok,
        { temperature_c: 25.4, humidity_percent: 61, heat_index_c: 25.9, success_pct: 100, age_ms: 800 }, test("DHT11", 45000, 2.4, 2)),
      comp("LDR_AMBIENT", health("LDR_AMBIENT", "OK"), "conectado, sinal estavel", ok,
        { value: ambient, raw: ambient, noise: 3.2 }, test("LDR_AMBIENT", 15000, 620, 500)),
      comp("LDR_LOCAL", health("LDR_LOCAL", "OK"), "conectado, sinal estavel", ok,
        // O LDR encostado no LED 1 acompanha a intensidade, como na montagem real
        { value: Math.round(700 + pct * 26 + 20 * Math.random()), raw: 0, noise: 2.1 }, test("LDR_LOCAL", 15000, 900, 725)),
      comp("ULTRASONIC", health("ULTRASONIC", "OK"), "modulo responde", ok,
        { distance_cm: dist, last: dist ? "VALID" : "NO_ECHO", echo_pct: 34, response_pct: 100, in_range: presenceHc },
        test("ULTRASONIC", 15000, 18, 60)),
      comp("IR", "NOT_AVAILABLE", "nao instalado nesta montagem", absent,
        { detected: false, changes: 0, last_change_ago_ms: null, active_low: true }),
      comp("MIC", health("MIC", "OK"), "ouvindo", ok,
        { rms: 3.1, dbfs: -62 + 6 * Math.random(), peak_dbfs: -55, floor_dbfs: -64, bias: 1840, do_events: 3, do_active: false },
        test("MIC", 15000, 14, 10)),
      comp("MQ135", "NOT_AVAILABLE", "nao instalado nesta montagem", absent,
        { adc: null, raw: 0, warming: false, warmup_left_ms: 0, level: null }),
      comp("LEDS", "WARNING", "LED sem resposta: 3", null,
        { zones, lamp1_optical_delta: 640, lamp1_optical_min: 150 }),
      comp("STATUS_LED", demo.confirmed.STATUS_LED ? "OK" : "VISUAL_CHECK", "", null, {}),
      comp("WIFI", "NOT_AVAILABLE", "sem credenciais", null, {}),
      comp("MQTT", "NOT_AVAILABLE", "desabilitado", null, {}),
    ] as ComponentState[],
  };
}

// Um dia típico de rua residencial: picos de manhã e no fim da tarde, madrugada calma
export function demoStreet(): StreetStats {
  const hour = new Date().getHours();
  const busy = (h: number) => Math.max(0.05, Math.exp(-((h - 7.5) ** 2) / 3) + 1.3 * Math.exp(-((h - 18) ** 2) / 4) + 0.4 * Math.exp(-((h - 12.5) ** 2) / 2));
  const hours: StreetHour[] = Array.from({ length: 24 }, (_, h) => {
    const s = h === hour ? 1800 : h > hour && h < hour + 3 ? 0 : 3600 * 3;
    if (!s) return { s: 0, p: 0, m: 0, n: null, l: null, pk: null, t: null, u: null };
    const b = busy(h);
    const hours = s / 3600;
    return {
      s, p: Math.round(b * 38 * hours), m: Math.round(b * 240 * hours),
      n: +(2 + b * 7).toFixed(1), l: +(Math.min(0.6, b * 0.18)).toFixed(3), pk: +(12 + b * 16).toFixed(1),
      t: +(19 + 7 * Math.sin(((h - 9) / 24) * 2 * Math.PI)).toFixed(1), u: Math.round(72 - 18 * Math.sin(((h - 9) / 24) * 2 * Math.PI)),
    };
  });
  return {
    clock_valid: true, epoch: Math.floor(Date.now() / 1000), tz_min: -new Date().getTimezoneOffset(), hour,
    since: Math.floor(Date.now() / 1000) - 3 * 86400, loud_db: 12, hours,
  };
}
