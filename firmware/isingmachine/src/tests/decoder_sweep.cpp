#include <Arduino.h>

#include "../board.h"
#include "../drivers/bus.h"
#include "../drivers/mcp4351.h"
#include "tests.h"

namespace tests {

// Sweep through all 7 digipot addresses. For each, assert CS for 5 s so you can
// probe the decoder outputs with a multimeter.
// U13 (SN74HCS138DYYR) output pins: Y0=15, Y1=14, Y2=13, Y3=12, Y4=11, Y5=10, Y6=9.
// Selected output → 0V; all others → 3.3V.
void decoderSweep() {
  static const uint8_t Y_PIN[board::NUM_DIGIPOTS] = {15, 14, 13, 12, 11, 10, 9};
  Serial.println("=== decoderSweep ===");
  for (uint8_t addr = 0; addr < board::NUM_DIGIPOTS; addr++) {
    bus::writeAddress(addr);
    const uint8_t read = bus::readAddress();
    const int a0 = read & 1, a1 = (read >> 1) & 1, a2 = (read >> 2) & 1;
    bus::select();
    Serial.printf("[addr=%d %s] GPIO%d(A0)=%d GPIO%d(A1)=%d GPIO%d(A2)=%d → U13 pin %d (Y%d) should be 0V (5s)...\n",
                  addr, board::DIGIPOT_LABEL[addr],
                  board::PIN_ADDR0, a0, board::PIN_ADDR1, a1, board::PIN_ADDR2, a2,
                  Y_PIN[addr], addr);
    delay(5000);
    bus::deselect();
  }
  Serial.println("=== end decoderSweep ===");
}

}  // namespace tests
