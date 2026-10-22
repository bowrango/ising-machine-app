#include "mcp4351.h"

#include <Arduino.h>
#include <SPI.h>

#include "../board.h"
#include "bus.h"

namespace mcp4351 {

namespace {

// CS low to first SCK edge. Conservative margin: the datasheet minimum is sub-microsecond.
// Revisit together with the SPI clock.
constexpr uint32_t CS_SETUP_US = 5;

// Register addresses (Table 4-2)
enum Register : uint8_t {
  WIPER0 = 0x00,
  WIPER1 = 0x01,
  TCON0  = 0x04,  // channels 0-1
  WIPER2 = 0x06,
  WIPER3 = 0x07,
  TCON1  = 0x0A,  // channels 2-3
};

const Register WIPER[NUM_CHANNELS] = {WIPER0, WIPER1, WIPER2, WIPER3};

constexpr uint8_t CMD_WRITE = 0b00;
constexpr uint8_t CMD_READ  = 0b11;
constexpr uint16_t READ_DUMMY = 0x1FF; // unused read data bits

constexpr uint16_t CMDERR_MASK = 0x200; // D9
constexpr uint16_t DATA_MASK = 0x1FF;   // D8:D0

// TCON data per channel pair, D8 set. Each channel: HW=1, A open, W and B connected when enabled
// (the couplers are wired as W-B rheostats). Indexed by (ch1 enabled << 1) | ch0 enabled.
const uint16_t TCON_VALUE[4] = {
  0b110001000,  // neither
  0b110001011,  // lower channel only
  0b110111000,  // upper channel only
  0b110111011,  // both
};

// TCON W and B bits of the lower / upper channel in a pair (R0W R0B, R1W R1B).
constexpr uint16_t TCON_WB[2] = {0b000000011, 0b000110000};

uint16_t encode(Register reg, uint8_t cmd, uint16_t data) {
  // pack command: [15:12] register | [11:10] command | [9:0] data
  return (uint16_t(reg) << 12) | (uint16_t(cmd) << 10) | data;
}

void transferWords(uint8_t pot, const uint16_t words[NUM_WORDS], uint16_t responses[NUM_WORDS] = nullptr) {
  bus::writeAddress(pot);
  bus::beginTransaction(SPI_MODE0);
  bus::select();
  delayMicroseconds(CS_SETUP_US);
  for (uint8_t i = 0; i < NUM_WORDS; i++) {
    uint16_t r = bus::transfer16(words[i]);
    if (responses) responses[i] = r;
  }
  bus::deselect();
  bus::endTransaction();
}

}  // namespace

void readWipers(uint8_t pot, int16_t codes[NUM_CHANNELS], uint16_t debug[NUM_WORDS]) {
  // Verify
  assert(pot < board::NUM_DIGIPOTS);

  // Build
  uint16_t words[NUM_WORDS];
  words[0] = encode(TCON0, CMD_READ, READ_DUMMY);
  words[1] = encode(TCON1, CMD_READ, READ_DUMMY);
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    words[2 + ch] = encode(WIPER[ch], CMD_READ, READ_DUMMY);
  }

  // Send
  uint16_t responses[NUM_WORDS];
  transferWords(pot, words, responses);

  // Decode
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    const uint16_t tcon = responses[ch / 2];
    const uint16_t wiper = responses[2 + ch];
    const int16_t code = int16_t(wiper & DATA_MASK);
    // [9] CMDERR (1 = valid) | [8:0] data
    if ((wiper & CMDERR_MASK) && (tcon & CMDERR_MASK) && code <= CODE_MAX) {
      const bool connected = tcon & TCON_WB[ch % 2];
      codes[ch] = connected ? code : DISCONNECTED;
    } else {
      codes[ch] = READ_ERROR;
    }
  }
  if (debug) {
    for (uint8_t i = 0; i < NUM_WORDS; i++) {
      debug[i] = responses[i];
    }
  }
}

void writeWipers(uint8_t pot, const int16_t codes[NUM_CHANNELS]) {
  // Verify
  assert(pot < board::NUM_DIGIPOTS);
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    assert(codes[ch] == DISCONNECTED || (codes[ch] >= 0 && codes[ch] <= CODE_MAX));
  }

  // Build
  uint16_t words[NUM_WORDS];
  const uint8_t conn01 = (codes[0] >= 0) | ((codes[1] >= 0) << 1);
  const uint8_t conn23 = (codes[2] >= 0) | ((codes[3] >= 0) << 1);
  words[0] = encode(TCON0, CMD_WRITE, TCON_VALUE[conn01]);
  words[1] = encode(TCON1, CMD_WRITE, TCON_VALUE[conn23]);
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    // Disconnected channels are parked at full scale.
    const int16_t code = codes[ch] == DISCONNECTED ? CODE_MAX : codes[ch];
    words[2 + ch] = encode(WIPER[ch], CMD_WRITE, uint16_t(code));
  }

  // Send
  transferWords(pot, words);
}

}  // namespace mcp4351
