#include <Arduino.h>

#include "../board.h"
#include "../drivers/mcp4351.h"
#include "tests.h"

namespace tests {

// Exhaustive digipot check: every code 0..256 on all 28 channels.
// For each step, all 7 pots are written first and then all 7 read back, with a different
// offset per channel. Every channel still visits every code, and a write decoded to the
// wrong pot (decoder fault) or wrong channel shows up as a mismatch.
// Wipers are written and read 4 at a time per CS assertion to keep the sweep fast.
//
// History: data was once shifted to bits [9:2] (`w << 2`). Writing 50 actually set the wiper
// to 200, and the read decoded CMDERR (bit 9) as bit 7, giving 178. Fixed 2026-10-08.
Result digipotReadback() {
  Serial.println("=== digipotReadback ===");
  constexpr int16_t NUM_CODES = mcp4351::CODE_MAX + 1;  // 257
  constexpr uint8_t MAX_REPORTED = 20;

  // Connect W and B on every channel (codes below are all >= 0).
  const int16_t start[mcp4351::NUM_CHANNELS] = {0, 0, 0, 0};
  for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) mcp4351::setChannels(pot, start);

  uint32_t failures = 0;
  const uint32_t t0 = micros();

  for (int16_t step = 0; step < NUM_CODES; step++) {
    int16_t expected[board::NUM_DIGIPOTS][mcp4351::NUM_CHANNELS];
    for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
      for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
        expected[pot][ch] = (step + 37 * (pot * mcp4351::NUM_CHANNELS + ch)) % NUM_CODES;
      }
      mcp4351::writeWipers(pot, expected[pot]);
    }

    for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
      int16_t got[mcp4351::NUM_CHANNELS];
      uint16_t raw[mcp4351::NUM_CHANNELS];
      mcp4351::readWipers(pot, got, raw);
      for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
        if (got[ch] == expected[pot][ch]) continue;
        if (failures < MAX_REPORTED) {
          Serial.printf("  FAIL %s ch=%d: expected %d got %d (raw=0x%04X)\n",
                        board::DIGIPOT_LABEL[pot], ch, expected[pot][ch], got[ch], raw[ch]);
        }
        failures++;
      }
    }
  }

  const uint32_t elapsedUs = micros() - t0;
  const uint32_t total = uint32_t(NUM_CODES) * board::NUM_COUPLERS;
  if (failures > MAX_REPORTED) Serial.printf("  ... %lu more failures not shown\n", failures - MAX_REPORTED);
  Serial.printf("=== end digipotReadback: %s, %lu/%lu channel-codes correct in %.1f ms ===\n",
                failures ? "FAIL" : "PASS", total - failures, total, elapsedUs / 1000.0);
  return failures ? Result::Fail : Result::Pass;
}

}  // namespace tests
