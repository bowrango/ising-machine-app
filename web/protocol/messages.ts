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
