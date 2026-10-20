#include <Arduino.h>
#include <math.h>

#include "../board.h"
#include "../drivers/mcp4351.h"
#include "tests.h"

namespace tests {

namespace {

constexpr uint8_t NUM_NODES = 2 * board::NUM_SPINS;  // 0..7 = A_L..H_L, 8..15 = A_R..H_R
constexpr float TEST_CURRENT_A = 1e-3f;  // ohmmeter-like; R = V / I doesn't depend on it

uint8_t nodeL(uint8_t s) { return s; }
uint8_t nodeR(uint8_t s) { return board::NUM_SPINS + s; }

// Resistance between x_L and x_R when every coupler is r_ohms: ground x_R, inject TEST_CURRENT_A at x_L,
// and solve the nodal equations G v = i for v(x_L).
float solveNodeResistance(uint8_t x, float r_ohms) {
  
  // Build conductance matrix 
  float G[NUM_NODES][NUM_NODES] = {};
  for (uint8_t k = 0; k < board::NUM_COUPLERS; k++) {
    const auto [i, j] = board::COUPLER_PAIRS[k];
    const bool left = board::couplerJoinsLeftOfI(i, j);
    const uint8_t a = left ? nodeL(i) : nodeR(i);
    const uint8_t b = left ? nodeR(j) : nodeL(j);
    G[a][a] += 1 / r_ohms;
    G[b][b] += 1 / r_ohms;
    G[a][b] -= 1 / r_ohms;
    G[b][a] -= 1 / r_ohms;
  }

  // Setup reduced system without the grounded node, augmented with the current vector.
  constexpr uint8_t N = NUM_NODES - 1;
  const uint8_t ground = nodeR(x);
  float m[N][N + 1];
  for (uint8_t r = 0, rr = 0; r < NUM_NODES; r++) {
    if (r == ground) continue;
    for (uint8_t c = 0, cc = 0; c < NUM_NODES; c++) {
      if (c != ground) m[rr][cc++] = G[r][c];
    }
    m[rr][N] = r == nodeL(x) ? TEST_CURRENT_A : 0;
    rr++;
  }

  // Solve via Gaussian elimination with partial pivoting
  for (uint8_t col = 0; col < N; col++) {
    uint8_t pivot = col;
    for (uint8_t r = col + 1; r < N; r++) {
      if (fabsf(m[r][col]) > fabsf(m[pivot][col])) pivot = r;
    }
    for (uint8_t c = 0; c <= N; c++) {
      const float t = m[col][c];
      m[col][c] = m[pivot][c];
      m[pivot][c] = t;
    }
    for (uint8_t r = col + 1; r < N; r++) {
      const float f = m[r][col] / m[col][col];
      for (uint8_t c = col; c <= N; c++) m[r][c] -= f * m[col][c];
    }
  }

  // Compute resistance from voltage
  float v[N];
  for (int8_t r = N - 1; r >= 0; r--) {
    float s = m[r][N];
    for (uint8_t c = r + 1; c < N; c++) s -= m[r][c] * v[c];
    v[r] = s / m[r][r];
  }
  return v[nodeL(x)] / TEST_CURRENT_A;  // x_L sits below the grounded x_R, so its reduced index is unchanged
}

}  // namespace

// Set every coupler to code and print the expected resistance at each
// L and R oscillator header pin (oscillator board unplugged, coupler board powered).
void setCouplers(const char *arg) {
  const int16_t code = atoi(arg);
  if (code < 0 || code > mcp4351::CODE_MAX) {
    Serial.printf("Usage: setCouplers <0..%d>\n", mcp4351::CODE_MAX);
    return;
  }

  Serial.println("=== Begin setCouplers ===");
  const int16_t codes[mcp4351::NUM_CHANNELS] = {code, code, code, code};
  for (uint8_t pot = 0; pot < board::NUM_DIGIPOTS; pot++) {
    mcp4351::setChannels(pot, codes);
  }

  const float r = mcp4351::R_W_OHMS + code * mcp4351::R_AB_OHMS / mcp4351::CODE_MAX;
  Serial.printf("  code %d = nominal %.0f ohms per coupler\n", code, r);
  Serial.println("  osc  J1A(L)  J1B(R)  expected L-R");
  // ohm measurements for code 0:   A 123, B 180, C 100, D 165, E 165, F 129, G 138, H 95.
  // ohm measurements for code 256: A 7450, B 8490, C 7820, D 8030, E 8250, F 7840, G 8570, H 7510.
  for (uint8_t x = 0; x < board::NUM_SPINS; x++) {
    const float rx = solveNodeResistance(x, r);
    Serial.printf("  %c    %2u      %2u      %7.0f ohm  ", board::SPIN_LABEL[x],
                  board::J1A_PIN_L[x], board::J1B_PIN_R[x], rx);
  }
  Serial.println("=== End setCouplers ===");
}

}  // namespace tests
