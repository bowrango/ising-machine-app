# ising-machine-app

An 8-spin oscillator Ising machine whose max-cut results are published to [isingmachine.app](https://isingmachine.app). Firmware and boards: [`firmware/README.md`](firmware/README.md).

```
ESP32 ──serial──▶ Mac (hardware/io) ──POST /api/result──▶ Worker ──▶ DeviceSession (Durable Object) ──wss /api/ws──▶ browsers
```

The bench scripts solve a problem on the ESP32 over serial and POST the result to a Cloudflare Worker. A single Durable Object keeps the latest result and relays new ones to every browser. The site is public and read-only.

## Setup

```
cd web
npm install
npm run dev       # runs the site, Worker and Durable Object locally
npm run deploy    # builds and deploys to Cloudflare
```

| | Dev | Prod |
|---|---|---|
| Browser | `http://localhost:5173` | `https://isingmachine.app` |
| Publish endpoint | `http://localhost:5173/api/result` | `https://isingmachine.app/api/result` |
| `DEVICE_TOKEN` | `web/.dev.vars` | `wrangler secret put DEVICE_TOKEN` |

## Publishing results

`python3 hardware/io/run_maxcut.py <graph> --publish` POSTs each result to `/api/result` with `Authorization: Bearer $DEVICE_TOKEN`. The body is a `Result` from [`web/protocol/messages.ts`](web/protocol/messages.ts); browsers get it over `/api/ws` stamped with `receivedMs`.

## Routes

| Route | Use |
|---|---|
| `/api/result` | POST a result (bearer token) |
| `/api/ws` | browser WebSocket |
| `/api/state` | JSON snapshot of the latest result |
| `/api/health` | health check |

## Layout

```
web/         Cloudflare site: React app (src/), Worker + Durable Object (worker/), message types (protocol/)
firmware/    ESP32 sketch (serial CLI)
hardware/    schematics and bench scripts
```
