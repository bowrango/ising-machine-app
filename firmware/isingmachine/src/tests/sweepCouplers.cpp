#include <Arduino.h>

#include "../board.h"
#include "../drivers/mcp4351.h"
#include "tests.h"

namespace tests {

// Exhaustive sweep over all codes for all 28 channels
void sweepCouplers() {
  Serial.println("=== Begin sweepCouplers ===");

  const uint32_t t0 = micros();

  for (int16_t code = mcp4351::DISCONNECTED; code <= mcp4351::CODE_MAX; code++) {

    // Write
    const int16_t codes[mcp4351::NUM_CHANNELS] = {code, code, code, code};
    for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
      mcp4351::writeWipers(pot, codes);
    }

    // Read
    for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
      uint16_t raw[mcp4351::NUM_WORDS];
      int16_t got[mcp4351::NUM_CHANNELS];
      mcp4351::readWipers(pot, got, raw);
      // Print mismatches
      for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
        if (got[ch] == code) continue;
        Serial.printf("  %s ch=%d: wrote %d read %d (raw=0x%04X)\n",
                      board::DIGIPOT_LABEL[pot], ch, code, got[ch], raw[2 + ch]);
      }
    }
  }
  const uint32_t elapsedUs = micros() - t0;
  
  Serial.printf("  %.1f ms\n", elapsedUs / 1000.0);
  Serial.println("=== End sweepCouplers ===");
}

}  // namespace tests
