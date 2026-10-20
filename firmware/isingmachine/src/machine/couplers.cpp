#include "couplers.h"

#include <initializer_list>

#include "../board.h"
#include "../drivers/mcp4351.h"

namespace couplers {

namespace {
// TCON bits that connect W and B on each channel of a pair (R0W, R0B, R1W, R1B).
constexpr uint16_t TCON_WB_BITS = 0b000110011;
}  // namespace

uint8_t disconnect() {
  const int16_t off[mcp4351::NUM_CHANNELS] = {mcp4351::DISCONNECTED, mcp4351::DISCONNECTED,
                                              mcp4351::DISCONNECTED, mcp4351::DISCONNECTED};
  uint8_t failed = 0;
  for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
    mcp4351::setChannels(pot, off);
    for (mcp4351::Register reg : {mcp4351::TCON0, mcp4351::TCON1}) {
      const int16_t tcon = mcp4351::readRegister(pot, reg);
      if (tcon < 0 || (tcon & TCON_WB_BITS)) failed |= 1 << pot;
    }
  }
  return failed;
}

}  // namespace couplers
