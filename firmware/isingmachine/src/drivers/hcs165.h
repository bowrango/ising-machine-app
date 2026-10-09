// SN74HCS165 parallel-in shift register (U15): snapshots the 8 window-comparator outputs.
#pragma once
#include <stdint.h>

namespace hcs165 {

// Configures SH_LD. Call after bus::begin().
void begin();

// Latches WIN_A..WIN_H at the same instant and shifts them out.
// Bit i = oscillator i (A = bit 0, H = bit 7).
uint8_t readSnapshot();

}  // namespace hcs165
