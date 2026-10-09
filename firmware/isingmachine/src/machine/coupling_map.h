// Coupler wiring: digipot channel (pot, ch) -> spin pair (i, j). Derived from the netlist
// (firmware/README.md §1.4); verify on the bench. Every coupler joins an _L node to an _R node,
// so all couplings share one sign and only the magnitude is programmable.
#pragma once
#include <stdint.h>

#include "../board.h"
#include "../drivers/mcp4351.h"

namespace coupling_map {

struct SpinPair {
  uint8_t i, j;  // i < j; 0 = A ... 7 = H
};

// Indexed by coupler k = pot * 4 + ch.
constexpr SpinPair PAIRS[board::NUM_COUPLERS] = {
  {0, 1}, {0, 2}, {0, 3}, {0, 4},  // U16: A-B A-C A-D A-E
  {0, 5}, {0, 6}, {0, 7}, {1, 2},  // U17: A-F A-G A-H B-C
  {1, 3}, {1, 4}, {1, 5}, {1, 6},  // U18: B-D B-E B-F B-G
  {1, 7}, {2, 3}, {2, 4}, {2, 5},  // U19: B-H C-D C-E C-F
  {2, 6}, {2, 7}, {3, 4}, {3, 5},  // U20: C-G C-H D-E D-F
  {3, 6}, {3, 7}, {4, 5}, {4, 6},  // U21: D-G D-H E-F E-G
  {4, 7}, {5, 6}, {5, 7}, {6, 7},  // U22: E-H F-G F-H G-H
};

constexpr char SPIN_LABEL[board::NUM_SPINS + 1] = "ABCDEFGH";

constexpr uint8_t potOf(uint8_t k) { return k / mcp4351::NUM_CHANNELS; }
constexpr uint8_t channelOf(uint8_t k) { return k % mcp4351::NUM_CHANNELS; }

// Coupler index for a pair in either order, or -1 if i == j.
inline int8_t indexOf(uint8_t i, uint8_t j) {
  if (i > j) { uint8_t t = i; i = j; j = t; }
  for (uint8_t k = 0; k < board::NUM_COUPLERS; k++) {
    if (PAIRS[k].i == i && PAIRS[k].j == j) return k;
  }
  return -1;
}

}  // namespace coupling_map
