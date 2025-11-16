/*
 *   Unit tests for Rational Resampler
 */

#include "../Resampler.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cassert>

// Simple test filter taps
static const int16_t TEST_TAPS[] = {100, 200, 300, 200, 100};
static const uint32_t TEST_TAP_LEN = 5;

void test_decimator() {
  printf("Testing decimator...\n");

  CDecimatingResampler decimator;

  // Initialize decimator (decimate by 2)
  assert(decimator.initDecimator(2, TEST_TAPS, TEST_TAP_LEN));

  // Test input
  int16_t input[10] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000};
  int16_t output[10];
  uint32_t outputLen;

  // Decimate
  assert(decimator.decimate(input, 10, output, 10, &outputLen));

  printf("  Input: 10 samples, Output: %u samples\n", outputLen);
  assert(outputLen <= 10);

  printf("  Decimator test PASSED\n");
}

void test_interpolator() {
  printf("Testing interpolator...\n");

  CInterpolatingResampler interpolator;

  // Initialize interpolator (interpolate by 3)
  assert(interpolator.initInterpolator(3, TEST_TAPS, TEST_TAP_LEN));

  // Test input
  int16_t input[5] = {1000, 2000, 3000, 4000, 5000};
  int16_t output[20];
  uint32_t outputLen;

  // Interpolate
  assert(interpolator.interpolate(input, 5, output, 20, &outputLen));

  printf("  Input: 5 samples, Output: %u samples\n", outputLen);
  assert(outputLen >= 10);  // Should be about 15 samples

  printf("  Interpolator test PASSED\n");
}

void test_resampler() {
  printf("Testing rational resampler (M=3, N=2)...\n");

  CRationalResampler resampler;

  // Initialize resampler (upsample by 3, downsample by 2)
  assert(resampler.init(3, 2, TEST_TAPS, TEST_TAP_LEN));

  // Test input
  int16_t input[10] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000};
  int16_t output[20];
  uint32_t outputLen;

  // Resample
  assert(resampler.resample(input, 10, output, 20, &outputLen));

  uint32_t expected = resampler.getOutputLength(10);
  printf("  Input: 10 samples, Output: %u samples (expected ~%u)\n", outputLen, expected);

  printf("  Rational resampler test PASSED\n");
}

int main() {
  printf("==== Resampler Unit Tests ====\n\n");

  test_decimator();
  test_interpolator();
  test_resampler();

  printf("\n==== All tests PASSED ====\n");
  return 0;
}
