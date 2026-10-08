import { DurableObject } from "cloudflare:workers";
import {
  PROTOCOL_VERSION,
  type Config,
  type DeviceMessage,
  type Hello,
  type Presence,
  type Snapshot,
  type State,
  type Viewers,
} from "../protocol/messages";

const STALE_MS = 5000;

// Single instance relaying the one device to all browsers (SPEC.MD §5.2).
export class DeviceSession extends DurableObject<Env> {
  private hello: Hello | null = null;
  private config: Config | null = null;
  private state: State | null = null;
  private lastSeenMs: number | null = null;

  constructor(ctx: DurableObjectState, env: Env) {
    super(ctx, env);
    ctx.blockConcurrencyWhile(async () => {
      this.hello = (await ctx.storage.get<Hello>("hello")) ?? null;
      this.config = (await ctx.storage.get<Config>("config")) ?? null;
      this.lastSeenMs = (await ctx.storage.get<number>("lastSeenMs")) ?? null;
    });
  }

  async fetch(request: Request): Promise<Response> {
    const { pathname } = new URL(request.url);

    if (pathname === "/api/state") {
      return Response.json(this.snapshot());
    }

    if (request.headers.get("Upgrade") !== "websocket") {
      return new Response("Expected WebSocket", { status: 426 });
    }

    const { 0: client, 1: server } = new WebSocketPair();

    if (pathname === "/api/device") {
      // One device at a time: a new connection replaces the old one.
      for (const old of this.ctx.getWebSockets("device")) {
        old.close(1000, "replaced");
      }
      this.ctx.acceptWebSocket(server, ["device"]);
      this.state = null;
      this.lastSeenMs = Date.now();
      await this.ctx.storage.setAlarm(Date.now() + STALE_MS);
      this.broadcast(this.presence());
      server.send(JSON.stringify(this.viewers()));
    } else {
      this.ctx.acceptWebSocket(server, ["browser"]);
      for (const msg of [this.presence(), this.hello, this.config, this.state]) {
        if (msg) server.send(JSON.stringify(msg));
      }
      this.sendViewers();
    }

    return new Response(null, { status: 101, webSocket: client });
  }

  async webSocketMessage(ws: WebSocket, data: string | ArrayBuffer) {
    if (!this.ctx.getTags(ws).includes("device") || typeof data !== "string") return;

    let msg: DeviceMessage;
    try {
      msg = JSON.parse(data);
    } catch {
      return;
    }
    this.lastSeenMs = Date.now();

    switch (msg.type) {
      case "hello":
        if (msg.protocol !== PROTOCOL_VERSION) {
          ws.close(1008, `unsupported protocol ${msg.protocol}`);
          return;
        }
        this.hello = msg;
        await this.ctx.storage.put("hello", msg);
        break;
      case "config":
        this.config = msg;
        await this.ctx.storage.put("config", msg);
        break;
      case "state":
        this.state = msg;
        break;
      default:
        return;
    }
    this.broadcast(data);
  }

  async webSocketClose(ws: WebSocket) {
    if (this.ctx.getTags(ws).includes("device")) {
      if (this.devices(ws).length === 0) await this.markOffline();
    } else {
      this.sendViewers(ws);
    }
  }

  async webSocketError(ws: WebSocket) {
    await this.webSocketClose(ws);
  }

  // Catches devices that vanish without closing their socket.
  async alarm() {
    const devices = this.devices();
    if (devices.length === 0) return;
    if (Date.now() - (this.lastSeenMs ?? 0) > STALE_MS) {
      for (const ws of devices) ws.close(1001, "stale");
      await this.markOffline();
    } else {
      await this.ctx.storage.setAlarm(Date.now() + STALE_MS);
    }
  }

  private async markOffline() {
    this.state = null;
    await this.ctx.storage.put("lastSeenMs", this.lastSeenMs);
    this.broadcast(this.presence());
  }

  private devices(excluding?: WebSocket) {
    return this.ctx.getWebSockets("device").filter((ws) => ws !== excluding);
  }

  private presence(excluding?: WebSocket): Presence {
    return { type: "presence", online: this.devices(excluding).length > 0, lastSeenMs: this.lastSeenMs };
  }

  private viewers(excluding?: WebSocket): Viewers {
    const count = this.ctx.getWebSockets("browser").filter((ws) => ws !== excluding).length;
    return { type: "viewers", count };
  }

  private sendViewers(excluding?: WebSocket) {
    const msg = JSON.stringify(this.viewers(excluding));
    for (const ws of this.devices()) ws.send(msg);
  }

  private broadcast(msg: string | object) {
    const data = typeof msg === "string" ? msg : JSON.stringify(msg);
    for (const ws of this.ctx.getWebSockets("browser")) {
      try {
        ws.send(data);
      } catch {
        // Socket already closing.
      }
    }
  }

  private snapshot(): Snapshot {
    return { presence: this.presence(), hello: this.hello, config: this.config, state: this.state };
  }
}
