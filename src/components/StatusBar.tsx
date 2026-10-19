import type { Hello, Presence } from "../../protocol/messages";
import type { Stats } from "../hooks/useDeviceState";

interface Props {
  presence: Presence | null;
  hello: Hello | null;
  stats: Stats;
  seq: number | undefined;
}

export function StatusBar({ presence, hello, stats, seq }: Props) {
  const online = stats.connected && !!presence?.online;
  const source = online ? "Device online" : "No device";

  let detail: string;
  if (!stats.connected) detail = "Server unreachable";
  else if (online) detail = `fw ${hello?.firmware ?? "?"} · seq ${seq ?? "–"} · ${stats.rateHz.toFixed(1)} Hz · ${stats.dropped} dropped`;
  else if (presence?.lastSeenMs) detail = `Last seen ${new Date(presence.lastSeenMs).toLocaleString()}`;
  else detail = "Never connected";

  return (
    <div className="status">
      <span className={`dot ${online ? "online" : "offline"}`} aria-hidden />
      <strong>{source}</strong>
      <span className="muted">{detail}</span>
    </div>
  );
}
