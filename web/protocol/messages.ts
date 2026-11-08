// Protocol message types. hardware/io/device.py mirrors Result by hand.

export const SPIN_COUNT = 8;
export const SPIN_LABELS = ["A", "B", "C", "D", "E", "F", "G", "H"] as const;

export type Phase = "+" | "-";

/** A max-cut solve, POSTed to /api/result: the problem and the device's read-out vs the ground truth. */
export interface Result {
  type: "result";
  /** Graph preset, e.g. "ring8b" or "rand8w". */
  name: string;
  /** Problem weight per edge, e.g. { AB: 1, BC: 3 }. */
  problem: Record<string, number>;
  /** Read-out phase per spin and its cut value. */
  found: { state: Record<string, Phase>; cut: number };
  /** Max cut and an optimal state. */
  max: { state: Record<string, Phase>; cut: number };
  /** Each spin's correlation with the reference spin, -1 (antiphase) … 1 (in phase). */
  corr: Record<string, number>;
}

const isObject = (x: unknown): x is Record<string, unknown> => typeof x === "object" && x !== null && !Array.isArray(x);
const isSpin = (k: string) => (SPIN_LABELS as readonly string[]).includes(k);
const isEdge = (k: string) => k.length === 2 && isSpin(k[0]) && isSpin(k[1]) && k[0] !== k[1];
const isMap = (x: unknown, key: (k: string) => boolean, value: (v: unknown) => boolean) =>
  isObject(x) && Object.entries(x).every(([k, v]) => key(k) && value(v));
const isNumber = (v: unknown) => typeof v === "number" && Number.isFinite(v);
const isCut = (x: unknown) => isObject(x) && isNumber(x.cut) && isMap(x.state, isSpin, (v) => v === "+" || v === "-");

/** Runtime check that x has Result's shape; the server rejects anything else, so a publisher that
 * drifts from this file (device.py) gets a 400 instead of breaking the page. */
export function isResult(x: unknown): x is Result {
  return (
    isObject(x) &&
    x.type === "result" &&
    typeof x.name === "string" &&
    isMap(x.problem, isEdge, isNumber) &&
    isMap(x.corr, isSpin, (v) => isNumber(v) && Math.abs(v as number) <= 1) &&
    isCut(x.found) &&
    isCut(x.max)
  );
}

/** A Result as the server relays it, stamped with when it arrived. */
export interface Published extends Result {
  receivedMs: number;
}

/** Server → browser over /api/ws. */
export type BrowserMessage = Published;

/** JSON snapshot served by GET /api/state. */
export interface Snapshot {
  result: Published | null;
}
