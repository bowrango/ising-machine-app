#include "oscillators.h"

#include <string.h>

#include "../drivers/hcs165.h"

namespace oscillators {

void sample(uint32_t shots, uint32_t agree[board::NUM_SPINS][board::NUM_SPINS]) {
  memset(agree, 0, sizeof(uint32_t) * board::NUM_SPINS * board::NUM_SPINS);
  for (uint32_t s = 0; s < shots; s++) {
    const uint8_t raw = hcs165::read();
    for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
      const uint8_t ref = raw >> i & 1 ? 0xFF : 0x00;
      const uint8_t same = ~(raw ^ ref);  // bit j set where spin j matches spin i
      for (uint8_t j = 0; j < board::NUM_SPINS; j++) agree[i][j] += same >> j & 1;
    }
  }
}

}  // namespace oscillators
