import { useEffect, useRef, useState } from "react";
import type { BrowserMessage, Config, Hello, Presence, State } from "../../protocol/messages";

export interface Stats {
  connected: boolean;
  rateHz: number;
  dropped: number;
  resets: number;
}

export interface DeviceState {
  presence: Presence | null;
  hello: Hello | null;
  config: Config | null;
  state: State | null;
  stats: Stats;
}

const RATE_WINDOW_MS = 2000;

/** Owns the browser WebSocket to /api/ws. */
export function useDeviceState(): DeviceState {
  const [presence, setPresence] = useState<Presence | null>(null);
  const [hello, setHello] = useState<Hello | null>(null);
  const [config, setConfig] = useState<Config | null>(null);
  const [state, setState] = useState<State | null>(null);
  const [stats, setStats] = useState<Stats>({ connected: false, rateHz: 0, dropped: 0, resets: 0 });

  const track = useRef({ lastSeq: -1, bootId: "", arrivals: [] as number[], dropped: 0, resets: 0 });

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
        setStats((s) => ({ ...s, connected: true }));
      };

      ws.onmessage = (event) => {
        const msg: BrowserMessage = JSON.parse(event.data);
        const t = track.current;
        switch (msg.type) {
          case "presence":
            setPresence(msg);
            if (!msg.online) setState(null);
            break;
          case "hello":
            if (t.bootId && msg.bootId !== t.bootId) t.resets++;
            t.bootId = msg.bootId;
            t.lastSeq = -1;
            setHello(msg);
            break;
          case "config":
            setConfig(msg);
            break;
          case "state": {
            if (t.lastSeq >= 0 && msg.seq > t.lastSeq + 1) t.dropped += msg.seq - t.lastSeq - 1;
            t.lastSeq = msg.seq;
            const now = performance.now();
            t.arrivals = [...t.arrivals.filter((a) => now - a < RATE_WINDOW_MS), now];
            setState(msg);
            break;
          }
        }
      };

      ws.onclose = () => {
        setStats((s) => ({ ...s, connected: false }));
        if (closed) return;
        retryTimer = setTimeout(connect, backoffMs);
        backoffMs = Math.min(backoffMs * 2, 10_000);
      };
    };

    connect();

    // Refresh derived stats at a steady pace rather than on every message.
    const statsTimer = setInterval(() => {
      const t = track.current;
      const now = performance.now();
      t.arrivals = t.arrivals.filter((a) => now - a < RATE_WINDOW_MS);
      setStats((s) => ({ ...s, rateHz: (t.arrivals.length * 1000) / RATE_WINDOW_MS, dropped: t.dropped, resets: t.resets }));
    }, 500);

    return () => {
      closed = true;
      clearTimeout(retryTimer);
      clearInterval(statsTimer);
      ws?.close();
    };
  }, []);

  return { presence, hello, config, state, stats };
}
