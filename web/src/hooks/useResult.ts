import { useEffect, useState } from "react";
import type { BrowserMessage, Published } from "../../protocol/messages";

export interface ResultState {
  /** Browser WebSocket to the server is open. */
  connected: boolean;
  /** Latest published max-cut result. */
  result: Published | null;
}

/** Owns the browser WebSocket to /api/ws. */
export function useResult(): ResultState {
  const [connected, setConnected] = useState(false);
  const [result, setResult] = useState<Published | null>(null);

  useEffect(() => {
    let ws: WebSocket | null = null;
    let retryTimer: ReturnType<typeof setTimeout> | undefined;
    let backoffMs = 500;
    let closed = false;

    const connect = () => {
      const proto = location.protocol === "https:" ? "wss:" : "ws:";
      ws = new WebSocket(`${proto}//${location.host}/api/ws`);

      ws.onopen = () => {
        backoffMs = 500;
        setConnected(true);
      };

      ws.onmessage = (event) => {
        const msg: BrowserMessage = JSON.parse(event.data);
        if (msg.type === "result") setResult(msg);
      };

      ws.onclose = () => {
        setConnected(false);
        if (closed) return;
        retryTimer = setTimeout(connect, backoffMs);
        backoffMs = Math.min(backoffMs * 2, 10_000);
      };
    };

    connect();

    return () => {
      closed = true;
      clearTimeout(retryTimer);
      ws?.close();
    };
  }, []);

  return { connected, result };
}
