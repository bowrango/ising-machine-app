# Ising Machine Firmware

ESP32 Feather V2 firmware for the 8-spin oscillator Ising machine. The web server and device protocol are in the [main README](../README.md). Schematics are in [`../hardware/schematic/`](../hardware/schematic/).

The device is composed of two boards:

- **Oscillator:** 8 differential ~1 MHz LC oscillators, locked to 0 or π by a 2 MHz SYNC signal. Comparators turn each oscillator's phase into a bit, and a shift register snapshots all 8 for the ESP32.
- **Coupler:** 28 digipot couplers, one per spin pair, set over SPI. Coupling strength is programmable and the sign is fixed (antiferromagnetic).

Pins and coupler wiring are in `src/board.h`.

## Build and flash

Use arduino-cli bundled with the Arduino IDE:
```
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
PORT=$("$CLI" board list | grep -E "usbserial" | awk '{print $1}')
"$CLI" compile --upload --fqbn esp32:esp32:adafruit_feather_esp32_v2:UploadSpeed=460800 -p "$PORT" firmware/isingmachine
```

## Serial test menu

At boot the firmware disconnects all couplers and prints a test menu. Type a command and press Enter:

| Command | Test |
|---|---|
| `sweepCouplers` | Writes and reads back every code on every coupler |
| `samplePhases [shots]` | Samples the shift register and prints each spin's agreement with A |
| `sweepDecoder` | Steps the chip-select decoder for probing |
| `setCouplers <0..256>` | Sets every coupler to one code and prints the expected resistance at each oscillator |
| `disconnect` | Disconnects every coupler so the oscillators run uncoupled |

## Code layout

All code lives under `firmware/isingmachine/`, since the Arduino build only compiles the sketch folder and its `src/`.

```
firmware/isingmachine/
├── isingmachine.ino      # setup and serial test menu
└── src/
    ├── board.h           # pins and board wiring
    ├── drivers/          # SPI bus, MCP4351 digipots, 74HCS165 shift register
    ├── machine/          # couplers
    └── tests/            # serial tests
```

Drivers never print, and only `bus` touches the SPI pins.
