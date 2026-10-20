#include <Arduino.h>

#include "../board.h"
#include "../drivers/mcp4351.h"
#include "tests.h"

namespace tests {

// Exhaustive sweep over all 0..256 codes for all 28 channels.
// At each code step, all 28 channels are written first then read back
// Wipers are written and read 4 at a time
void sweepCouplers() {
  Serial.println("=== Begin sweepCouplers ===");

  // Connect W and B on every channel.
  const int16_t start[mcp4351::NUM_CHANNELS] = {0, 0, 0, 0};
  for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) mcp4351::setChannels(pot, start);

  const uint32_t t0 = micros();
  for (int16_t code = 0; code <= mcp4351::CODE_MAX; code++) {
    const int16_t codes[mcp4351::NUM_CHANNELS] = {code, code, code, code};

    // Write codes
    for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) mcp4351::writeWipers(pot, codes);

    // Read codes
    for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
      int16_t got[mcp4351::NUM_CHANNELS];
      uint16_t raw[mcp4351::NUM_CHANNELS];
      mcp4351::readWipers(pot, got, raw);
      for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
        if (got[ch] == code) continue;
        Serial.printf("  %s ch=%d: wrote %d read %d (raw=0x%04X)\n",
                      board::DIGIPOT_LABEL[pot], ch, code, got[ch], raw[ch]);
      }
    }
  }

  const uint32_t elapsedUs = micros() - t0;
  Serial.printf("  %.1f ms\n", elapsedUs / 1000.0);
  Serial.println("=== End sweepCouplers ===");
}

}  // namespace tests
