# Ising Machine — Prototype Specification

Revision 4 · 2026-10-08

## 1. Objective

Build a minimal, read-only web interface at **isingmachine.app** for the ESP32-controlled 8-spin oscillator Ising machine.

- The ESP32 owns the hardware and publishes telemetry over an outbound WebSocket.
- A Cloudflare Worker and Durable Object relay that telemetry to browsers.
- The browser displays machine state. It sends no control commands in v1.
- There is exactly one device. It is either the real ESP32 or a fake device that sends dummy values. Both use the same endpoint and protocol.
- Dev and prod run the same code. Only the host differs.

The firmware and the web app can be developed independently, with the protocol (§4) as the contract between them. Hardware and firmware details live in [`firmware/README.md`](firmware/README.md).

---

## 2. Architecture

```
PCB ──SPI── ESP32  ─┐
                    ├─► /api/device ──► Worker ─► DeviceSession (DO) ─► /api/ws ──► Browser
tools/fake-device ──┘     (one at a time; a new connection replaces the old)
```

- **Prod host:** `isingmachine.app`, served from the Cloudflare edge.
- **Dev host:** `<mac-lan-ip>:5173`. `npm run dev` runs the SPA, Worker, and DO in a single process.
- The device always opens the connection itself and never exposes an inbound server.

---

## 3. Repository Layout

```
ising-machine-app/
├── firmware/              # ESP32 sketch, see firmware/README.md
├── hardware/              # schematics (PDF)
├── protocol/
│   └── messages.ts        # TS message types, source of truth for §4
├── src/                   # React SPA
├── worker/
│   ├── index.ts           # routing, auth
│   └── device-session.ts  # Durable Object
├── tools/
│   └── fake-device.ts
├── index.html
├── vite.config.ts
├── wrangler.jsonc
├── package.json
├── .dev.vars              # DEVICE_TOKEN for dev (gitignored)
└── SPEC.MD
```

It is one npm package with no monorepo tooling. `protocol/` is imported by `src/`, `worker/`, and `tools/`.

---

## 4. Protocol v1

The protocol uses JSON text frames over WebSocket, and every message has a `type` field. Receivers ignore unknown fields and unknown message types. Renaming or removing a field, or changing a field's meaning, bumps `protocol`.

### 4.1 Device → server

**`hello`** is sent on every connect:

```json
{ "type": "hello", "protocol": 1, "firmware": "0.1.0", "bootId": "7f3a", "fake": false }
```

`bootId` is random per boot and lets the browser detect resets. `fake` marks the fake device.

**`config`** is sent on connect and whenever the couplings change:

```json
{
  "type": "config",
  "configId": 3,
  "preset": "ring8",
  "couplers": [
    { "i": 0, "j": 1, "code": 128 },
    { "i": 0, "j": 2, "code": null }
  ]
}
```

- The array has 28 entries, one for each pair i<j of the 8 spins.
- `code` runs from 0 to 256. `null` means the coupler is disconnected.
- `preset` is `"off"`, `"ring8"`, or `null`.

**`state`** is sent at the telemetry rate: 10 Hz while someone is viewing, 1 Hz otherwise.

```json
{
  "type": "state",
  "seq": 1042,
  "uptimeMs": 843200,
  "configId": 3,
  "raw": 73,
  "spins": [1, -1, 1, -1, 1, -1, 1, -1],
  "agreement": [100, 97, 100, 88, 100, 94, 100, 91],
  "oscillating": true
}
```

- `seq` increases monotonically within a boot, which lets the browser detect drops.
- `spins` are gauge-fixed so that `spins[0] = +1`.
- `agreement` is the percentage of snapshots that agree with each spin.
- The browser derives cut weight and energy from `config` and `spins`.

### 4.2 Server → browser

- `presence` is sent when a browser connects and whenever the device connects or disconnects:
  ```json
  { "type": "presence", "online": true, "lastSeenMs": 1791460000000 }
  ```
- The device's `hello`, `config`, and `state` messages are forwarded verbatim.

### 4.3 Server → device

```json
{ "type": "viewers", "count": 2 }
```

This is a telemetry-rate hint and the only message the server sends to the device in v1.

---

## 5. Worker, Durable Object, and Environments

### 5.1 Routes

| Route | Client | Notes |
|---|---|---|
| `GET /api/state` | browser / curl | JSON `{presence, hello, config, state}` |
| `GET /api/ws` | browser | WebSocket, read-only. Inbound messages are ignored. |
| `GET /api/device` | ESP32 or fake device | WebSocket. Requires `Authorization: Bearer <DEVICE_TOKEN>`. |
| `GET /api/health` | curl | `{"ok":true}` |
| everything else | browser | static SPA assets |

### 5.2 `DeviceSession`

There is a single DO instance, `idFromName("device")`.

- Uses the WebSocket Hibernation API, with sockets tagged `device` or `browser`.
- Allows at most one device socket. A new device connection closes the old one, which also handles ESP32 reconnects and switching between the real and fake device.
- Caches the latest `hello`, `config`, and `state` and broadcasts each to browsers.
- Persists `hello`, `config`, and `lastSeen` to storage on change. `state` is kept in memory only.
- Marks the device offline when its socket closes, or after 5 s without a message (via an alarm).
- Sends `viewers` to the device whenever the browser count changes.

**Cost:** at 10 Hz the DO never hibernates. The `viewers` hint drops the rate to 1 Hz when nobody is watching. Check usage against the plan limits before leaving the device online 24/7.

### 5.3 Config

```jsonc
{
  "name": "isingmachine",
  "main": "worker/index.ts",
  "compatibility_date": "2026-10-01",
  "assets": { "not_found_handling": "single-page-application", "run_worker_first": ["/api/*"] },
  "observability": { "enabled": true },
  "routes": [{ "pattern": "isingmachine.app", "custom_domain": true }],
  "durable_objects": { "bindings": [{ "name": "DEVICE_SESSION", "class_name": "DeviceSession" }] },
  "migrations": [{ "tag": "v1", "new_sqlite_classes": ["DeviceSession"] }]
}
```

- The DO entries are added together with `DeviceSession`.
- There is no D1, KV, or R2.

### 5.4 Environments and workflows

There is one codebase and one `wrangler.jsonc`, with no environment-specific build.

| | Dev | Prod |
|---|---|---|
| Start | `npm run dev` | `npm run deploy` (`vite build && wrangler deploy`) |
| Runs on | Mac: Vite, plus `@cloudflare/vite-plugin` running the Worker and DO in workerd | Cloudflare edge |
| Browser | `http://localhost:5173` | `https://isingmachine.app` |
| Device endpoint | `ws://<mac-lan-ip>:5173/api/device` | `wss://isingmachine.app/api/device` |
| `DEVICE_TOKEN` | `.dev.vars` | `wrangler secret put DEVICE_TOKEN` |
| DO storage | `.wrangler/state/` | Cloudflare |

Each component picks its environment as follows:

- **SPA:** builds every URL from `window.location`, using `ws:` under `http:` and `wss:` under `https:`. It has no build-time environment variables.
- **Worker:** the code is identical in both environments, and only the secret differs.
- **ESP32:** `ENV_LOCAL` in `secrets.h` selects the host (see `firmware/README.md`). Switching means reflashing.
- **Fake device:** `npm run fake-device` targets dev, and `npm run fake-device:prod` targets prod.

Workflows:

| Task | Terminal 1 | Terminal 2 |
|---|---|---|
| Web development | `npm run dev` | `npm run fake-device` |
| Hardware against dev | `npm run dev` | flash ESP32 with `ENV_LOCAL 1` + serial monitor |
| Production | `npm run deploy` | ESP32 with `ENV_LOCAL 0`, or `npm run fake-device:prod` |

Notes:

- Hosting: isingmachine.app is registered with Cloudflare Registrar in the same account as the Worker. The `custom_domain` route manages the DNS record and certificate, so there are no hand-maintained DNS records.
- The ESP32 can reach the dev server because `vite.config.ts` sets `server.host = "0.0.0.0"`. Allow incoming connections when the macOS firewall prompts.

---

## 6. Frontend

The stack is Vite + React + TypeScript with no UI framework and no router.

`useDeviceState()` owns the WebSocket. It handles connecting, reconnecting with backoff, parsing messages, and tracking stats. Components never open sockets themselves.

```ts
const { presence, hello, config, state, stats } = useDeviceState();
// stats: { connected, rateHz, dropped, resets }
```

The v1 page shows:

- **Header:**
  - online/offline status with last seen
  - firmware version, with a "fake" badge when `hello.fake` is set
  - seq, rate, and dropped count
- **Spins:** 8 cells showing ±1, each shaded by agreement, plus the raw byte in binary.
- **Couplings:** an 8×8 upper-triangular grid of codes, with "off" for disconnected couplers.
- **Derived:** cut weight, plus the expected pattern when `preset` is `ring8`.

---

## 7. Fake Device

`tools/fake-device.ts` runs under Node and connects to the same `/api/device` endpoint with the same token. It sends dummy data:

- `hello` with `fake: true`
- the `ring8` `config`
- `state` at the rate set by `viewers`, with random spins and agreement

The fake device does no simulation. Its only job is to exercise the data path and the GUI.

---

## 8. Authentication

- **Device:** the Worker checks a shared bearer token on `/api/device`. In dev the token is `dev`, and in prod it is a Wrangler secret.
- **Browser:** viewing is public and read-only, with no login.

---

## 9. Non-Goals for v1

- Browser → machine control
- Multiple devices
- Accounts, history, databases
- OTA updates, binary serialization, waveform streaming

Control can later be added as new message types on the existing device socket.

---

## 10. Milestones

Firmware milestones are tracked in `firmware/README.md`.

1. ~~Scaffold Vite + React + Worker; deploy to isingmachine.app~~ (done 2026-10-08)
2. `protocol/messages.ts`, `DeviceSession`, and routes
3. Fake device → dev → browser, end to end
4. Fake device → prod → browser
5. ESP32 → dev → browser (requires the firmware net task)
6. ESP32 → prod → browser

---

## 11. Guiding Principle

```
SPI / registers / pin mux     (firmware drivers, board-revision specific)
        ↓
spins, couplings, agreement   (IsingMachine, firmware)
        ↓
Protocol v1                   (protocol/messages.ts)
        ↓
Worker / DO / browser         (never sees registers)
```
