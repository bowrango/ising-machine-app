#include "couplers.h"

#include "../board.h"
#include "../drivers/mcp4351.h"

namespace couplers {

uint8_t disconnect() {
  const int16_t off[mcp4351::NUM_CHANNELS] = {mcp4351::DISCONNECTED, mcp4351::DISCONNECTED,
                                              mcp4351::DISCONNECTED, mcp4351::DISCONNECTED};
  uint8_t failed = 0;
  for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
    mcp4351::writeWipers(pot, off);
    int16_t codes[mcp4351::NUM_CHANNELS];
    mcp4351::readWipers(pot, codes);
    for (uint8_t ch = 0; ch < mcp4351::NUM_CHANNELS; ch++) {
      if (codes[ch] != mcp4351::DISCONNECTED) failed |= 1 << pot;
    }
  }
  return failed;
}

}  // namespace couplers
