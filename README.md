# ising-machine-app

An 8-spin oscillator Ising machine (ESP32 + coupled 1 MHz oscillators) streamed live to [isingmachine.app](https://isingmachine.app). Full spec: [`SPEC.MD`](SPEC.MD). Firmware and hardware notes: [`firmware/README.md`](firmware/README.md).

```
ESP32 ──wss /api/device──▶ Worker ──▶ DeviceSession (Durable Object) ──wss /api/ws──▶ browsers
```

## Device interface

The firmware talks to a Cloudflare Worker over one WebSocket. A single Durable Object, `DeviceSession`, holds the device socket, caches the latest messages, and relays them to every browser.

**Connect:** `GET /api/device` with `Upgrade: websocket` and `Authorization: Bearer <DEVICE_TOKEN>`. A missing or wrong token gets `401`.

| | Dev | Prod |
|---|---|---|
| Server | `npm run dev` (Worker + DO run locally on the Mac) | `npm run deploy` (Cloudflare edge) |
| ESP32 build | `ENV_LOCAL 1` in `secrets.h` | `ENV_LOCAL 0` in `secrets.h` |
| Device endpoint | `ws://<mac-lan-ip>:5173/api/device` | `wss://isingmachine.app/api/device` |
| `DEVICE_TOKEN` | `.dev.vars` | `wrangler secret put DEVICE_TOKEN` |
| Browser | `http://localhost:5173` | `https://isingmachine.app` |

Switching environments means reflashing the ESP32. For dev, the Mac and ESP32 must be on the same network, and macOS must allow incoming connections to Vite.

**Messages** are JSON text frames with a `type` field. Types live in [`protocol/messages.ts`](protocol/messages.ts), and the firmware mirrors them by hand.

| Direction | `type` | When |
|---|---|---|
| device → server | `hello` | first, on every connect: `protocol`, `firmware`, `bootId` |
| device → server | `config` | on connect and whenever couplings change: 28 couplers, `code` 0–256 or `null` |
| device → server | `state` | 10 Hz while viewed, 1 Hz otherwise: `seq`, `spins` (`spins[0] = +1`), `agreement`, … |
| server → device | `viewers` | on connect and when the browser count changes; sets the telemetry rate |

**Rules the firmware must follow:**

- Send `hello` with `protocol: 1`. A mismatch closes the socket with `1008`.
- Send something at least every 5 s. Otherwise the server closes the socket with `1001 stale` and marks the device offline. 1 Hz `state` is enough.
- Only one device can be connected. A new connection closes the old one with `1000 replaced`, so reconnecting after a drop just works.
- Reconnect with a short delay (~2 s) on any close.
- The server ignores binary frames and unknown types. Browsers ignore unknown fields.

## Other routes

| Route | Use |
|---|---|
| `GET /api/ws` | browser WebSocket (read-only): `presence`, then the cached `hello` / `config` / `state` |
| `GET /api/state` | JSON snapshot `{presence, hello, config, state}` |
| `GET /api/health` | `{"ok":true}` |

## Layout

```
src/                 React SPA (useDeviceState owns the socket)
worker/              index.ts routes, device-session.ts Durable Object
protocol/            messages.ts, the shared message types
firmware/            ESP32 Arduino sketch
hardware/            schematics and bench notes
wrangler.jsonc       Worker + DO config
```
