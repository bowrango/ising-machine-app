# Ising Machine Firmware

This folder holds the ESP32 Feather V2 firmware. The web side and the telemetry protocol are described in [`../SPEC.MD`](../SPEC.MD).

## Bench setup

- Cable: the Anker USB-C cable, plugged into the second port on the left.
- VS Code must not be holding the serial port.
- Arduino IDE → Tools → Upload Speed = 460800.
- Serial Monitor = 115200 baud, line ending "Newline" (or any setting except "No line ending").
- Open `firmware/isingmachine/isingmachine.ino` in the IDE.

Building with arduino-cli (bundled with the IDE) from the repo root:

```
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$CLI" compile --fqbn esp32:esp32:adafruit_feather_esp32_v2 firmware/isingmachine
"$CLI" upload  --fqbn esp32:esp32:adafruit_feather_esp32_v2 --board-options UploadSpeed=460800 \
  -p /dev/cu.usbserial-5AA60810241 firmware/isingmachine
```

The serial port must be free, so close the IDE's Serial Monitor before uploading.

At boot the firmware prints a test menu. Type a command and press Enter:

| Command | Test |
|---|---|
| `all` | `map` + `digipots` + `sr`; fails only on a FAIL, not a SKIP |
| `digipots` | every code 0–256 on all 28 channels, all pots written before reading back (7196 checks, ~0.34 s) |
| `sr` | 16 shift-register snapshots with relative spins. PASS if the bits change; SKIP if they're static (oscillator board off) |
| `map` | checks the coupling table covers each pair exactly once |
| `decoder` | holds each decoder address 5 s for probing U13 |

---

## 1. Hardware

The tables in this section are derived from `../hardware/oscillator-schematic.PDF`, `../hardware/coupler-schematic.PDF`, and `isingmachine.ino`. They still need to be checked on the bench.

### 1.1 Signal chain

| Block | Parts | Function |
|---|---|---|
| Oscillators | 2× OSC_BLOCK_X4 (B1: A–D, B2: E–H), SN74HC240 + LC tank (750 pF) | 8 differential 1 MHz oscillators, outputs `OSC_x_L` / `OSC_x_R` |
| Injection | SYNC via BNC (J8) + filter | 2 MHz SYNC locks each oscillator's phase to 0 or π |
| Enable | DPDT switch SW1 → `OE_B1`, `OE_B2` | Manual only. The firmware can't read or control it. |
| Comparators | 2× WIN_DETECT_X4, TLV3502 | `WIN_x = (OSC_x_R > OSC_x_L)` |
| Readout | SN74HCS165 (U15) | Snapshots all 8 `WIN_x` at once and shifts them out on SPI |
| Couplers | 7× MCP4351-103 (U16–U22), 10 kΩ, 4 ch each | 28 couplers, one per pair, giving all-to-all coupling for 8 spins |
| CS decode | SN74HCS138 (U13), enabled by `CS1` | Address 0–6 → `DP_CS0..6`. Address 7 → `CLK_INH`, which also switches the MISO mux to the shift register. |
| MISO/MOSI mux | TMUX1072 (U14) + SW4 | Selects the MISO source (digipot or shift register). SW4 selects MOSI between GPIO13 and GPIO32. |

### 1.2 Pin map

| Signal | GPIO | Notes |
|---|---|---|
| SCK1 | 14 | |
| MISO1 | 12 | Strapping pin (MTDI) |
| MOSI1 | 13 | Shared with the onboard LED |
| MOSI1_BK | 32 | Currently used as MOSI (SW4 position) |
| CS1 | 21 | Decoder enable |
| ADDR0/1/2 | 15 / 33 / 27 | Verified with `testDecoderSweep` |
| SH_LD | 19 | 74HCS165 parallel load (active low) |

### 1.3 Spin readout

- Shift-register bit *i* is `WIN` of oscillator *i*, with A = bit 0 and H = bit 7.
- A snapshot captures every oscillator's sign at the same instant. A single bit toggles at 1 MHz, so on its own it carries no information. **The bit relative to oscillator A is what carries meaning**, because once the oscillators are locked, each one is either in phase or anti-phase with A.
- Spin decoding: `s_i = +1 if bit_i == bit_A else −1`, which fixes `s_A = +1`.
- **Take N snapshots per measurement, with jittered timing.**
  - A snapshot takes about 10 µs, which is close to a whole number of oscillator periods. With fixed spacing, every snapshot would land at the same point in the cycle.
  - Before each `SH_LD` pulse, insert a random 0–1 µs busy-wait on the CPU cycle counter. `delayMicroseconds` is too coarse for this.
- **Agreement** is the percentage of snapshots that match the majority value. Snapshots taken near a zero crossing lower it. Agreement that stays low means the spins aren't locked.
- **Oscillating heuristic:** if the raw byte never changes during a measurement, the oscillators are off (SW1) or have no supply.

### 1.4 Coupling map (from netlist; verify on the bench)

- Each MCP4351 channel is a rheostat between W and B, with A not connected.
- Code 0 gives about R_W, the strongest coupling. Code 256 gives about 10 kΩ, the weakest. A TCON disconnect gives J = 0.

**Every coupler joins an `_L` node to an `_R` node**, so every coupling has the same sign, which is antiferromagnetic under the comparator convention above. Only the magnitude is programmable, which makes the board a weighted Max-Cut machine.

| Digipot (addr) | Ch0 | Ch1 | Ch2 | Ch3 |
|---|---|---|---|---|
| U16 (0) | A–B | A–C | A–D | A–E |
| U17 (1) | A–F | A–G | A–H | B–C |
| U18 (2) | B–D | B–E | B–F | B–G |
| U19 (3) | B–H | C–D | C–E | C–F |
| U20 (4) | C–G | C–H | D–E | D–F |
| U21 (5) | D–G | D–H | E–F | E–G |
| U22 (6) | E–H | F–G | F–H | G–H |

- Index `k = addr*4 + ch` enumerates the pairs (i<j) in lexicographic order.
- This table belongs in `coupling_map.h`.
- The protocol carries pairs `(i, j)` and never exposes `(addr, ch)`.

### 1.5 Known issues

- **GPIO12 strapping (found 2026-10-08):** with the Feather seated on the board, MISO is driven high during reset (`boot:0x33` vs `0x13` off-board).
  - The ESP32 then selects 1.8 V flash, which shows up as `wrong chip id 0xffff` at boot and `MD5 of file does not match` on upload.
  - The likely cause is floating `CS1`/`ADDR` pins during reset, which enable the decoder and put a digipot SDO or the shift register's QH onto MISO.
  - **Fix for this Feather:** `espefuse set_flash_voltage 3.3V` forces 3.3 V flash and makes the chip ignore GPIO12 at reset. This is irreversible, and correct for the PICO-MINI-02.
  - TCON writes were scrambled the same way.
  - Read responses are `0xFE00 | value`, with bit 9 = CMDERR = 1 when valid.
  - All 7 pots × 4 channels now verify across 0–256, including full scale.
- Make sure a shift-register read can never overlap a digipot transaction (`CLK_INH` / MISO mux timing).

---

## 2. Firmware Design

### 2.1 Toolchain

- Uses arduino-esp32 core 3.x, which runs on FreeRTOS. `setup()` and `loop()` run in `loopTask` on core 1.
- Builds with arduino-cli; the IDE serial monitor is fine.
- Libraries: `links2004/WebSockets` (WS/WSS client) and `ArduinoJson` v7.

### 2.2 Layout

The Arduino build only compiles the sketch folder and its `src/`, so the code lives under `firmware/isingmachine/`. Entries marked *planned* don't exist yet.

```
firmware/
├── README.md
└── isingmachine/
    ├── isingmachine.ino          # setup(): bus init; loop(): serial test menu
    ├── config.h                  # planned: rates, firmware version
    ├── secrets.example.h         # planned: secrets.h is gitignored
    └── src/
        ├── board.h               # pin map, SPI clock, counts, decoder addresses
        ├── drivers/
        │   ├── bus.*             # HSPI + decoder CS + MISO mux; the only code that touches SPI pins
        │   ├── mcp4351.*         # register read/write, TCON, setChannels(); no printing
        │   └── hcs165.*          # readSnapshot()
        ├── machine/
        │   └── coupling_map.h    # (pot, ch) <-> spin pair table; planned: IsingMachine, spin decode
        ├── tests/                # decoderSweep, digipotReadback, shiftRegister, couplingMap
        ├── tasks/                # planned: machine_task, net_task, console_task
        └── net/                  # planned: wifi, ws_client, telemetry JSON
```

Rules:

- Drivers return values and never print. Only the tests and the console print.
- Each driver's `begin()` configures the pins it owns, and nothing outside `drivers/` touches SPI, decoder, or `SH_LD` pins.
- Board facts (pins, reference designators, counts) live in `board.h`. Chip drivers stay generic.
- An out-of-range pot, channel, or code is a programming error and trips an `assert`.

### 2.3 Tasks

| Task | Core | Priority | Cadence | Owns |
|---|---|---|---|---|
| `machine` | 1 | high | measurement period (~10 ms) | SPI bus, decoder GPIO, `SH_LD`. **Only this task touches SPI.** |
| `net` | 0 | medium | events + telemetry period | Wi-Fi, WebSocket, JSON |
| `console` | 1 | low | blocks on Serial | Nothing; it posts commands to `machine` |

The tasks communicate through queues rather than shared globals:

- `stateMailbox`: a length-1 queue. `machine` writes it with `xQueueOverwrite`, and `net` reads it with `xQueuePeek`.
- `configMailbox`: works the same way and holds the coupler settings.
- `cmdQueue`: carries commands from `console` to `machine`.

The machine task never blocks on networking, and the net task never touches SPI.

### 2.4 Machine abstraction

```cpp
struct MachineConfig {
    uint32_t configId;            // increments on every change
    int16_t  code[28];            // -1 = disconnected, 0..256
    bool     verified;            // readback matched
};

struct MachineState {
    uint32_t seq;
    uint32_t uptimeMs;
    uint32_t configId;
    uint8_t  raw;                 // last snapshot
    int8_t   spins[8];            // spins[0] = +1
    uint8_t  agreement[8];        // 0..100 %
    bool     oscillating;
};

class IsingMachine {
public:
    bool  begin();
    bool  applyConfig(const MachineConfig&);   // write + readback verify
    MachineState measure();                    // N snapshots → decoded state
};
```

### 2.5 Serial console

```
help
couple <i> <j> <code|off>
preset <off|ring8>
dump                          config + latest state (+ PASS/FAIL for ring8)
test decoder                  testDecoderSweep()
test digipots                 testWriteReadBack()
rate <measure_ms> <telemetry_ms>
```

There are two presets:

- **`off`**: all couplers disconnected. This is the boot default.
- **`ring8`**: the cycle A–B–C–D–E–F–G–H–A at one code (starting at 128, tuned on the bench), with every other coupler off.
  - The cycle has a unique maximum cut up to a global flip, so the expected result is `+−+−+−+−`.
  - A result of all `+1` instead would mean the coupling sign in §1.4 is wrong.
  - The ring uses U16 ch0, U17 ch2–3, U19 ch1, U20 ch2, U21 ch2, and U22 ch1 and ch3.

### 2.6 Network configuration

`secrets.h` is gitignored, and `secrets.example.h` is committed.

```cpp
#define ENV_LOCAL 1                 // 1 = dev server on the Mac, 0 = isingmachine.app
#define LOCAL_HOST "192.168.1.100"  // Mac LAN IP (use a DHCP reservation)
#define WIFI_SSID "..."
#define WIFI_PASS "..."
#define DEVICE_TOKEN "..."          // "dev" for local
```

| `ENV_LOCAL` | URL |
|---|---|
| 1 | `ws://LOCAL_HOST:5173/api/device` |
| 0 | `wss://isingmachine.app/api/device` (TLS validated with the ESP32 CA bundle) |

On every connect, the net task sends `hello` and then `config`. After that it sends `state` at 10 Hz while `viewers > 0` and at 1 Hz otherwise.

### 2.7 Error handling and logging

- Measurement continues through any network failure.
- Wi-Fi and the WebSocket reconnect with backoff (1 s → 30 s, with jitter).
- The machine task is subscribed to the task watchdog.

Serial logging uses tagged lines:

```
[BOOT] fw 0.1.0 boot=7f3a
[SPI] digipots verified 28/28
[WIFI] connected 192.168.1.42
[WS] connected
[STATE] seq=124 spins=+-+-+-+- agr=min 92%
```

### 2.8 Rates

| Quantity | Target |
|---|---|
| Snapshot | ~10 µs + 0–1 µs jitter |
| Snapshots per measurement | 16–64 |
| Measurement period | ~10 ms |
| Telemetry | 10 Hz while viewed, 1 Hz otherwise |

---

## 3. Milestones

1. ~~Fix digipot write/readback (§1.5). All 28 couplers should verify across 0–256.~~ Done 2026-10-08.
2. Verify the coupling map, its sign, and the comparator bit order. `ring8` covers all three.
3. Get reliable snapshots, with spins and agreement printed over serial.
4. Move to the layout in §2.2, splitting out the drivers and `IsingMachine`.
5. Add the machine and console tasks, plus the mailboxes and presets.
6. Add the net task, then connect to dev and then prod (SPEC milestones 5–6).
