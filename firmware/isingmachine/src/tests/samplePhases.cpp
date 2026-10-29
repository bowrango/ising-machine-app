#include <Arduino.h>

#include "../board.h"
#include "../drivers/hcs165.h"
#include "tests.h"

namespace tests {

// Takes a burst of comparator snapshots, prints the first few with the spins relative to A, and
// prints how often each spin agrees with A across all shots.
// Bits only change while the oscillators are running (oscillator board connected, SW1 on).
void samplePhases(const char *arg) {
  constexpr uint32_t DEFAULT_SHOTS = 16;
  constexpr uint8_t MAX_PRINTED = 16;
  const long n = *arg ? atol(arg) : DEFAULT_SHOTS;
  if (n < 1) {
    Serial.println("Usage: samplePhases [shots >= 1]");
    return;
  }
  const uint32_t shots = n;

  Serial.println("=== Begin samplePhases ===");
  uint8_t samples[MAX_PRINTED];
  uint32_t agree[board::NUM_SPINS] = {};
  for (uint32_t s = 0; s < shots; s++) {
    const uint8_t raw = hcs165::read();
    if (s < MAX_PRINTED) samples[s] = raw;
    const uint8_t ref = raw & 1;
    for (uint8_t i = 0; i < board::NUM_SPINS; i++) agree[i] += ((raw >> i) & 1) == ref;
  }

  const uint8_t printed = shots < MAX_PRINTED ? shots : MAX_PRINTED;
  Serial.printf("  first %u of %lu shots\n", printed, (unsigned long)shots);
  Serial.println("  raw      HGFEDCBA  spins A..H");
  for (uint8_t s = 0; s < printed; s++) {
    const uint8_t raw = samples[s];
    char bits[9], spins[9];
    for (uint8_t b = 0; b < 8; b++) bits[7 - b] = (raw >> b) & 1 ? '1' : '0';
    const uint8_t ref = raw & 1;
    for (uint8_t i = 0; i < board::NUM_SPINS; i++) spins[i] = ((raw >> i) & 1) == ref ? '+' : '-';
    bits[8] = spins[8] = '\0';
    Serial.printf("  0x%02X     %s  %s\n", raw, bits, spins);
  }

  Serial.print("  agree with A:");
  for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
    Serial.printf("  %c %5.1f%%", board::SPIN_LABEL[i], 100.0 * agree[i] / shots);
  }
  Serial.println();

  Serial.println("=== End samplePhases ===");
}

}  // namespace tests
