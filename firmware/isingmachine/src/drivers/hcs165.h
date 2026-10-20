// SN74HCS165 parallel-in shift register (U15): snapshots the 8 window-comparator outputs.
// Datasheet: https://www.ti.com/lit/ds/symlink/sn74hcs165.pdf
//
// SH/LD low loads A..H in parallel; SH/LD high shifts on rising CLK, QH first (H).
#pragma once
#include <stdint.h>

namespace hcs165 {

// Latches WIN_A..WIN_H at the same instant and shifts them out.
// Bit i = oscillator i (A = bit 0, H = bit 7).
uint8_t read();

}  // namespace hcs165
