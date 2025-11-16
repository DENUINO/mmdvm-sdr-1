/*
 *   Unit tests for NEON-optimized math functions
 */

#include "../arm_math_neon.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cmath>

void test_q15_to_q31() {
  printf("Testing arm_q15_to_q31_neon...\n");

  q15_t input[16] = {1000, -2000, 3000, -4000, 5000, -6000, 7000, -8000,
                     9000, -10000, 11000, -12000, 13000, -14000, 15000, -16000};
  q31_t output[16];

  arm_q15_to_q31_neon(input, output, 16);

  // Check conversion
  for (int i = 0; i < 16; i++) {
    q31_t expected = (q31_t)input[i] << 16;
    assert(output[i] == expected);
  }

  printf("  Q15 to Q31 conversion test PASSED\n");
}

void test_correlation() {
  printf("Testing arm_correlate_ssd_neon...\n");

  q15_t seq1[16] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000,
                    9000, 10000, 11000, 12000, 13000, 14000, 15000, 16000};
  q15_t seq2[16] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000,
                    9000, 10000, 11000, 12000, 13000, 14000, 15000, 16000};

  // Same sequence should give zero correlation
  uint32_t result = arm_correlate_ssd_neon(seq1, seq2, 16);
  printf("  Correlation (same): %u (should be 0)\n", result);
  assert(result == 0);

  // Different sequence
  q15_t seq3[16] = {2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
                    10000, 11000, 12000, 13000, 14000, 15000, 16000, 17000};

  result = arm_correlate_ssd_neon(seq1, seq3, 16);
  printf("  Correlation (different): %u\n", result);
  assert(result > 0);

  printf("  Correlation test PASSED\n");
}

void test_dot_product() {
  printf("Testing arm_dot_prod_q15_neon...\n");

  q15_t vec1[16] = {100, 200, 300, 400, 500, 600, 700, 800,
                    900, 1000, 1100, 1200, 1300, 1400, 1500, 1600};
  q15_t vec2[16] = {10, 20, 30, 40, 50, 60, 70, 80,
                    90, 100, 110, 120, 130, 140, 150, 160};

  int32_t result = arm_dot_prod_q15_neon(vec1, vec2, 16);

  // Calculate expected (scalar)
  int32_t expected = 0;
  for (int i = 0; i < 16; i++) {
    expected += (int32_t)vec1[i] * vec2[i];
  }

  printf("  Dot product: %d (expected: %d)\n", result, expected);
  assert(result == expected);

  printf("  Dot product test PASSED\n");
}

void test_vector_ops() {
  printf("Testing NEON vector operations...\n");

  q15_t a[8] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000};
  q15_t b[8] = {500, 600, 700, 800, 900, 1000, 1100, 1200};
  q15_t out[8];

  // Test addition
  arm_add_q15_neon(a, b, out, 8);
  printf("  Addition: a[0]+b[0]=%d, out[0]=%d\n", a[0]+b[0], out[0]);
  assert(out[0] == a[0] + b[0]);

  // Test subtraction
  arm_sub_q15_neon(a, b, out, 8);
  printf("  Subtraction: a[0]-b[0]=%d, out[0]=%d\n", a[0]-b[0], out[0]);
  assert(out[0] == a[0] - b[0]);

  // Test scaling
  arm_scale_q15_neon(a, 16384, out, 8);  // Scale by 0.5 in Q15
  printf("  Scaling: a[0]*0.5=%d, out[0]=%d\n", a[0]/2, out[0]);
  assert(abs(out[0] - a[0]/2) < 2);  // Allow small rounding error

  printf("  Vector operations test PASSED\n");
}

int main() {
  printf("==== NEON Math Unit Tests ====\n\n");

  test_q15_to_q31();
  test_correlation();
  test_dot_product();
  test_vector_ops();

  printf("\n==== All tests PASSED ====\n");
  return 0;
}
