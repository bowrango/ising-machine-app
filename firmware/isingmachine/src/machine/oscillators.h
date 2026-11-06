// Oscillator phases, read through the comparators. The oscillators are assumed powered (SW1 on): the
// firmware can't switch them.
#pragma once
#include <stdint.h>

#include "../board.h"

namespace oscillators {

// generates distribution using shots samples
void sample(uint32_t shots, uint32_t counts[board::NUM_STATES]);

}  // namespace oscillators
