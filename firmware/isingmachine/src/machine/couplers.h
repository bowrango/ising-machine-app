// Coupler state across all digipots, in terms of the board's 28 couplers rather than registers.
#pragma once
#include <stdint.h>

namespace couplers {

// Disconnects every coupler (W and B open on all channels) and reads back TCON to confirm.
// Returns a bitmask of the digipots (bit = decoder address) whose readback didn't show W and B
// open; 0 means every coupler is off.
uint8_t disconnectAll();

}  // namespace couplers
