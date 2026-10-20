#include "hcs165.h"

#include <Arduino.h>
#include <SPI.h>

#include "../board.h"
#include "bus.h"

namespace hcs165 {

namespace {
// Conservative margins; the SN74HCS165's SH/LD pulse width and SH/LD-to-CLK setup
// minimums are tens of nanoseconds. Revisit together with the SPI clock.
constexpr uint32_t LOAD_PULSE_US = 1;
constexpr uint32_t LOAD_TO_CLOCK_US = 1;
}  // namespace

uint8_t read() {
  // Mode 2: SCLK idles high and the ESP32 samples on falling edges, half a clock before each
  // rising edge shifts the register. QH (input H) is valid before the first edge.
  bus::beginTransaction(SPI_MODE2);
  bus::writeAddress(board::ADDR_SHIFT_REG);
  bus::load();
  // CLK_INH goes low and the MISO mux switches to QH. SCLK is already high, so ungating the clock
  // can't make a rising edge.
  bus::select();
  delayMicroseconds(LOAD_PULSE_US);  // SH/LD low pulse width; QH settles to input H.
  bus::shift();                      // Rising SH/LD: this is the snapshot instant for all 8 inputs.
  delayMicroseconds(LOAD_TO_CLOCK_US);  // SH/LD high setup before the first rising SCLK.
  const uint8_t result = bus::transfer8(0);  // H..A map to bits 7..0.
  bus::deselect();  // CLK_INH high again, MISO mux back to the digipots.
  bus::load();      // Back to idle: continuously loading.
  bus::endTransaction();
  return result;
}

}  // namespace hcs165
