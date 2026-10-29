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
#include "src/drivers/mcp4351.h"
#include "src/board.h"
#include "src/machine/couplers.h"
#include "src/tests/tests.h"

static constexpr size_t MAX_LINE = 64;

// Parses a coupler code from arg. Prints an error and returns false if it isn't a valid code.
static bool parseCode(const char *arg, int16_t &code) {
  char *end;
  const long n = strtol(arg, &end, 10);
  if (!*arg || *end || !couplers::verifyCode(n)) {
    Serial.printf("Error: code must be 0..%d\n", mcp4351::CODE_MAX);
    return false;
  }
  code = n;
  return true;
}

static void printHelp() {
  Serial.println("Commands:");
  Serial.println("  samplePhases [shots]");
  Serial.println("  sweepDecoder");
  Serial.println("  sweepCouplers");
  Serial.println("  setCouplers [code]");
  Serial.println("  measureCouplers [code]");
  Serial.println("  disconnect");
  Serial.println("  help");
}

static void runCommand(const char *cmd) {
  if (!strcmp(cmd, "samplePhases") || !strncmp(cmd, "samplePhases ", 13)) {
    tests::samplePhases(cmd + 12);
  } else if (!strcmp(cmd, "sweepDecoder")) {
    tests::sweepDecoder();
  } else if (!strcmp(cmd, "sweepCouplers")) {
    tests::sweepCouplers();
  } else if (!strncmp(cmd, "setCouplers ", 12)) {
    int16_t code;
    if (parseCode(cmd + 12, code)) couplers::setAll(code);
  } else if (!strncmp(cmd, "measureCouplers ", 16)) {
    int16_t code;
    if (parseCode(cmd + 16, code)) tests::measureCouplers(code);
  } else if (!strcmp(cmd, "disconnect")) {
    couplers::disconnect();
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
  bus::setup();
  Serial.println("\n[BOOT] Ising machine firmware");

  // Disconnect all couplers for simple reset
  couplers::disconnect();
  printHelp();
}

void loop() {
  // Fixed-size line buffer
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
