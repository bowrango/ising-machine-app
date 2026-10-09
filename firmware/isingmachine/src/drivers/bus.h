// Shared SPI bus: HSPI + 3-8 decoder chip select + MISO mux.
// Only one task may use the bus; callers sequence the primitives below.
#pragma once
#include <stdint.h>

namespace bus {

// Configures the SPI peripheral, CS1 and the decoder address lines. Call before any driver.
void begin();

// Point the decoder at addr (0-6 digipot, 7 shift register). Takes effect while CS is asserted.
void setAddress(uint8_t addr);

void assertCs();   // CS1 low: decoder output for the current address goes low
void releaseCs();  // CS1 high: all decoder outputs high

void beginTransaction(uint8_t spiMode);
void endTransaction();

uint16_t transfer16(uint16_t data);
uint8_t transfer8(uint8_t data);

}  // namespace bus
