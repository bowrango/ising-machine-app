// Coupler state across all digipots, in terms of the board's 28 couplers rather than registers.
#pragma once
#include <stdint.h>

#include "../board.h"

namespace couplers {

// Graph with every edge at max coupling, one bit per coupler (bit k = board::COUPLER_PAIRS[k]):
// set = code 0 (min resistance), clear = disconnected.
using BinaryGraph = uint32_t;

// Graph with a coupler code per edge: graph[i][j] = graph[j][i] = code (0..mcp4351::CODE_MAX), or
// mcp4351::DISCONNECTED for no edge. The diagonal is DISCONNECTED.
using WeightedGraph = int16_t[board::NUM_SPINS][board::NUM_SPINS];

// Sets every coupler to code and reads it back.
// Returns bitmask of the digipots (bit = decoder address) whose readback didn't match; 0 = success
uint8_t writeCode(int16_t code);

// Sets graph's edges to code 0 and disconnects the rest. Returns the same bitmask.
uint8_t writeBinaryGraph(BinaryGraph graph);

// Sets each coupler to graph's code for its pair (DISCONNECTED for no edge). Returns the same bitmask.
uint8_t writeWeightedGraph(const WeightedGraph graph);

// Disconnects every coupler. Returns the same bitmask
uint8_t disconnect();

// Reads every coupler back from the digipots into graph. Prints an error and returns false if a
// digipot can't be read.
bool readGraph(WeightedGraph graph);

}  // namespace couplers
