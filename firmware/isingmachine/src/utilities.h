// CLI helpers: input parsing and value checks (each prints its own error and returns false on bad
// input), and printers for the CLI's replies.
#pragma once
#include <stdint.h>

#include "machine/couplers.h"

namespace utilities {

// Splits text at its first space: terminates the first word and returns the rest, or nullptr if
// there's only one word.
char *splitWord(char *text);

// Parses an integer from arg. Prints an error and returns false if arg is missing or isn't one.
bool parseNumber(const char *arg, long &value);

// Parses an optional shot count: dflt if arg is missing.
bool parseShots(const char *arg, long dflt, long &shots);

// True if code is a valid coupler code (0..mcp4351::CODE_MAX); prints an error otherwise.
bool verifyCode(long code);

// True if shots is a valid shot count (>= 1); prints an error otherwise.
bool verifyShots(long shots);

// Creates a graph from an edge list of spin-letter pairs separated by commas, e.g. "AB,BC,CD,DA".
// Prints an error and returns false if text isn't one.
bool createBinaryGraph(const char *text, couplers::BinaryGraph &graph);

// Creates a graph from an edge list with codes, e.g. "AB:0,BC:128"; unlisted pairs are disconnected.
// Prints an error and returns false if text isn't one.
bool createWeightedGraph(const char *text, couplers::WeightedGraph graph);

// Prints graph's edges with their codes, e.g. "AB:0,BC:128", or "none".
void printGraph(const couplers::WeightedGraph graph);

// Prints one row per spin from oscillators::sample: its label, then the % of shots each spin A..H
// matched it.
void printSample(const uint32_t agree[board::NUM_SPINS][board::NUM_SPINS], uint32_t shots);

}  // namespace utilities
