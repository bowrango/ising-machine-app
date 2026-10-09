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

void begin() {
  // Idle in parallel-load mode.
  pinMode(board::PIN_SH_LD, OUTPUT);
  digitalWrite(board::PIN_SH_LD, LOW);
}

uint8_t readSnapshot() {
  // The SN74HCS165 shifts on rising edges; mode 2 samples before each shift.
  bus::beginTransaction(SPI_MODE2);
  bus::releaseCs();
  bus::setAddress(board::ADDR_SHIFT_REG);  // CLK_INH low, MISO mux -> QH
  digitalWrite(board::PIN_SH_LD, LOW);
  bus::assertCs();
  delayMicroseconds(LOAD_PULSE_US);  // Allow the parallel inputs to load and QH to settle.
  digitalWrite(board::PIN_SH_LD, HIGH);
  delayMicroseconds(LOAD_TO_CLOCK_US);
  const uint8_t result = bus::transfer8(0);  // H..A map to bits 7..0.
  bus::releaseCs();
  digitalWrite(board::PIN_SH_LD, LOW);
  bus::endTransaction();
  return result;
}

}  // namespace hcs165
