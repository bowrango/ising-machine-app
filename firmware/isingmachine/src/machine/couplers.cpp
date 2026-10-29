#include "couplers.h"

#include "../board.h"
#include "../drivers/mcp4351.h"

namespace couplers {

bool verifyCode(long code) { return code >= 0 && code <= mcp4351::CODE_MAX; }

uint8_t setAll(int16_t code) {
  const int16_t codes[mcp4351::NUM_CHANNELS] = {code, code, code, code};
  uint8_t failed = 0;
  for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
    mcp4351::writeWipers(pot, codes);
    int16_t got[mcp4351::NUM_CHANNELS];
    mcp4351::readWipers(pot, got);
    for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
      if (got[ch] != code) failed |= 1 << pot;
    }
  }
  return failed;
}

uint8_t disconnect() { return setAll(mcp4351::DISCONNECTED); }

}  // namespace couplers
