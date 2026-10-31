# Ising Machine Firmware

ESP32 Feather V2 firmware for the 8-spin oscillator Ising machine. The web server and device protocol are in the [main README](../README.md). Schematics are in [`../hardware/schematic/`](../hardware/schematic/).

The device is composed of two boards:

- **Oscillator:** 8 differential ~1 MHz LC oscillators, locked to 0 or π by a 2 MHz SYNC signal. Comparators turn each oscillator's phase into a bit, and a shift register snapshots all 8 for the ESP32.
- **Coupler:** 28 digipot couplers, one per spin pair, set over SPI. Coupling strength is programmable and the sign is fixed (antiferromagnetic).

Pins and coupler wiring are in `src/board.h`.

## Build and flash

With [arduino-cli](https://arduino.github.io/arduino-cli/):
```
PORT=$(arduino-cli board list | grep -E "usbserial" | awk '{print $1}')
arduino-cli compile --upload --fqbn esp32:esp32:adafruit_feather_esp32_v2:UploadSpeed=460800 -p "$PORT" firmware/isingmachine
```

## Serial CLI

At boot the firmware disconnects all couplers and listens for commands at 115200 baud, one per line. [`hardware/io/device.py`](../hardware/io/device.py) sends them from Python.

| Command | Test |
|---|---|
| `sweepCouplers` | Writes and reads back every code on every coupler |
| `sweepDecoder` | Steps the chip-select decoder for probing |
| `writeCode <0..256>` | Sets every coupler to one code |
| `measureCouplers <0..256>` | Sets every coupler to one code and prints the expected resistance at each oscillator |
| `writeBinaryGraph <graph>` | Sets the couplers on a graph's edges to code 0 (max coupling) and disconnects the rest. Graph: an edge list like `AB,BC,CA` |
| `writeWeightedGraph <graph>` | Sets each listed edge to its code and disconnects the rest. Graph: edges with codes like `AB:0,BC:128` |
| `readGraph` | Reads the couplers back and prints the connected ones with their codes, e.g. `AB:0,BC:0` |
| `sample [shots]` | Prints, for each spin, the % of shots every spin's comparator matched it |
| `disconnect` | Disconnects every coupler so the oscillators run uncoupled |

## Code layout

All code lives under `firmware/isingmachine/`, since the Arduino build only compiles the sketch folder and its `src/`.

```
firmware/isingmachine/
├── isingmachine.ino      # setup and serial CLI
└── src/
    ├── board.h           # pins and board wiring
    ├── utilities.h       # CLI input parsing and value checks
    ├── drivers/          # SPI bus, MCP4351 digipots, 74HCS165 shift register
    ├── machine/          # device state: couplers, oscillators
    └── tests/            # serial tests
```

Drivers never print, and only `bus` touches the SPI pins.
