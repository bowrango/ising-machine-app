// Protocol v1 message types (SPEC.MD §4). The firmware mirrors these by hand.

export const PROTOCOL_VERSION = 1;
export const SPIN_COUNT = 8;
export const SPIN_LABELS = ["A", "B", "C", "D", "E", "F", "G", "H"] as const;
export const CODE_MAX = 256;

// Device → server

export interface Hello {
  type: "hello";
  protocol: number;
  firmware: string;
  bootId: string;
  fake: boolean;
}

export interface Coupler {
  i: number;
  j: number;
  /** 0 (strongest) … 256 (weakest); null = disconnected. */
  code: number | null;
}

export interface Config {
  type: "config";
  configId: number;
  preset: "off" | "ring8" | null;
  /** 28 entries, pairs i<j in lexicographic order. */
  couplers: Coupler[];
}

export interface State {
  type: "state";
  seq: number;
  uptimeMs: number;
  configId: number;
  raw: number;
  /** Gauge-fixed: spins[0] = +1. */
  spins: (1 | -1)[];
  /** Percent of snapshots agreeing with each spin. */
  agreement: number[];
  oscillating: boolean;
}

export type DeviceMessage = Hello | Config | State;

// Server → browser (plus forwarded DeviceMessages)

export interface Presence {
  type: "presence";
  online: boolean;
  lastSeenMs: number | null;
}

export type BrowserMessage = DeviceMessage | Presence;

// Server → device

export interface Viewers {
  type: "viewers";
  count: number;
}

/** JSON snapshot served by GET /api/state. */
export interface Snapshot {
  presence: Presence;
  hello: Hello | null;
  config: Config | null;
  state: State | null;
}
