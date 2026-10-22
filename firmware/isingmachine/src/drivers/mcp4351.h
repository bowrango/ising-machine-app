// MCP4351-103 quad-channel 10 kΩ digital potentiometer (U16-U22).
// Datasheet: https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/22242A.pdf
//
// 16-bit message (Section 7): [15:12] register | [11:10] command | [9:0] data
// Terminal Control (TCON) handles disconnect. The wiper and TCONs occupy bits [8:0].
// On writes, bit 9 is unused.
// On reads, bit 9 is CMDERR (1 = valid), SDO returns 1s in bits [15:10] and the register in bits [8:0].

#pragma once
#include <stdint.h>

namespace mcp4351 {

constexpr uint8_t NUM_CHANNELS = 4;  // 4 wipers
constexpr uint8_t NUM_WORDS = 6;     // TCON0 + TCON1 + 4 wipers
constexpr int16_t CODE_MAX = 256;    // code for max resistance 10,000 ohms
constexpr int16_t DISCONNECTED = -1; // code for open channel
constexpr int16_t READ_ERROR = -2;   // code for invalid response

// R_AB = 10 kΩ ±20%, R_W ≈ 75 Ω typical (datasheet at 5 V; higher at 3.3 V).
constexpr float R_AB_OHMS = 10000.0f;
constexpr float R_W_OHMS = 75.0f;

// Read all four wiper codes for a single potentiometer pot
// (optional) debug gets the entire 16-bit responses
void readWipers(uint8_t pot, int16_t codes[NUM_CHANNELS], uint16_t debug[NUM_WORDS] = nullptr);

// Write all four wiper codes for a single potentiometer pot
void writeWipers(uint8_t pot, const int16_t codes[NUM_CHANNELS]);

}  // namespace mcp4351
