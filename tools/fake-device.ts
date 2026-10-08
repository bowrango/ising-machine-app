// Fake device: sends dummy telemetry to /api/device (SPEC.MD §7).
//   npm run fake-device                         → ws://localhost:5173
//   npm run fake-device -- --url <ws(s)://…>   → anywhere
//   DEVICE_TOKEN=… npm run fake-device:prod     → wss://isingmachine.app

import type { Config, Coupler, DeviceMessage, Hello, State, Viewers } from "../protocol/messages.ts";

const urlArg = process.argv.indexOf("--url");
const url = urlArg > 0 ? process.argv[urlArg + 1] : "ws://localhost:5173/api/device";
const token = process.env.DEVICE_TOKEN ?? "dev";

const RING8 = new Set(["0,1", "1,2", "2,3", "3,4", "4,5", "5,6", "6,7", "0,7"]);
const couplers: Coupler[] = [];
for (let i = 0; i < 8; i++) {
  for (let j = i + 1; j < 8; j++) couplers.push({ i, j, code: RING8.has(`${i},${j}`) ? 128 : null });
}

const bootId = Math.random().toString(16).slice(2, 6);
const bootTime = Date.now();
let seq = 0;

function connect() {
  console.log(`[WS] connecting ${url}`);
  const ws = new WebSocket(url, { headers: { Authorization: `Bearer ${token}` } } as never);
  const send = (msg: DeviceMessage) => ws.send(JSON.stringify(msg));
  let timer: ReturnType<typeof setInterval> | undefined;

  const schedule = (hz: number) => {
    clearInterval(timer);
    timer = setInterval(() => {
      const spins = Array.from({ length: 8 }, (_, k) => (k === 0 || Math.random() < 0.5 ? 1 : -1)) as State["spins"];
      const raw = spins.reduce((acc, s, k) => acc | ((s === 1 ? 1 : 0) << k), 0);
      send({
        type: "state",
        seq: seq++,
        uptimeMs: Date.now() - bootTime,
        configId: 1,
        raw,
        spins,
        agreement: spins.map(() => 80 + Math.floor(Math.random() * 21)),
        oscillating: true,
      });
    }, 1000 / hz);
  };

  ws.onopen = () => {
    console.log("[WS] connected");
    send({ type: "hello", protocol: 1, firmware: "fake-0.1.0", bootId, fake: true } satisfies Hello);
    send({ type: "config", configId: 1, preset: "ring8", couplers } satisfies Config);
    schedule(1);
  };

  ws.onmessage = (event) => {
    const msg = JSON.parse(String(event.data)) as Viewers;
    if (msg.type === "viewers") {
      console.log(`[WS] viewers=${msg.count}`);
      schedule(msg.count > 0 ? 10 : 1);
    }
  };

  ws.onclose = (event) => {
    clearInterval(timer);
    console.log(`[WS] disconnected (${event.code}${event.reason ? ` ${event.reason}` : ""}); retrying in 2 s`);
    setTimeout(connect, 2000);
  };

  ws.onerror = () => {}; // onclose follows and handles the retry
}

connect();
