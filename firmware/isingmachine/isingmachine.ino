// Ising Machine, ESP32 Feather Firmware
// Date: Oct 08, 2026
//
// Layout (Arduino compiles the sketch folder and src/ recursively):
//   src/board.h             pin map, board constants, coupler wiring (spin pairs <-> digipot channels)
//   src/drivers/            SPI bus, MCP4351 digipots, SN74HCS165 shift register
//   src/machine/            device state: couplers and oscillators (mirrored by hardware/io/device.py)
//   src/tests/              bench tests, run from the serial CLI below
// Hardware notes: firmware/README.md

#include "src/drivers/bus.h"
#include "src/drivers/mcp4351.h"
#include "src/board.h"
#include "src/machine/couplers.h"
#include "src/machine/oscillators.h"
#include "src/tests/tests.h"
#include "src/utilities.h"

static constexpr size_t MAX_LINE = 256;  // fits a full 28-edge weighted graph

static constexpr uint32_t SAMPLE_SHOTS = 10000;  // default shots for sample

static void printHelp() {
  Serial.println("Commands:");
  Serial.println("  sweepDecoder");
  Serial.println("  sweepCouplers");
  Serial.println("  writeCode [code]");
  Serial.println("  measureCouplers [code]");
  Serial.println("  writeBinaryGraph <graph> edges at code 0, e.g. AB,BC,CA; the rest disconnected");
  Serial.println("  writeWeightedGraph <graph> edges with codes, e.g. AB:0,BC:128; the rest disconnected");
  Serial.println("  readGraph                connected couplers and codes, e.g. AB:0,BC:0");
  Serial.println("  sample [shots]           % of shots each pair of spins matched");
  Serial.println("  disconnect");
  Serial.println("  help");
}

static void runCommand(const char *cmd) {
  char line[MAX_LINE + 1];
  strncpy(line, cmd, MAX_LINE);
  line[MAX_LINE] = '\0';
  char *arg = utilities::splitWord(line);  // line is now the command word
  couplers::BinaryGraph binary;
  couplers::WeightedGraph weighted;
  long code, shots;

  if (!strcmp(line, "sweepDecoder")) {
    tests::sweepDecoder();
  } else if (!strcmp(line, "sweepCouplers")) {
    tests::sweepCouplers();
  } else if (!strcmp(line, "writeCode")) {
    if (utilities::parseNumber(arg, code) && utilities::verifyCode(code)) couplers::writeCode(code);
  } else if (!strcmp(line, "measureCouplers")) {
    if (utilities::parseNumber(arg, code) && utilities::verifyCode(code)) tests::measureCouplers(code);
  } else if (!strcmp(line, "writeBinaryGraph")) {
    if (!arg) {
      Serial.println("Error: expected writeBinaryGraph <graph>");
    } else if (utilities::createBinaryGraph(arg, binary)) {
      couplers::writeBinaryGraph(binary);
    }
  } else if (!strcmp(line, "writeWeightedGraph")) {
    if (!arg) {
      Serial.println("Error: expected writeWeightedGraph <graph>");
    } else if (utilities::createWeightedGraph(arg, weighted)) {
      couplers::writeWeightedGraph(weighted);
    }
  } else if (!strcmp(line, "readGraph")) {
    if (couplers::readGraph(weighted)) utilities::printGraph(weighted);
  } else if (!strcmp(line, "sample")) {
    if (utilities::parseShots(arg, SAMPLE_SHOTS, shots) && utilities::verifyShots(shots)) {
      uint32_t agree[board::NUM_SPINS][board::NUM_SPINS];
      oscillators::sample(shots, agree);
      utilities::printSample(agree, shots);
    }
  } else if (!strcmp(line, "disconnect")) {
    couplers::disconnect();
  } else if (!strcmp(line, "help")) {
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
