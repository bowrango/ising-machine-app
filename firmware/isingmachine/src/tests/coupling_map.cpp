#include <Arduino.h>

#include "../board.h"
#include "../machine/coupling_map.h"
#include "tests.h"

namespace tests {

// Checks that the coupler table covers every spin pair i<j exactly once and that the
// index helpers round-trip. Pure software check; no hardware access.
Result couplingMap() {
  Serial.println("=== couplingMap ===");
  bool pass = true;
  uint8_t seen[board::NUM_SPINS][board::NUM_SPINS] = {};

  for (uint8_t k = 0; k < board::NUM_COUPLERS; k++) {
    const auto p = coupling_map::PAIRS[k];
    if (p.i >= p.j || p.j >= board::NUM_SPINS) {
      Serial.printf("  FAIL k=%d: bad pair (%d,%d)\n", k, p.i, p.j);
      pass = false;
      continue;
    }
    seen[p.i][p.j]++;
    if (coupling_map::indexOf(p.j, p.i) != k) {
      Serial.printf("  FAIL k=%d: indexOf(%c,%c) != k\n", k,
                    coupling_map::SPIN_LABEL[p.j], coupling_map::SPIN_LABEL[p.i]);
      pass = false;
    }
  }
  for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
    for (uint8_t j = i + 1; j < board::NUM_SPINS; j++) {
      if (seen[i][j] != 1) {
        Serial.printf("  FAIL pair %c-%c appears %d times\n",
                      coupling_map::SPIN_LABEL[i], coupling_map::SPIN_LABEL[j], seen[i][j]);
        pass = false;
      }
    }
  }

  Serial.printf("=== end couplingMap: %s ===\n", pass ? "PASS, 28 pairs each covered once" : "FAIL");
  return pass ? Result::Pass : Result::Fail;
}

}  // namespace tests
