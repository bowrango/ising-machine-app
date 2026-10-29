// Coupler state across all digipots, in terms of the board's 28 couplers rather than registers.
#pragma once
#include <stdint.h>

namespace couplers {

// True if code is a valid coupler code (0..mcp4351::CODE_MAX).
bool verifyCode(long code);

// Sets every coupler to code and reads it back.
// Returns bitmask of the digipots (bit = decoder address) whose readback didn't match; 0 = success
uint8_t setAll(int16_t code);

// Disconnects every coupler. Returns the same bitmask
uint8_t disconnect();

}  // namespace couplers
