#include "bus.h"

#include <Arduino.h>
#include <SPI.h>

#include "../board.h"

namespace bus {

namespace {
SPIClass spi(HSPI);
}

void begin() {
  pinMode(board::PIN_ADDR0, OUTPUT);
  pinMode(board::PIN_ADDR1, OUTPUT);
  pinMode(board::PIN_ADDR2, OUTPUT);
  setAddress(0);

  spi.begin(board::PIN_SCLK, board::PIN_MISO, board::PIN_MOSI_BK, board::PIN_CS);
  // The Arduino API doesn't drive SS automatically; CS1 is a plain GPIO.
  pinMode(board::PIN_CS, OUTPUT);
  releaseCs();
}

void setAddress(uint8_t addr) {
  assert(addr <= 7);
  digitalWrite(board::PIN_ADDR0, addr & 1);
  digitalWrite(board::PIN_ADDR1, (addr >> 1) & 1);
  digitalWrite(board::PIN_ADDR2, (addr >> 2) & 1);
}

void assertCs() { digitalWrite(board::PIN_CS, LOW); }
void releaseCs() { digitalWrite(board::PIN_CS, HIGH); }

void beginTransaction(uint8_t spiMode) {
  spi.beginTransaction(SPISettings(board::SPI_CLOCK_HZ, MSBFIRST, spiMode));
}

void endTransaction() { spi.endTransaction(); }

uint16_t transfer16(uint16_t data) { return spi.transfer16(data); }
uint8_t transfer8(uint8_t data) { return spi.transfer(data); }

}  // namespace bus
