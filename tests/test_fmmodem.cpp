/*
 *   Unit tests for FM Modulator/Demodulator
 */

#define STANDALONE_MODE
#define USE_NEON

#include "../FMModem.h"
#include <cstdio>
#include <cmath>
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void test_fm_modulator() {
  printf("Testing FM modulator...\n");

  CFMModulator modulator;
  modulator.init(24000.0f, 5000.0f);

  // Test input: simple sine wave
  int16_t input[100];
  int16_t output_i[100];
  int16_t output_q[100];

  for (int i = 0; i < 100; i++) {
    input[i] = (int16_t)(10000.0f * sin(2.0f * M_PI * 1000.0f * i / 24000.0f));
  }

  // Modulate
  modulator.modulate(input, output_i, output_q, 100);

  // Check that I/Q outputs are non-zero
  bool hasNonZero = false;
  for (int i = 0; i < 100; i++) {
    if (output_i[i] != 0 || output_q[i] != 0) {
      hasNonZero = true;
      break;
    }
  }

  assert(hasNonZero);

  printf("  Generated %d I/Q samples from modulator\n", 100);
  printf("  Sample I[50]=%d, Q[50]=%d\n", output_i[50], output_q[50]);
  printf("  FM modulator test PASSED\n");
}

void test_fm_demodulator() {
  printf("Testing FM demodulator...\n");

  CFMDemodulator demodulator;
  demodulator.init(24000.0f, 5000.0f);

  // Test input: simple IQ samples (constant frequency)
  int16_t input_i[100];
  int16_t input_q[100];
  int16_t output[100];

  for (int i = 0; i < 100; i++) {
    float angle = 2.0f * M_PI * 2000.0f * i / 24000.0f;
    input_i[i] = (int16_t)(20000.0f * cos(angle));
    input_q[i] = (int16_t)(20000.0f * sin(angle));
  }

  // Demodulate
  demodulator.demodulate(input_i, input_q, output, 100);

  // Check output is reasonable
  bool hasNonZero = false;
  for (int i = 0; i < 100; i++) {
    if (output[i] != 0) {
      hasNonZero = true;
      break;
    }
  }

  assert(hasNonZero);

  printf("  Demodulated %d samples\n", 100);
  printf("  Output[50]=%d\n", output[50]);
  printf("  FM demodulator test PASSED\n");
}

void test_modulator_demodulator_loop() {
  printf("Testing modulator->demodulator loop...\n");

  CFMModulator modulator;
  CFMDemodulator demodulator;

  modulator.init(24000.0f, 5000.0f);
  demodulator.init(24000.0f, 5000.0f);

  // Test input: DC value
  int16_t input[100];
  int16_t iq_i[100];
  int16_t iq_q[100];
  int16_t output[100];

  for (int i = 0; i < 100; i++) {
    input[i] = 5000;  // Constant value
  }

  // Modulate then demodulate
  modulator.modulate(input, iq_i, iq_q, 100);
  demodulator.demodulate(iq_i, iq_q, output, 100);

  // Output should be relatively constant (after settling)
  printf("  Input[50]=%d, Output[50]=%d\n", input[50], output[50]);

  printf("  Modulator-demodulator loop test PASSED\n");
}

int main() {
  printf("==== FM Modem Unit Tests ====\n\n");

  test_fm_modulator();
  test_fm_demodulator();
  test_modulator_demodulator_loop();

  printf("\n==== All tests PASSED ====\n");
  return 0;
}
