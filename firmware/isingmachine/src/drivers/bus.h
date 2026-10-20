// Shared SPI bus: HSPI + 3-8 decoder chip select + MISO mux + shift-register SH_LD.
// Only one task may use the bus; callers sequence the primitives below.
#pragma once
#include <stdint.h>

namespace bus {

// Configures the SPI peripheral, CS1, SH_LD and the decoder address lines. Call before any driver.
void setup();

// Point the decoder at addr (0-6 digipot, 7 shift register). Takes effect while CS is asserted.
void writeAddress(uint8_t addr);

// Address currently on the decoder lines, read back from the GPIOs.
uint8_t readAddress();

void select();    // CS1 low: decoder output for the current address goes low
void deselect();  // CS1 high: all decoder outputs high

void load();   // SH_LD low: shift register loads its parallel inputs (idle state)
void shift();  // SH_LD high: shift register shifts on SCLK

// Holds the bus at SPI_CLOCK_HZ, MSB first, until endTransaction(). Call while deselected.
//   SPI_MODE0: SCLK idles low, sample on rising edge (MCP4351)
//   SPI_MODE2: SCLK idles high, sample on falling edge (SN74HCS165)
void beginTransaction(uint8_t spiMode);
void endTransaction();

uint16_t transfer16(uint16_t data);
uint8_t transfer8(uint8_t data);

}  // namespace bus
