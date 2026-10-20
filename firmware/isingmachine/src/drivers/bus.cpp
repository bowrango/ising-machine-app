#include "bus.h"

#include <Arduino.h>
#include <SPI.h>

#include "../board.h"

namespace bus {

namespace {
SPIClass spi(HSPI);
}

void setup() {
  // Until these pins are driven they float, which can enable the decoder (see README, GPIO12 strapping).
  spi.begin(board::PIN_SCLK, board::PIN_MISO, board::PIN_MOSI_BK, board::PIN_CS);
  // The Arduino API doesn't drive SS automatically; CS1 is a plain GPIO.
  // Idle high: decoder disabled, so no digipot is selected and CLK_INH is high.
  pinMode(board::PIN_CS, OUTPUT);
  deselect();

  pinMode(board::PIN_ADDR0, OUTPUT);
  pinMode(board::PIN_ADDR1, OUTPUT);
  pinMode(board::PIN_ADDR2, OUTPUT);
  writeAddress(0);

  // Idle low: the shift register continuously loads WIN_A..WIN_H.
  pinMode(board::PIN_SH_LD, OUTPUT);
  load();
}

// Deselects first: with CS1 low the decoder outputs would glitch through intermediate addresses
// as the three lines change one at a time.
void writeAddress(uint8_t addr) {
  assert(addr <= 7);
  deselect();
  digitalWrite(board::PIN_ADDR0, addr & 1);
  digitalWrite(board::PIN_ADDR1, (addr >> 1) & 1);
  digitalWrite(board::PIN_ADDR2, (addr >> 2) & 1);
}

uint8_t readAddress() {
  return digitalRead(board::PIN_ADDR0) | (digitalRead(board::PIN_ADDR1) << 1) | (digitalRead(board::PIN_ADDR2) << 2);
}

// CS1 enables the decoder: low drives the addressed output low (a DP_CS, or CLK_INH for address 7).
void select() { digitalWrite(board::PIN_CS, LOW); }
void deselect() { digitalWrite(board::PIN_CS, HIGH); }

// SH_LD low: parallel load (the register follows its inputs). The rising edge to high freezes the
// snapshot, and the register then shifts on each rising SCLK.
void load() { digitalWrite(board::PIN_SH_LD, LOW); }
void shift() { digitalWrite(board::PIN_SH_LD, HIGH); }

// Each driver passes its own mode (see bus.h); the settings apply until endTransaction().
// Deselects first: a mode change moves SCLK's idle level, which would clock a selected device.
void beginTransaction(uint8_t spiMode) {
  deselect();
  spi.beginTransaction(SPISettings(board::SPI_CLOCK_HZ, MSBFIRST, spiMode));
}

void endTransaction() { spi.endTransaction(); }

uint16_t transfer16(uint16_t data) { return spi.transfer16(data); }
uint8_t transfer8(uint8_t data) { return spi.transfer(data); }

}  // namespace bus
