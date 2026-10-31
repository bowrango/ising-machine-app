// Oscillator phases, read through the comparators. The oscillators are assumed powered (SW1 on): the
// firmware can't switch them.
#pragma once
#include <stdint.h>

#include "../board.h"

namespace oscillators {

// Takes shots comparator snapshots. agree[i][j] counts the snapshots where spins i and j had the same bit
void sample(uint32_t shots, uint32_t agree[board::NUM_SPINS][board::NUM_SPINS]);

}  // namespace oscillators
