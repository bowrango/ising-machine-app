// MCP4351-103 quad 10 kΩ digital potentiometer (U16-U22).
// Datasheet: https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/22242A.pdf
//
// 16-bit command (Section 7): [15:12] register | [11:10] command | [9:0] data.
// Wiper and TCON data occupy D8:D0 (bits [8:0]); D9 is unused on writes.
// On reads, SDO returns 1s in bits [15:10], CMDERR in bit 9 (1 = valid), and the register in bits [8:0].
//
// `pot` is the decoder address of the chip (0 .. board::NUM_DIGIPOTS-1). Out-of-range pots,
// channels or codes are programming errors and trip an assert.
#pragma once
#include <stdint.h>

namespace mcp4351 {

constexpr uint8_t NUM_CHANNELS = 4;
constexpr int16_t CODE_MAX = 256;  // W-B resistance: 0 = R_W (strongest coupling), 256 = 10 kΩ
constexpr int16_t DISCONNECTED = -1;

// Register addresses (Table 4-2)
enum Register : uint8_t {
  WIPER0 = 0x00,
  WIPER1 = 0x01,
  TCON0  = 0x04,  // channels 0-1
  WIPER2 = 0x06,
  WIPER3 = 0x07,
  TCON1  = 0x0A,  // channels 2-3
};

void writeRegister(uint8_t pot, Register reg, uint16_t data);

// Returns the 9-bit register value, or -1 if CMDERR is clear (invalid command or floating MISO).
// raw, if given, receives the full 16-bit response for diagnostics.
int16_t readRegister(uint8_t pot, Register reg, uint16_t *raw = nullptr);

int16_t readWiper(uint8_t pot, uint8_t channel, uint16_t *raw = nullptr);

// Writes / reads all four wipers of one pot in a single CS assertion (no TCON change).
// codes[ch] must be 0..CODE_MAX.
// readWipers stores -1 for any channel whose response has CMDERR clear.
void writeWipers(uint8_t pot, const int16_t codes[NUM_CHANNELS]);
void readWipers(uint8_t pot, int16_t codes[NUM_CHANNELS], uint16_t raw[NUM_CHANNELS] = nullptr);

// Sets all four channels of one pot in a single CS assertion (TCON0, TCON1, then wipers).
// values[ch]: DISCONNECTED opens W and B; otherwise 0..CODE_MAX with W and B connected.
void setChannels(uint8_t pot, const int16_t values[NUM_CHANNELS]);

}  // namespace mcp4351
