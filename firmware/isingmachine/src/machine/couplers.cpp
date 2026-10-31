#include "couplers.h"

#include <Arduino.h>

#include "../drivers/mcp4351.h"

namespace couplers {

namespace {

// Writes graph's code for every coupler and reads them back. Returns the failed-digipot bitmask.
uint8_t write(const WeightedGraph graph) {
  uint8_t failed = 0;
  for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
    int16_t want[mcp4351::NUM_CHANNELS], got[mcp4351::NUM_CHANNELS];
    for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
      const auto [i, j] = board::COUPLER_PAIRS[pot * mcp4351::NUM_CHANNELS + ch];
      want[ch] = graph[i][j];
    }
    mcp4351::writeWipers(pot, want);
    mcp4351::readWipers(pot, got);
    for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
      if (got[ch] != want[ch]) failed |= 1 << pot;
    }
  }
  return failed;
}

// Fills graph with code on every edge (i != j) and DISCONNECTED on the diagonal.
void fill(WeightedGraph graph, int16_t code) {
  for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
    for (uint8_t j = 0; j < board::NUM_SPINS; j++) graph[i][j] = i == j ? mcp4351::DISCONNECTED : code;
  }
}

}  // namespace

uint8_t writeCode(int16_t code) {
  WeightedGraph graph;
  fill(graph, code);
  return write(graph);
}

uint8_t writeBinaryGraph(BinaryGraph binary) {
  WeightedGraph graph;
  fill(graph, mcp4351::DISCONNECTED);
  for (uint8_t k = 0; k < board::NUM_COUPLERS; k++) {
    const auto [i, j] = board::COUPLER_PAIRS[k];
    if (binary >> k & 1) graph[i][j] = graph[j][i] = 0;
  }
  return write(graph);
}

uint8_t writeWeightedGraph(const WeightedGraph graph) { return write(graph); }

uint8_t disconnect() { return writeCode(mcp4351::DISCONNECTED); }

bool readGraph(WeightedGraph graph) {
  fill(graph, mcp4351::DISCONNECTED);
  bool ok = true;
  for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
    int16_t codes[mcp4351::NUM_CHANNELS];
    mcp4351::readWipers(pot, codes);
    for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
      if (codes[ch] == mcp4351::READ_ERROR) {
        Serial.printf("Error: couldn't read %s channel %u\n", board::DIGIPOT_LABEL[pot], ch);
        ok = false;
      } else {
        const auto [i, j] = board::COUPLER_PAIRS[pot * mcp4351::NUM_CHANNELS + ch];
        graph[i][j] = graph[j][i] = codes[ch];
      }
    }
  }
  return ok;
}

}  // namespace couplers
