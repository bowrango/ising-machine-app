#pragma once
#include <stdint.h>

namespace tests {

// Samples the comparators shots times (default 16) and prints each spin's agreement with A.
void samplePhases(const char *arg);
void sweepDecoder();
void sweepCouplers();
// Sets all 28 couplers to code (0..256) and prints the expected L-R resistance at each
// oscillator's header pins. Couplers stay set afterwards.
void measureCouplers(int16_t code);

}  // namespace tests
