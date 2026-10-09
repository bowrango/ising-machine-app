// Bench tests, run from the serial menu in isingmachine.ino.
// The decoder sweep is manual (probe with a multimeter); the others return a Result.
#pragma once

namespace tests {

// Skip: the test couldn't exercise the hardware (e.g. oscillator board disconnected).
// Only Fail indicates a fault.
enum class Result { Pass, Fail, Skip };

const char *toString(Result r);

void decoderSweep();
Result digipotReadback();
Result shiftRegister();
Result couplingMap();

}  // namespace tests
