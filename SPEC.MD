Ising Machine Web Interface — Rough Specification

1. Objective

Build a minimal web interface for an ESP32-controlled Ising machine PCB.

The initial system is read-only from the web application’s perspective:

* ESP32 reads measurements/state from the PCB.
* ESP32 publishes telemetry over Wi-Fi.
* Cloudflare receives and relays telemetry.
* Web GUI displays current machine state and measurements.
* The web GUI does not send control commands to the ESP32 in the initial implementation.

The architecture should support both:

1. Local development
2. Production deployment at isingmachine.app

without requiring major code changes between environments.

⸻

2. High-Level Architecture

Production

PCB
 │
 │ SPI
 ▼
ESP32
 │
 │ WebSocket / HTTPS over Wi-Fi
 ▼
Cloudflare Worker
 │
 │ WebSocket
 ▼
React Web GUI

Domain:

https://isingmachine.app

The ESP32 initiates all network connections. The ESP32 should not expose an inbound HTTP server to the public Internet.

⸻

Local Development

PCB
 │
 │ SPI
 ▼
ESP32
 │
 │ local Wi-Fi
 ▼
Local Cloudflare Worker / Vite dev server
 │
 ▼
Browser

Example:

ESP32
  ↓
ws://192.168.1.100:5173/api/device
  ↓
Mac / development machine

The ESP32 and development computer should normally be on the same LAN.

Production and local environments should use the same telemetry protocol.

⸻

3. Repository Structure

Initial repository:

isingmachine/
├── firmware/
│   ├── isingmachine.ino
│   ├── tasks/
│   ├── drivers/
│   ├── network/
│   └── config/
│
├── src/
│   ├── components/
│   ├── hooks/
│   ├── pages/
│   └── protocol/
│
├── worker/
│   ├── index.ts
│   └── device-session.ts
│
├── tools/
│   └── fake-device.ts
│
├── public/
│
├── vite.config.ts
├── wrangler.jsonc
├── package.json
├── .dev.vars
├── .gitignore
└── SPEC.md

This should remain a single simple repository initially.

Avoid adding monorepo frameworks, multiple Node packages, containers, or additional infrastructure unless they become necessary.

⸻

4. ESP32 Firmware

Technology

Firmware will use:

Arduino .ino
FreeRTOS
SPI
Wi-Fi
WebSocket client

The Arduino framework may expose FreeRTOS functionality directly through the ESP32 environment.

⸻

5. Firmware Responsibilities

The ESP32 is responsible for:

1. Configuring PCB peripherals.
2. Communicating with PCB components over SPI.
3. Sampling machine state.
4. Running periodic FreeRTOS tasks.
5. Formatting telemetry.
6. Maintaining a Wi-Fi connection.
7. Maintaining an outbound WebSocket connection.
8. Sending telemetry to the server.

The ESP32 should not depend on the web GUI being connected.

PCB control and measurement should continue operating even if networking is unavailable.

⸻

6. Firmware Architecture

Suggested organization:

isingmachine.ino
      │
      ├── FreeRTOS tasks
      │
      ├── machine abstraction
      │
      ├── SPI drivers
      │
      └── network layer

Suggested modules:

firmware/
├── isingmachine.ino
│
├── drivers/
│   ├── spi_bus.*
│   ├── potentiometer.*
│   └── phase_comparator.*
│
├── tasks/
│   ├── measurement_task.*
│   └── telemetry_task.*
│
├── network/
│   ├── wifi.*
│   └── websocket.*
│
└── config/
    └── config.h

Exact file organization may remain flexible while the firmware is still under active development.

⸻

7. FreeRTOS Tasks

Initial task separation should be simple.

Measurement Task

Responsible for interacting with the PCB.

Example:

MeasurementTask
    ↓
read SPI / GPIO
    ↓
update current machine state

Possible rate:

100–1000 Hz

depending on what data ultimately needs to be exposed.

The measurement rate and web telemetry rate do not need to be identical.

⸻

Telemetry Task

Responsible for sending the latest state to the web backend.

Example:

TelemetryTask
    ↓
read latest machine state
    ↓
serialize JSON
    ↓
send WebSocket message

Initial telemetry rate:

5–20 Hz

This should be sufficient for a responsive GUI without generating unnecessary network traffic.

⸻

8. Shared Firmware State

The PCB measurement task and networking task should communicate through a shared state structure.

Example:

struct MachineState {
    uint32_t sequence;
    uint32_t uptime_ms;
    int8_t spins[8];
    float phases[8];
    uint8_t comparator_bits;
    bool running;
};

The exact fields will evolve.

Access should be protected appropriately using a FreeRTOS synchronization mechanism such as:

mutex
critical section
queue

A queue is preferred if telemetry should consume discrete samples.

A shared structure protected by a mutex is sufficient if the GUI only needs the latest state.

⸻

9. Machine Abstraction

Network code should not contain SPI-level details.

For example:

MachineState readMachineState();

or:

class IsingMachine {
public:
    void update();
    MachineState getState();
};

The networking layer should consume MachineState, not MCP4351 register values or raw SPI transactions.

Low-level diagnostic data may later be exposed separately.

⸻

10. Telemetry Protocol

The initial protocol is read-only.

Use JSON over WebSocket.

Initial protocol version:

1

⸻

Device Hello

When the ESP32 connects:

{
  "type": "hello",
  "protocol": 1,
  "deviceId": "im-0001",
  "firmware": "0.1.0",
  "capabilities": {
    "oscillators": 8
  }
}

⸻

State Message

Example:

{
  "type": "state",
  "seq": 1042,
  "uptimeMs": 843200,
  "spins": [1, -1, 1, 1, -1, -1, 1, -1],
  "phases": [
    0.02,
    3.12,
    0.04,
    0.01,
    3.10,
    3.14,
    0.03,
    3.11
  ],
  "comparatorBits": 73,
  "running": true
}

Not every field needs to be populated immediately.

The protocol should favor meaningful machine quantities rather than raw hardware implementation details.

⸻

11. Sequence Numbers

Every state message should contain a monotonically increasing sequence number:

seq

Example:

{
  "type": "state",
  "seq": 582
}

This allows the frontend to detect:

* dropped samples
* out-of-order messages
* ESP32 resets

A reboot may reset seq to zero.

⸻

12. Web Backend

The backend runs as a Cloudflare Worker.

Its initial responsibilities are deliberately limited:

1. Accept ESP32 WebSocket connections.
2. Accept browser WebSocket connections.
3. Track the latest state for each connected device.
4. Broadcast ESP32 telemetry to connected browsers.

It should not initially implement:

* control commands
* experiment scheduling
* accounts
* databases
* run history
* device OTA
* complex authentication

⸻

13. Durable Object

Use one Durable Object per physical device.

Example:

device ID: im-0001
        ↓
DeviceSession("im-0001")

The Durable Object maintains:

ESP32 WebSocket
browser WebSocket(s)
latest telemetry message
connection status

Conceptually:

ESP32 ──────────► DeviceSession ◄──────── Browser
                       │
                       └── latest state

⸻

14. Initial Worker Routes

Keep the Worker API small.

Suggested endpoints:

GET /api/status
GET /api/devices/:deviceId/state
GET /api/devices/:deviceId/ws

Possible device endpoint:

GET /api/device/:deviceId/ws

The exact route naming is not critical as long as browser and ESP32 endpoints remain conceptually distinct.

⸻

15. Cloudflare Vite Setup

Use:

Vite
React
TypeScript
@cloudflare/vite-plugin
Wrangler

Keep configuration minimal.

Example vite.config.ts:

import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { cloudflare } from "@cloudflare/vite-plugin";
export default defineConfig({
  plugins: [
    react(),
    cloudflare(),
  ],
  server: {
    host: "0.0.0.0",
  },
});

host: "0.0.0.0" allows the ESP32 to connect to the development server over the local network.

Avoid adding proxies or multiple development servers unless required.

⸻

16. Wrangler Configuration

Initial wrangler.jsonc should contain only what is needed for the Worker and Durable Object.

Example:

{
  "name": "isingmachine",
  "main": "worker/index.ts",
  "compatibility_date": "2026-10-01",
  "durable_objects": {
    "bindings": [
      {
        "name": "DEVICE_SESSION",
        "class_name": "DeviceSession"
      }
    ]
  },
  "migrations": [
    {
      "tag": "v1",
      "new_sqlite_classes": [
        "DeviceSession"
      ]
    }
  ]
}

Do not introduce D1 initially.

⸻

17. React Frontend

Initial GUI requirements are minimal.

The page should display:

Device: im-0001
Connection:
● Connected
Firmware:
0.1.0
Sequence:
1042
Spins:
+1 -1 +1 +1 -1 -1 +1 -1
Comparator:
01001001
Uptime:
843.2 s

A basic layout is sufficient.

The initial objective is validating the full data path:

PCB
→ ESP32
→ WebSocket
→ Cloudflare
→ Browser

Visual polish should come later.

⸻

18. Frontend State

Create one simple React hook:

useDeviceState(deviceId)

Responsibilities:

open browser WebSocket
receive telemetry
parse protocol messages
track connection status
return latest MachineState

Example API:

const {
  connected,
  state,
  firmware,
} = useDeviceState("im-0001");

Components should not directly manage WebSockets.

⸻

19. Fake Device

A fake ESP32 implementation should be maintained under:

tools/fake-device.ts

This allows frontend development without the PCB.

Example behavior:

connect
send hello
send state every 100 ms
randomly modify spins/phases

Development:

npm run dev
npm run fake-device

This environment should mimic the same protocol used by the actual ESP32.

⸻

20. Local Development Modes

Two local workflows should be supported.

Web-Only Development

Fake Device
     ↓
Local Worker
     ↓
React GUI

Commands:

npm run dev
npm run fake-device

Use this for:

* frontend work
* protocol work
* Worker development

⸻

Hardware Development

PCB
 ↓
ESP32
 ↓
Local Worker
 ↓
React GUI

Typical workflow:

Terminal 1:
npm run dev
Terminal 2:
Arduino Serial Monitor / build-upload workflow

The ESP32 should connect to the development machine’s LAN address.

Example:

ws://192.168.1.100:5173/api/device/im-0001/ws

⸻

21. Firmware Environment Configuration

The ESP32 should have separate development and production configuration.

Conceptually:

#ifdef LOCAL_DEV
#define SERVER_HOST "192.168.1.100"
#define SERVER_PORT 5173
#define SERVER_TLS false
#else
#define SERVER_HOST "isingmachine.app"
#define SERVER_PORT 443
#define SERVER_TLS true
#endif

The exact implementation may use Arduino build flags or configuration headers.

Do not maintain separate firmware codebases.

Only environment configuration should differ.

⸻

22. Production Connection

Production ESP32 connection:

wss://isingmachine.app/api/device/im-0001/ws

Local connection:

ws://192.168.1.100:5173/api/device/im-0001/ws

The telemetry messages themselves should be identical.

⸻

23. Device Identity

For the initial board, use a fixed identifier:

im-0001

Store it in firmware configuration.

Later this can be generated during provisioning.

The backend should never assume there will only be one device, even though only one exists initially.

⸻

24. Authentication

Authentication can initially remain lightweight.

For local development:

no authentication

or a development-only shared token.

For production, the ESP32 should eventually send a device secret.

Possible header:

Authorization: Bearer <device-secret>

Authentication implementation is not required for the first end-to-end prototype.

The API should nevertheless be structured so authentication can be added without changing the telemetry protocol.

⸻

25. Error Handling

The ESP32 should tolerate network failure.

Expected behavior:

Wi-Fi unavailable
    ↓
PCB continues operating
    ↓
retry Wi-Fi periodically
WebSocket disconnected
    ↓
PCB continues operating
    ↓
reconnect with backoff

Networking failures must never block the measurement task indefinitely.

⸻

26. Timing Requirements

Initial target rates:

PCB sampling:
hardware dependent
MachineState update:
~100+ Hz if useful
Web telemetry:
10 Hz
GUI refresh:
up to telemetry rate

These rates should remain configurable.

The web interface does not need every raw hardware sample.

⸻

27. Logging

ESP32 serial output should provide basic diagnostics:

[BOOT] firmware 0.1.0
[WIFI] connected 192.168.1.42
[WS] connecting
[WS] connected
[SPI] initialized
[STATE] seq=124
[WS] disconnected
[WS] reconnecting

The firmware should remain debuggable without the web application.

⸻

28. Non-Goals for Version 1

Version 1 explicitly does not need:

* Web-to-ESP32 commands
* Coupling configuration from the browser
* Run/stop controls
* Cloud database
* Saved experiments
* User accounts
* Multi-user permissions
* OTA firmware updates
* Historical plots
* High-rate waveform streaming
* Binary serialization
* MQTT
* Kafka
* Kubernetes
* Docker-based local infrastructure

These may be added later.

⸻

29. Initial Milestone

Version 1 is complete when the following works reliably:

PCB state
    ↓
ESP32 reads state
    ↓
ESP32 publishes JSON telemetry
    ↓
local Worker receives telemetry
    ↓
browser displays telemetry

and the same firmware can switch configuration and produce:

PCB
 ↓
ESP32
 ↓
isingmachine.app
 ↓
browser

without changing the telemetry protocol.

⸻

30. Implementation Order

Recommended implementation sequence:

1. Stabilize PCB SPI readout
2. Define MachineState
3. Create FreeRTOS measurement task
4. Print MachineState over serial
5. Create minimal Vite + React + Cloudflare Worker project
6. Implement fake-device.ts
7. Display fake-device telemetry in React
8. Add ESP32 Wi-Fi
9. Add ESP32 WebSocket client
10. Connect ESP32 to local development server
11. Validate PCB → browser telemetry
12. Deploy Worker + frontend to isingmachine.app
13. Connect ESP32 to production

Only after this pipeline is stable should bidirectional control and persistent experiment data be added.

⸻

31. Guiding Principle

The main software boundary is:

PCB / SPI implementation
        ↓
MachineState
        ↓
Telemetry Protocol
        ↓
Web Application

The web application should understand the Ising machine state, not the details of individual SPI transactions.

This keeps the frontend and cloud architecture independent of future PCB revisions.