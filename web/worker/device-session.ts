import { DurableObject } from "cloudflare:workers";
import type { Published, Result, Snapshot } from "../protocol/messages";

// Single instance holding the latest result and relaying new ones to all browsers.
export class DeviceSession extends DurableObject<Env> {
  private result: Published | null = null;

  constructor(ctx: DurableObjectState, env: Env) {
    super(ctx, env);
    ctx.blockConcurrencyWhile(async () => {
      this.result = (await ctx.storage.get<Published>("result")) ?? null;
    });
  }

  async fetch(request: Request): Promise<Response> {
    const { pathname } = new URL(request.url);

    if (pathname === "/api/state") {
      const snapshot: Snapshot = { result: this.result };
      return Response.json(snapshot);
    }

    if (pathname === "/api/result") {
      let result: Result;
      try {
        result = await request.json();
      } catch {
        return new Response("Invalid JSON", { status: 400 });
      }
      if (result?.type !== "result" || typeof result.name !== "string") {
        return new Response("Expected a result", { status: 400 });
      }
      this.result = { ...result, receivedMs: Date.now() };
      await this.ctx.storage.put("result", this.result);
      this.broadcast(this.result);
      return new Response(null, { status: 204 });
    }

    if (request.headers.get("Upgrade") !== "websocket") {
      return new Response("Expected WebSocket", { status: 426 });
    }
    const { 0: client, 1: server } = new WebSocketPair();
    this.ctx.acceptWebSocket(server);
    if (this.result) server.send(JSON.stringify(this.result));
    return new Response(null, { status: 101, webSocket: client });
  }

  private broadcast(msg: Published) {
    const data = JSON.stringify(msg);
    for (const ws of this.ctx.getWebSockets()) {
      try {
        ws.send(data);
      } catch {
        // Socket already closing.
      }
    }
  }
}
