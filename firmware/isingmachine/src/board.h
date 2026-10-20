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

constexpr uint32_t SPI_CLOCK_HZ = 10000000;  // MCP4351 max is 10 MHz

// Decoder addresses 0-6 select digipots U16-U22 (DP_CS0-6).
// Address 7 drives CLK_INH, which also switches the MISO mux to the shift register.
constexpr uint8_t NUM_DIGIPOTS   = 7;
constexpr uint8_t ADDR_SHIFT_REG = 7;

// Reference designators of the digipots, indexed by decoder address.
constexpr const char *DIGIPOT_LABEL[NUM_DIGIPOTS] = {"U16", "U17", "U18", "U19", "U20", "U21", "U22"};

constexpr uint8_t CHANNELS_PER_DIGIPOT = 4;

// Spins (oscillators) A..H = 0..7, and the 28 couplers joining every pair.
constexpr uint8_t NUM_SPINS    = 8;
constexpr uint8_t NUM_COUPLERS = 28;
constexpr char SPIN_LABEL[NUM_SPINS + 1] = "ABCDEFGH";

// Coupler wiring: digipot channel -> spin pair. Derived from the netlist (firmware/README.md §1.4);
// verify on the bench. Each channel is a W-B rheostat joining an _L node to an _R node, so all
// couplings share one sign and only the magnitude is programmable.
struct SpinPair {
  uint8_t i, j;  // i < j
};

// Indexed by coupler k = pot * CHANNELS_PER_DIGIPOT + channel.
constexpr SpinPair COUPLER_PAIRS[NUM_COUPLERS] = {
  {0, 1}, {0, 2}, {0, 3}, {0, 4},  // U16: A-B A-C A-D A-E
  {0, 5}, {0, 6}, {0, 7}, {1, 2},  // U17: A-F A-G A-H B-C
  {1, 3}, {1, 4}, {1, 5}, {1, 6},  // U18: B-D B-E B-F B-G
  {1, 7}, {2, 3}, {2, 4}, {2, 5},  // U19: B-H C-D C-E C-F
  {2, 6}, {2, 7}, {3, 4}, {3, 5},  // U20: C-G C-H D-E D-F
  {3, 6}, {3, 7}, {4, 5}, {4, 6},  // U21: D-G D-H E-F E-G
  {4, 7}, {5, 6}, {5, 7}, {6, 7},  // U22: E-H F-G F-H G-H
};

// Which terminals coupler (i, j), i < j, joins (hardware/schematic/coupling-network.pdf):
// j - i odd joins i_L to j_R, j - i even joins i_R to j_L.
constexpr bool couplerJoinsLeftOfI(uint8_t i, uint8_t j) { return (j - i) % 2; }

// Oscillator header pins on the coupler board: _L on J1A, _R on J1B.
constexpr uint8_t J1A_PIN_L[NUM_SPINS] = {5, 9, 11, 7, 1, 13, 15, 3};
constexpr uint8_t J1B_PIN_R[NUM_SPINS] = {6, 10, 12, 8, 2, 14, 16, 4};

constexpr uint8_t couplerPot(uint8_t k) { return k / CHANNELS_PER_DIGIPOT; }
constexpr uint8_t couplerChannel(uint8_t k) { return k % CHANNELS_PER_DIGIPOT; }

// Coupler index for a spin pair in either order, or -1 if i == j.
inline int8_t couplerIndex(uint8_t i, uint8_t j) {
  if (i > j) { uint8_t t = i; i = j; j = t; }
  for (uint8_t k = 0; k < NUM_COUPLERS; k++) {
    if (COUPLER_PAIRS[k].i == i && COUPLER_PAIRS[k].j == j) return k;
  }
  return -1;
}

}  // namespace board
