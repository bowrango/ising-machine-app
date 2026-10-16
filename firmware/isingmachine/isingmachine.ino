// Ising Machine, ESP32 Feather Firmware
// Date: Oct 08, 2026
//
// Layout (Arduino compiles the sketch folder and src/ recursively):
//   src/board.h             pin map, board constants, coupler wiring (spin pairs <-> digipot channels)
//   src/drivers/            SPI bus, MCP4351 digipots, SN74HCS165 shift register
//   src/machine/            coupler state across all digipots
//   src/tests/              bench tests, run from the serial menu below
// Hardware notes: firmware/README.md

#include "src/drivers/bus.h"
#include "src/drivers/hcs165.h"
#include "src/board.h"
#include "src/machine/couplers.h"
#include "src/tests/tests.h"

static constexpr size_t MAX_LINE = 64;

static void printHelp() {
  Serial.println("Commands:");
  Serial.println("  digipots  every code 0..256 on all 28 digipot channels (timed)");
  Serial.println("  sr        shift-register snapshots + relative spins");
  Serial.println("  map       check the coupling map table");
  Serial.println("  osccal    all couplers off, phase of each oscillator vs A over 10 rounds");
  Serial.println("  all       map + digipots + sr");
  Serial.println("  decoder   decoder sweep, 5 s per address (probe U13 with a multimeter)");
  Serial.println("  help      this list");
}

static void runCommand(const char *cmd) {
  using tests::Result;
  if (!strcmp(cmd, "digipots")) {
    tests::digipotReadback();
  } else if (!strcmp(cmd, "sr")) {
    tests::shiftRegister();
  } else if (!strcmp(cmd, "map")) {
    tests::couplingMap();
  } else if (!strcmp(cmd, "osccal")) {
    tests::oscillatorCalibrate();
  } else if (!strcmp(cmd, "all")) {
    // Fail if any test fails; Skip doesn't count as a failure.
    const Result results[] = {tests::couplingMap(), tests::digipotReadback(), tests::shiftRegister()};
    uint8_t failed = 0, skipped = 0;
    for (Result r : results) {
      failed += r == Result::Fail;
      skipped += r == Result::Skip;
    }
    Serial.printf("=== all: %s (%d failed, %d skipped) ===\n", failed ? "FAIL" : "PASS", failed, skipped);
  } else if (!strcmp(cmd, "decoder")) {
    tests::decoderSweep();
  } else if (!strcmp(cmd, "help")) {
    printHelp();
  } else {
    Serial.printf("Unknown command '%s'\n", cmd);
    printHelp();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  bus::begin();
  hcs165::begin();
  Serial.println("\n[BOOT] Ising machine firmware");

  // Start from a known state: every coupler off, so a reset disconnects them all.
  if (const uint8_t failed = couplers::disconnectAll()) {
    Serial.print("[BOOT] WARNING: couplers not confirmed off on");
    for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
      if (failed & (1 << pot)) Serial.printf(" %s", board::DIGIPOT_LABEL[pot]);
    }
    Serial.println(" (coupler board disconnected?)");
  } else {
    Serial.println("[BOOT] all couplers off");
  }

  printHelp();
  Serial.print("> ");
}

void loop() {
  // Fixed-size line buffer: no heap use, and input without a line ending can't grow it.
  static char line[MAX_LINE + 1];
  static size_t len = 0;
  static bool overflow = false;

  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\r' || c == '\n') {
      // Handles \n, \r and \r\n line endings; blank lines are ignored.
      line[len] = '\0';
      if (overflow) {
        Serial.printf("\nLine too long (max %u characters)\n> ", unsigned(MAX_LINE));
      } else {
        // Trim surrounding whitespace in place.
        char *cmd = line;
        while (isspace(uint8_t(*cmd))) cmd++;
        for (char *end = line + len; end > cmd && isspace(uint8_t(end[-1])); end--) end[-1] = '\0';
        if (*cmd) {
          Serial.println(cmd);
          runCommand(cmd);
          Serial.print("> ");
        }
      }
      len = 0;
      overflow = false;
    } else if (len < MAX_LINE) {
      line[len++] = c;
    } else {
      overflow = true;
    }
  }
}
