#include <Arduino.h>

#include "../board.h"
#include "../drivers/hcs165.h"
#include "../machine/coupling_map.h"
#include "tests.h"

namespace tests {

// Takes a burst of comparator snapshots and prints each one with the spins relative to A.
// Passes if any bit changes across the burst, i.e. the oscillators are running (SW1 on).
// Identical snapshots are reported as SKIP: with the oscillator board disconnected or SW1 off
// the comparator inputs don't move, which isn't a fault in the readout path.
// Spins are only meaningful once the oscillators are locked by SYNC (README §1.3).
Result shiftRegister() {
  Serial.println("=== shiftRegister ===");
  constexpr uint8_t NUM_SAMPLES = 16;
  uint8_t samples[NUM_SAMPLES];
  for (uint8_t s = 0; s < NUM_SAMPLES; s++) {
    samples[s] = hcs165::readSnapshot();
    delayMicroseconds(10);
  }

  Serial.println("  raw      HGFEDCBA  spins A..H");
  uint8_t changed = 0;
  for (uint8_t s = 0; s < NUM_SAMPLES; s++) {
    const uint8_t raw = samples[s];
    changed |= raw ^ samples[0];
    char bits[9], spins[9];
    for (uint8_t b = 0; b < 8; b++) bits[7 - b] = (raw >> b) & 1 ? '1' : '0';
    const uint8_t ref = raw & 1;
    for (uint8_t i = 0; i < board::NUM_SPINS; i++) spins[i] = ((raw >> i) & 1) == ref ? '+' : '-';
    bits[8] = spins[8] = '\0';
    Serial.printf("  0x%02X     %s  %s\n", raw, bits, spins);
  }

  const Result result = changed ? Result::Pass : Result::Skip;
  Serial.printf("=== end shiftRegister: %s, %s ===\n", toString(result),
                changed ? "comparator bits are changing"
                        : "all snapshots identical (oscillator board disconnected or SW1 off?)");
  return result;
}

}  // namespace tests
