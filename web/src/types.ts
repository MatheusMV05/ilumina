// Formato de GET /api/state (src/Telemetry.cpp, buildDashboardJson)

export type Health = "OK" | "WARNING" | "FAILURE" | "WARMING_UP" | "NOT_TESTED" | "NOT_AVAILABLE" | "VISUAL_CHECK";
export type LinkState = "CONNECTED" | "UNKNOWN" | "SUSPECT" | "DISCONNECTED";
export type Mode = "DAY_OFF" | "ECONOMY" | "ACTIVE" | "SAFE";
export type Summary = "NORMAL" | "ALERTS" | "SAFE_MODE";

export interface Link {
  state: LinkState;
  reason: string;
}

export interface TestState {
  available: boolean;
  confirmable: boolean;
  state: "IDLE" | "RUNNING" | "PASSED" | "FAILED";
  window_ms: number;
  remaining_ms: number;
  baseline?: number;
  best: number;
  target: number;
  finished_ago_ms: number | null;
}

export interface LdrData { value: number | null; raw: number; noise: number }
export interface UltrasonicData {
  distance_cm: number | null;
  last: "VALID" | "NO_ECHO" | "NO_RESPONSE" | "BUSY" | string;
  echo_pct: number;
  response_pct: number;
  in_range: boolean;
}
export interface IrData { detected: boolean; changes: number; last_change_ago_ms: number | null; active_low: boolean }
export interface DhtData {
  temperature_c: number | null;
  humidity_percent: number | null;
  heat_index_c: number | null;
  success_pct: number;
  age_ms: number | null;
}
export interface Mq135Data { adc: number | null; raw: number; warming: boolean; warmup_left_ms: number; level: "low" | "medium" | "high" | null }
export interface MicData {
  rms: number | null;
  dbfs: number | null;
  peak_dbfs: number | null;
  floor_dbfs: number | null;
  bias: number;
  do_events: number;
  do_active: boolean;
}
export interface LedsData { zones: number[]; lamp1_optical_delta: number | null; lamp1_optical_min: number }

export interface DataByCode {
  LDR_AMBIENT: LdrData;
  LDR_LOCAL: LdrData;
  ULTRASONIC: UltrasonicData;
  IR: IrData;
  DHT11: DhtData;
  MQ135: Mq135Data;
  MIC: MicData;
  OLED: Record<string, never>;
  LEDS: LedsData;
  STATUS_LED: Record<string, never>;
  WIFI: Record<string, never>;
  MQTT: Record<string, never>;
}
export type Code = keyof DataByCode;
export type SensorCode = "LDR_AMBIENT" | "ULTRASONIC" | "IR" | "LDR_LOCAL" | "DHT11" | "MQ135" | "MIC";
export type SystemCode = "OLED" | "LEDS" | "STATUS_LED" | "WIFI" | "MQTT";

export interface ComponentState<C extends Code = Code> {
  code: C;
  name: string;
  health: Health;
  label: string;
  note: string;
  alert: boolean;
  // Componentes sem detecção física (OLED, LEDs, rede) vêm com link nulo
  link: Link | null;
  data: DataByCode[C];
  test: TestState;
}
export type ByCode = { [C in Code]: ComponentState<C> };

export interface LightingState {
  mode: Mode;
  is_night: boolean | null;
  target_percent: number;
  zones: number[];
  presence_hc: boolean;
  presence_ir: boolean;
  presence_available: boolean;
  hc_usable: boolean;
  ir_usable: boolean;
  hold_remaining_ms: number;
  reason: string;
}

export interface LightingConfig {
  day_threshold: number;
  night_threshold: number;
  standby_percent: number;
  active_percent: number;
  hold_ms: number;
  min_distance_cm: number;
  max_distance_cm: number;
  mq135_medium: number;
  mq135_high: number;
}

export interface DashboardState {
  device: string;
  uptime_ms: number;
  summary: Summary;
  network: { wifi_configured: boolean; wifi: boolean; mqtt_configured: boolean; mqtt: boolean };
  lighting: LightingState;
  config: LightingConfig;
  components: ComponentState[];
}

// Uma amostra por atualização, para os gráficos e a tabela
export interface HistoryPoint {
  t: number;
  ambient: number | null;
  period: string;
  presence: boolean;
  light: number;
  mode: Mode;
}

export interface SparkPoint {
  t: number;
  v: number | null;
}
