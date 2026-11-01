import type { Published } from "../../protocol/messages";

interface Props {
  connected: boolean;
  result: Published | null;
}

export function StatusBar({ connected, result }: Props) {
  let detail: string;
  if (!connected) detail = "Server unreachable";
  else if (result) detail = `Last result ${new Date(result.receivedMs).toLocaleString()}`;
  else detail = "No results yet";

  return (
    <div className="status">
      <span className={`dot ${connected ? "online" : "offline"}`} aria-hidden />
      <span className="muted">{detail}</span>
    </div>
  );
}
