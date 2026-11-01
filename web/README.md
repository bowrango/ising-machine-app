# isingmachine.app

A public read-only Cloudflare site showing the latest max-cut result.

```
device.py ──POST /api/result──▶ Worker ──▶ DeviceSession (Durable Object) ──wss /api/ws──▶ browsers
```

The Worker checks the bearer token and forwards each result to a single Durable Object, which stores the latest one and relays it to every connected browser.

## Setup

```
npm install
npm run dev       # site, Worker and Durable Object locally
npm run deploy    # build and deploy to Cloudflare
```

| | Dev | Prod |
|---|---|---|
| Browser | `http://localhost:5173` | `https://isingmachine.app` |
| Publish endpoint | `http://localhost:5173/api/result` | `https://isingmachine.app/api/result` |
| `DEVICE_TOKEN` | `.dev.vars` | `npx wrangler secret put DEVICE_TOKEN` |

`device.py` reads the same token from its `DEVICE_TOKEN` environment variable (e.g. exported in `~/.zshrc`).

## Routes

| Route | Use |
|---|---|
| `/api/result` | POST a `Result` (bearer token) |
| `/api/ws` | browser WebSocket: the latest result, then each new one |
| `/api/state` | JSON snapshot of the latest result |
| `/api/health` | health check |

Message types are in [`protocol/messages.ts`](protocol/messages.ts): `Result` is the POST body, `Published` is a result as browsers get it, stamped with `receivedMs`.

## Layout

```
src/         React app
worker/      Worker (routing, auth) and DeviceSession Durable Object
protocol/    message types
```
