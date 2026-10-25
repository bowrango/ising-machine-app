# ising-machine-app

An 8-spin oscillator Ising machine streamed live to [isingmachine.app](https://isingmachine.app). Firmware and boards: [`firmware/README.md`](firmware/README.md).

```
ESP32 ──wss /api/device──▶ Worker ──▶ DeviceSession (Durable Object) ──wss /api/ws──▶ browsers
```

The ESP32 connects out to a Cloudflare Worker over a WebSocket. A single Durable Object relays its messages to every browser. The site is public and read-only.

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
| Device endpoint | `ws://<mac-lan-ip>:5173/api/device` | `wss://isingmachine.app/api/device` |
| ESP32 build | `ENV_LOCAL 1` in `secrets.h` | `ENV_LOCAL 0` in `secrets.h` |
| `DEVICE_TOKEN` | `web/.dev.vars` | `wrangler secret put DEVICE_TOKEN` |

Switching environments means reflashing the ESP32. In dev, the Mac and ESP32 must be on the same network.

## Device protocol

The device connects to `/api/device` with `Authorization: Bearer <DEVICE_TOKEN>` and sends JSON messages. The types are defined in [`web/protocol/messages.ts`](web/protocol/messages.ts).

| Direction | `type` | When |
|---|---|---|
| device → server | `hello` | first, on every connect |
| device → server | `config` | on connect and when the couplings change |
| device → server | `state` | 10 Hz while someone is viewing, 1 Hz otherwise |
| server → device | `viewers` | when the browser count changes |

The device must send something at least every 5 s, or it is marked offline. A new connection replaces the old one, so the device can just reconnect after a drop.

## Routes

| Route | Use |
|---|---|
| `/api/device` | ESP32 WebSocket |
| `/api/ws` | browser WebSocket |
| `/api/state` | JSON snapshot of the latest state |
| `/api/health` | health check |

## Layout

```
web/         Cloudflare site: React app (src/), Worker + Durable Object (worker/), message types (protocol/)
firmware/    ESP32 sketch
hardware/    schematics and bench scripts
```
