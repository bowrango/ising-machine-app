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

constexpr uint8_t CMD_WRITE = 0b00;
constexpr uint8_t CMD_READ  = 0b11;
constexpr uint16_t CMDERR_OK = 0x200;

const Register WIPER[NUM_CHANNELS] = {WIPER0, WIPER1, WIPER2, WIPER3};

// TCON data per channel pair, D8 set. Each channel: HW=1, A open, W and B connected when enabled
// (the couplers are wired as W-B rheostats). Indexed by (ch1 enabled << 1) | ch0 enabled.
const uint16_t TCON_MASK[4] = {
  0b110001000,  // neither
  0b110001011,  // lower channel only
  0b110111000,  // upper channel only
  0b110111011,  // both
};

constexpr uint16_t DATA_MASK = 0x1FF;  // D8:D0
constexpr uint16_t READ_DUMMY = 0x1FF;

uint16_t command(Register reg, uint8_t cmd, uint16_t data) {
  assert(data <= DATA_MASK);
  return (uint16_t(reg) << 12) | (uint16_t(cmd) << 10) | data;
}

uint16_t wiperWrite(uint8_t ch, int16_t code) {
  assert(ch < NUM_CHANNELS);
  assert(code >= 0 && code <= CODE_MAX);
  return command(WIPER[ch], CMD_WRITE, uint16_t(code));
}

int16_t decode(uint16_t response) {
  return (response & CMDERR_OK) ? int16_t(response & DATA_MASK) : int16_t(-1);
}

void transferWords(uint8_t pot, const uint16_t *words, uint8_t count, uint16_t *responses = nullptr) {
  assert(pot < board::NUM_DIGIPOTS);  // address 7 is the shift register
  bus::writeAddress(pot);
  bus::beginTransaction(SPI_MODE0);
  bus::select();
  delayMicroseconds(CS_SETUP_US);
  for (uint8_t i = 0; i < count; i++) {
    uint16_t r = bus::transfer16(words[i]);
    if (responses) responses[i] = r;
  }
  bus::deselect();
  bus::endTransaction();
}

}  // namespace

void writeRegister(uint8_t pot, Register reg, uint16_t data) {
  const uint16_t word = command(reg, CMD_WRITE, data);
  transferWords(pot, &word, 1);
}

int16_t readRegister(uint8_t pot, Register reg, uint16_t *raw) {
  const uint16_t word = command(reg, CMD_READ, READ_DUMMY);
  uint16_t response;
  transferWords(pot, &word, 1, &response);
  if (raw) *raw = response;
  return decode(response);
}

int16_t readWiper(uint8_t pot, uint8_t channel, uint16_t *raw) {
  assert(channel < NUM_CHANNELS);
  return readRegister(pot, WIPER[channel], raw);
}

void writeWipers(uint8_t pot, const int16_t codes[NUM_CHANNELS]) {
  uint16_t words[NUM_CHANNELS];
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) words[ch] = wiperWrite(ch, codes[ch]);
  transferWords(pot, words, NUM_CHANNELS);
}

void readWipers(uint8_t pot, int16_t codes[NUM_CHANNELS], uint16_t raw[NUM_CHANNELS]) {
  uint16_t words[NUM_CHANNELS], responses[NUM_CHANNELS];
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) words[ch] = command(WIPER[ch], CMD_READ, READ_DUMMY);
  transferWords(pot, words, NUM_CHANNELS, responses);
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    codes[ch] = decode(responses[ch]);
    if (raw) raw[ch] = responses[ch];
  }
}

void setChannels(uint8_t pot, const int16_t values[NUM_CHANNELS]) {
  const uint8_t conn01 = (values[0] >= 0) | ((values[1] >= 0) << 1);
  const uint8_t conn23 = (values[2] >= 0) | ((values[3] >= 0) << 1);

  uint16_t words[2 + NUM_CHANNELS];
  words[0] = command(TCON0, CMD_WRITE, TCON_MASK[conn01]);
  words[1] = command(TCON1, CMD_WRITE, TCON_MASK[conn23]);
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    // Disconnected channels are parked at full scale.
    words[2 + ch] = wiperWrite(ch, values[ch] == DISCONNECTED ? CODE_MAX : values[ch]);
  }
  transferWords(pot, words, 2 + NUM_CHANNELS);
}

}  // namespace mcp4351
