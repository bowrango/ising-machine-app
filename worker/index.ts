export { DeviceSession } from "./device-session";

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    switch (url.pathname) {
      case "/api/health":
        return Response.json({ ok: true });

      case "/api/device":
        // Fail closed if the secret isn't configured.
        if (!env.DEVICE_TOKEN || request.headers.get("Authorization") !== `Bearer ${env.DEVICE_TOKEN}`) {
          return new Response("Unauthorized", { status: 401 });
        }
      // fallthrough
      case "/api/ws":
      case "/api/state":
        return env.DEVICE_SESSION.get(env.DEVICE_SESSION.idFromName("device")).fetch(request);
    }

    return new Response("Not found", { status: 404 });
  },
} satisfies ExportedHandler<Env>;
