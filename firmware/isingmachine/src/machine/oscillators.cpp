#include "oscillators.h"

#include <string.h>

#include "../drivers/hcs165.h"

namespace oscillators {

void sample(uint32_t shots, uint32_t counts[board::NUM_STATES]) {
  memset(counts, 0, sizeof(uint32_t) * board::NUM_STATES);
  for (uint32_t s = 0; s < shots; s++) counts[hcs165::read()]++;
}

}  // namespace oscillators
