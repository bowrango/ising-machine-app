#include "utilities.h"

#include <Arduino.h>
#include <string.h>

#include "board.h"
#include "drivers/mcp4351.h"

namespace utilities {

namespace {

// Spin index for a label 'A'..'H', or -1 for anything else.
int8_t spinIndex(char label) {
  for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
    if (board::SPIN_LABEL[i] == label) return i;
  }
  return -1;
}

}  // namespace

char *splitWord(char *text) {
  char *space = strchr(text, ' ');
  if (!space) return nullptr;
  *space = '\0';
  return space + 1;
}

bool parseNumber(const char *arg, long &value) {
  char *end;
  value = arg ? strtol(arg, &end, 10) : 0;
  if (!arg || !*arg || *end) {
    Serial.printf("Error: expected a number, got '%s'\n", arg ? arg : "");
    return false;
  }
  return true;
}

bool parseShots(const char *arg, long dflt, long &shots) {
  if (!arg) {
    shots = dflt;
    return true;
  }
  return parseNumber(arg, shots);
}

bool verifyCode(long code) {
  if (code >= 0 && code <= mcp4351::CODE_MAX) return true;
  Serial.printf("Error: code must be 0..%d\n", mcp4351::CODE_MAX);
  return false;
}

bool verifyShots(long shots) {
  if (shots >= 1) return true;
  Serial.println("Error: shots must be >= 1");
  return false;
}

bool createBinaryGraph(const char *text, couplers::BinaryGraph &graph) {
  graph = 0;
  for (const char *e = text;; e += 3) {
    const int8_t i = spinIndex(e[0]), j = i < 0 ? -1 : spinIndex(e[1]);
    if (i < 0 || j < 0 || i == j || (e[2] != ',' && e[2] != '\0')) {
      Serial.printf("Error: invalid edge list '%s'\n", text);
      return false;
    }
    graph |= 1UL << board::couplerIndex(i, j);
    if (e[2] == '\0') return true;
  }
}

bool createWeightedGraph(const char *text, couplers::WeightedGraph graph) {
  for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
    for (uint8_t j = 0; j < board::NUM_SPINS; j++) graph[i][j] = mcp4351::DISCONNECTED;
  }
  for (const char *e = text;;) {
    const int8_t i = spinIndex(e[0]), j = i < 0 ? -1 : spinIndex(e[1]);
    char *end;
    const long code = i < 0 || j < 0 || e[2] != ':' ? -1 : strtol(e + 3, &end, 10);
    if (i == j || code < 0 || code > mcp4351::CODE_MAX || end == e + 3 || (*end != ',' && *end != '\0')) {
      Serial.printf("Error: invalid weighted edge list '%s'\n", text);
      return false;
    }
    graph[i][j] = graph[j][i] = code;
    if (*end == '\0') return true;
    e = end + 1;
  }
}

void printGraph(const couplers::WeightedGraph graph) {
  bool first = true;
  for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
    for (uint8_t j = i + 1; j < board::NUM_SPINS; j++) {
      if (graph[i][j] == mcp4351::DISCONNECTED) continue;
      Serial.printf("%s%c%c:%d", first ? "" : ",", board::SPIN_LABEL[i], board::SPIN_LABEL[j], graph[i][j]);
      first = false;
    }
  }
  Serial.println(first ? "none" : "");
}

void printSample(const uint32_t agree[board::NUM_SPINS][board::NUM_SPINS], uint32_t shots) {
  for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
    Serial.print(board::SPIN_LABEL[i]);
    for (uint8_t j = 0; j < board::NUM_SPINS; j++) Serial.printf(" %.1f", 100.0 * agree[i][j] / shots);
    Serial.println();
  }
}

}  // namespace utilities
