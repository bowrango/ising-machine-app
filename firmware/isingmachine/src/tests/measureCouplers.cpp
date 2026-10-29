#include <Arduino.h>
#include <math.h>

#include "../board.h"
#include "../drivers/mcp4351.h"
#include "../machine/couplers.h"
#include "tests.h"

namespace tests {

namespace {

constexpr uint8_t NUM_NODES = 2*board::NUM_SPINS; // A_L...H_L (0...7) A_R...H_R (8...15)
constexpr float TEST_CURRENT_A = 0.001; // multimeter-scale
const uint8_t nodeL(uint8_t x) {return x;}
const uint8_t nodeR(uint8_t x) {return x+board::NUM_SPINS;}

// Solve Gv=I for resistance across oscillator terminals x_L x_R
// when entire resistance network is set to r_ohms 
float solveNodeResistance(uint8_t x, float r_ohms) {
  
  // Build conductance matrix G
  float G[NUM_NODES][NUM_NODES] = {};
  for (uint8_t k = 0; k < board::NUM_COUPLERS; k++) {
    const auto [i, j] = board::COUPLER_PAIRS[k]; // i < j
    // true if pair joins i_L and j_R else joins i_R and j_L 
    const bool left = (j - i) % 2; 
    // terminal pair (a,b) for oscillator pair (i,j)
    const uint8_t a = left ? nodeL(i) : nodeR(i);
    const uint8_t b = left ? nodeR(j) : nodeL(j);
    const float g = 1.0f / r_ohms;
    G[a][a] += g;
    G[b][b] += g;
    G[a][b] -= g;
    G[b][a] -= g;
  }

  // Set x_R as ground and setup augmented system M
  constexpr uint8_t N = NUM_NODES - 1;
  const uint8_t gnd = nodeR(x);
  float M[N][NUM_NODES]; // [reducedG I]
  for (uint8_t r = 0, rr = 0; r < NUM_NODES; r++) {
    if(r==gnd) {continue;}
    for (uint8_t c = 0, rc = 0; c < NUM_NODES; c++) {
      if (c!=gnd) {M[rr][rc++] = G[r][c];}
    }
    // Apply TEST_CURRENT at x_L
    M[rr++][N] = r == nodeL(x) ? TEST_CURRENT_A : 0.0f;
  }

  // Convert M to upper triangular form (pivoted Gaussian elimination)
  for (uint8_t col = 0; col < N; col++) {
    uint8_t pivot = col;
    for (uint8_t r = col + 1; r < N; r++) {
      if (fabsf(M[r][col]) > fabsf(M[pivot][col])) pivot = r;
    }
    for (uint8_t c = 0; c <= N; c++) {
      const float t = M[col][c];
      M[col][c] = M[pivot][c];
      M[pivot][c] = t;
    }
    for (uint8_t r = col + 1; r < N; r++) {
      const float f = M[r][col] / M[col][col];
      for (uint8_t c = col; c <= N; c++) M[r][c] -= f * M[col][c];
    }
  }

  // Get v with back-substitution
  float v[N];
  for (int8_t r = N - 1; r >= 0; r--) {
    float sum = M[r][N];
    for (uint8_t c = r + 1; c < N; c++) {sum -= M[r][c]*v[c];}
    v[r] = sum / M[r][r];
  }

  // Return resistance across x_L and x_R (assumed gnd)
  return v[nodeL(x)] / TEST_CURRENT_A;
}

}  // namespace

// Set every coupler to code and print the expected resistance at each
// L and R oscillator header pin (oscillator board unplugged, coupler board powered).
void measureCouplers(int16_t code) {
  Serial.println("=== Begin measureCouplers ===");
  couplers::setAll(code);

  const float r = mcp4351::R_W_OHMS + code * mcp4351::R_AB_OHMS / mcp4351::CODE_MAX;
  Serial.printf("  code %d = nominal %.0f ohms per coupler\n", code, r);
  Serial.println("  osc  J1A(L)  J1B(R)  expected L-R");
  for (uint8_t x = 0; x < board::NUM_SPINS; x++) {
    const float rx = solveNodeResistance(x, r);
    Serial.printf("  %c    %2u      %2u      %7.0f ohm\n", board::SPIN_LABEL[x],
                  board::J1A_PIN_L[x], board::J1B_PIN_R[x], rx);
  }
  Serial.println("=== End measureCouplers ===");
}

}  // namespace tests
