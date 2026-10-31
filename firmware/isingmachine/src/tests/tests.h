#pragma once
#include <stdint.h>

namespace tests {

void sweepDecoder();
void sweepCouplers();
// Sets all 28 couplers to code (0..256) and prints the expected L-R resistance at each
// oscillator's header pins. Couplers stay set afterwards.
void measureCouplers(int16_t code);

}  // namespace tests
