#include <Arduino.h>

#include "../board.h"
#include "../drivers/hcs165.h"
#include "../machine/couplers.h"
#include "tests.h"

namespace tests {

namespace {

// Each window must be short compared with the beat period between oscillators, or the phase
// averages out to 90 deg. Phase resolution per window is 180 / WINDOW_SAMPLES deg.
constexpr uint8_t NUM_WINDOWS = 40;
constexpr uint8_t WINDOW_SAMPLES = 32;

}  // namespace

// With every coupler disconnected, measures each oscillator's phase relative to A in back-to-back
// short windows. The fraction of snapshots in a window in which bit i differs from bit A
// gives |phi_i - phi_A| = 180 deg * mismatch, assuming equal frequencies and 50% duty over the
// window. A phase that sweeps between windows means the oscillators run at different frequencies.
// This is a measurement, not a PASS/FAIL: without SYNC, component variability sets the deviation.
// Leaves all couplers disconnected.
void oscillatorCalibrate() {
  Serial.println("=== oscillatorCalibrate ===");
  if (const uint8_t failed = couplers::disconnectAll()) {
    for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
      if (failed & (1 << pot)) Serial.printf("  %s: W and B not open after disconnect\n", board::DIGIPOT_LABEL[pot]);
    }
    Serial.println("=== end oscillatorCalibrate: aborted, couplers did not disconnect ===");
    return;
  }

  // Acquire first, print afterwards, so the windows are contiguous in time.
  uint8_t mismatch[NUM_WINDOWS][board::NUM_SPINS] = {};
  uint32_t startUs[NUM_WINDOWS], endUs = 0;
  uint32_t ones[board::NUM_SPINS] = {0};
  uint8_t first = 0, changed = 0;
  for (uint8_t w = 0; w < NUM_WINDOWS; w++) {
    startUs[w] = micros();
    for (uint8_t s = 0; s < WINDOW_SAMPLES; s++) {
      const uint8_t raw = hcs165::read();
      if (w == 0 && s == 0) first = raw;
      changed |= raw ^ first;
      const uint8_t diff = raw ^ ((raw & 1) ? 0xFF : 0x00);  // bit i set if i differs from A
      for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
        mismatch[w][i] += (diff >> i) & 1;
        ones[i] += (raw >> i) & 1;
      }
    }
  }
  endUs = micros();

  if (!changed) {
    Serial.println("=== end oscillatorCalibrate: no data, all snapshots identical (oscillator board off?) ===");
    return;
  }

  const float windowUs = float(endUs - startUs[0]) / NUM_WINDOWS;
  Serial.printf("  all %d couplers disconnected; %d windows x %d snapshots, %.0f us per window\n",
                board::NUM_COUPLERS, NUM_WINDOWS, WINDOW_SAMPLES, windowUs);
  Serial.print("  window  t (us)  phase vs A (deg):");
  for (uint8_t i = 1; i < board::NUM_SPINS; i++) Serial.printf("     %c", board::SPIN_LABEL[i]);
  Serial.println();

  float phase[NUM_WINDOWS][board::NUM_SPINS];
  for (uint8_t w = 0; w < NUM_WINDOWS; w++) {
    Serial.printf("  %6d  %6lu                   ", w, (unsigned long)(startUs[w] - startUs[0]));
    for (uint8_t i = 1; i < board::NUM_SPINS; i++) {
      phase[w][i] = 180.0f * mismatch[w][i] / WINDOW_SAMPLES;
      Serial.printf("  %4.0f", phase[w][i]);
    }
    Serial.println();
  }

  // Per-oscillator summary: duty cycle, and the phase deviation from A across windows.
  Serial.println("  osc  duty%   mean  min  max (deg from A)");
  for (uint8_t i = 0; i < board::NUM_SPINS; i++) {
    const float duty = 100.0f * ones[i] / (uint32_t(NUM_WINDOWS) * WINDOW_SAMPLES);
    if (i == 0) {
      Serial.printf("  %c    %5.1f   (reference)\n", board::SPIN_LABEL[i], duty);
      continue;
    }
    float sum = 0, lo = 180, hi = 0;
    for (uint8_t w = 0; w < NUM_WINDOWS; w++) {
      sum += phase[w][i];
      lo = min(lo, phase[w][i]);
      hi = max(hi, phase[w][i]);
    }
    Serial.printf("  %c    %5.1f  %5.0f %4.0f %4.0f\n", board::SPIN_LABEL[i], duty, sum / NUM_WINDOWS, lo, hi);
  }
  Serial.println("  (0 deg = in phase with A; 90 deg = uncorrelated; 180 deg = anti-phase)");
  Serial.println("=== end oscillatorCalibrate ===");
}

}  // namespace tests
