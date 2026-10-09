// Board definitions: Ising machine main board + Adafruit ESP32 Feather V2.
// Schematics: hardware/coupler-schematic.PDF, hardware/oscillator-schematic.PDF
#pragma once
#include <stdint.h>

namespace board {

// SPI (HSPI). MOSI is routed through U14 by switch SW4; the firmware uses MOSI1_BK.
constexpr uint8_t PIN_MISO    = 12;  // MISO1. Strapping pin: flash voltage forced to 3.3 V by eFuse.
constexpr uint8_t PIN_MOSI    = 13;  // MOSI1 (shared with the onboard LED, unused)
constexpr uint8_t PIN_MOSI_BK = 32;  // MOSI1_BK
constexpr uint8_t PIN_SCLK    = 14;  // SCK1
constexpr uint8_t PIN_CS      = 21;  // CS1: enables the 3-8 decoder (U13)

// 3-8 decoder (SN74HCS138, U13) address lines
constexpr uint8_t PIN_ADDR0 = 15;
constexpr uint8_t PIN_ADDR1 = 33;
constexpr uint8_t PIN_ADDR2 = 27;

// Parallel-in shift register (SN74HCS165, U15) load, active low
constexpr uint8_t PIN_SH_LD = 19;

constexpr uint32_t SPI_CLOCK_HZ = 1000000;

// Decoder addresses 0-6 select digipots U16-U22 (DP_CS0-6).
// Address 7 drives CLK_INH, which also switches the MISO mux to the shift register.
constexpr uint8_t NUM_DIGIPOTS   = 7;
constexpr uint8_t ADDR_SHIFT_REG = 7;

// Reference designators of the digipots, indexed by decoder address.
constexpr const char *DIGIPOT_LABEL[NUM_DIGIPOTS] = {"U16", "U17", "U18", "U19", "U20", "U21", "U22"};

constexpr uint8_t NUM_SPINS    = 8;
constexpr uint8_t NUM_COUPLERS = 28;

}  // namespace board
