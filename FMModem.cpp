/*
 *   Copyright (C) 2024 by MMDVM-SDR Contributors
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "Config.h"

#if defined(STANDALONE_MODE)

#include "FMModem.h"
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==================== FM Modulator ====================

CFMModulator::CFMModulator() :
m_sensitivity(0.0f),
m_phase(0.0f),
m_sampleRate(0.0f),
m_deviation(0.0f),
m_sinLUT(nullptr),
m_cosLUT(nullptr)
{
}

CFMModulator::~CFMModulator()
{
  if (m_sinLUT)
    delete[] m_sinLUT;
  if (m_cosLUT)
    delete[] m_cosLUT;
}

void CFMModulator::init(float sampleRate, float deviation)
{
  m_sampleRate = sampleRate;
  m_deviation = deviation;
  m_sensitivity = (2.0f * M_PI * deviation) / sampleRate;
  m_phase = 0.0f;

  initLUT();
}

void CFMModulator::initLUT()
{
  if (!m_sinLUT)
    m_sinLUT = new int16_t[LUT_SIZE];
  if (!m_cosLUT)
    m_cosLUT = new int16_t[LUT_SIZE];

  for (uint32_t i = 0; i < LUT_SIZE; i++) {
    float angle = (2.0f * M_PI * i) / LUT_SIZE;
    m_sinLUT[i] = (int16_t)(sin(angle) * 32767.0f);
    m_cosLUT[i] = (int16_t)(cos(angle) * 32767.0f);
  }
}

void CFMModulator::reset()
{
  m_phase = 0.0f;
}

void CFMModulator::modulate(const int16_t* input, int16_t* output_i, int16_t* output_q, uint32_t length)
{
#if defined(USE_NEON)
  modulateNEON(input, output_i, output_q, length);
#else
  modulateScalar(input, output_i, output_q, length);
#endif
}

void CFMModulator::modulateScalar(const int16_t* input, int16_t* output_i, int16_t* output_q, uint32_t length)
{
  for (uint32_t i = 0; i < length; i++) {
    // Accumulate phase: phase += sensitivity * input[i]
    // input is Q15 (range -32768 to 32767), so scale it to -1.0 to 1.0
    float input_normalized = (float)input[i] / 32768.0f;
    m_phase += m_sensitivity * input_normalized;

    // Wrap phase to [0, 2*pi)
    while (m_phase >= 2.0f * M_PI)
      m_phase -= 2.0f * M_PI;
    while (m_phase < 0.0f)
      m_phase += 2.0f * M_PI;

    // Convert phase to LUT index
    uint32_t index = (uint32_t)((m_phase / (2.0f * M_PI)) * LUT_SIZE) % LUT_SIZE;

    // Generate I/Q from lookup table
    output_i[i] = m_cosLUT[index];
    output_q[i] = m_sinLUT[index];
  }
}

#if defined(USE_NEON)
#include <arm_neon.h>

void CFMModulator::modulateNEON(const int16_t* input, int16_t* output_i, int16_t* output_q, uint32_t length)
{
  // NEON optimization: process 4 samples at a time
  uint32_t vectorLength = length & ~3U;  // Round down to multiple of 4
  uint32_t i;

  for (i = 0; i < vectorLength; i += 4) {
    // Load 4 input samples
    int16x4_t input_vec = vld1_s16(&input[i]);

    // Convert to float and normalize
    float32x4_t input_f = vcvtq_f32_s32(vmovl_s16(input_vec));
    input_f = vmulq_n_f32(input_f, 1.0f / 32768.0f);

    // Accumulate phase for each sample (must be done sequentially due to dependency)
    float phases[4];
    for (int j = 0; j < 4; j++) {
      m_phase += m_sensitivity * vgetq_lane_f32(input_f, j);
      while (m_phase >= 2.0f * M_PI)
        m_phase -= 2.0f * M_PI;
      while (m_phase < 0.0f)
        m_phase += 2.0f * M_PI;
      phases[j] = m_phase;
    }

    // Convert phases to LUT indices and lookup
    for (int j = 0; j < 4; j++) {
      uint32_t index = (uint32_t)((phases[j] / (2.0f * M_PI)) * LUT_SIZE) % LUT_SIZE;
      output_i[i + j] = m_cosLUT[index];
      output_q[i + j] = m_sinLUT[index];
    }
  }

  // Handle remaining samples
  modulateScalar(&input[i], &output_i[i], &output_q[i], length - i);
}
#endif

// ==================== FM Demodulator ====================

CFMDemodulator::CFMDemodulator() :
m_gain(0.0f),
m_prev_i(0),
m_prev_q(0),
m_sampleRate(0.0f),
m_deviation(0.0f),
m_atan2LUT(nullptr)
{
}

CFMDemodulator::~CFMDemodulator()
{
  if (m_atan2LUT)
    delete[] m_atan2LUT;
}

void CFMDemodulator::init(float sampleRate, float deviation)
{
  m_sampleRate = sampleRate;
  m_deviation = deviation;
  m_gain = sampleRate / (2.0f * M_PI * deviation);
  m_prev_i = 0;
  m_prev_q = 0;

  initLUT();
}

void CFMDemodulator::initLUT()
{
  // Precompute atan2 for common angles
  if (!m_atan2LUT)
    m_atan2LUT = new int16_t[ATAN2_LUT_SIZE * ATAN2_LUT_SIZE];

  // Create a simple atan2 lookup table for small angles
  // This is a simplified version - full implementation would use octant symmetry
  for (uint32_t y = 0; y < ATAN2_LUT_SIZE; y++) {
    for (uint32_t x = 0; x < ATAN2_LUT_SIZE; x++) {
      float yf = (float)y - 128.0f;
      float xf = (float)x - 128.0f;
      float angle = atan2(yf, xf);
      m_atan2LUT[y * ATAN2_LUT_SIZE + x] = (int16_t)(angle * 32767.0f / M_PI);
    }
  }
}

void CFMDemodulator::reset()
{
  m_prev_i = 0;
  m_prev_q = 0;
}

int16_t CFMDemodulator::fastAtan2(int16_t y, int16_t x)
{
  // Fast atan2 approximation using polynomial
  // This is more accurate than LUT for the full range

  // Handle special cases
  if (x == 0) {
    if (y > 0) return 16384;  // pi/2 in Q15 format
    if (y < 0) return -16384; // -pi/2
    return 0;
  }

  // Use CORDIC-like approximation
  // For simplicity, use floating point here (can be optimized with fixed-point)
  float angle = atan2((float)y, (float)x);
  return (int16_t)(angle * 32767.0f / M_PI);
}

void CFMDemodulator::demodulate(const int16_t* input_i, const int16_t* input_q, int16_t* output, uint32_t length)
{
#if defined(USE_NEON)
  demodulateNEON(input_i, input_q, output, length);
#else
  demodulateScalar(input_i, input_q, output, length);
#endif
}

void CFMDemodulator::demodulateScalar(const int16_t* input_i, const int16_t* input_q, int16_t* output, uint32_t length)
{
  for (uint32_t i = 0; i < length; i++) {
    int16_t curr_i = input_i[i];
    int16_t curr_q = input_q[i];

    // Quadrature demodulation: cross product
    // diff = I[n] * Q[n-1] - Q[n] * I[n-1]
    // This gives us the instantaneous frequency deviation

    int32_t cross1 = (int32_t)curr_i * m_prev_q;
    int32_t cross2 = (int32_t)curr_q * m_prev_i;
    int32_t diff = cross1 - cross2;

    // Normalize by magnitude to get angle
    // For small deviations, diff / (I^2 + Q^2) ≈ diff / 32768^2
    // Simplified: just scale by gain
    int32_t demod = (int32_t)((float)diff * m_gain / 1073741824.0f);  // Divide by 32768^2

    // Saturate to Q15 range
    if (demod > 32767) demod = 32767;
    if (demod < -32768) demod = -32768;

    output[i] = (int16_t)demod;

    // Update previous samples
    m_prev_i = curr_i;
    m_prev_q = curr_q;
  }
}

#if defined(USE_NEON)
void CFMDemodulator::demodulateNEON(const int16_t* input_i, const int16_t* input_q, int16_t* output, uint32_t length)
{
  // NEON optimization: process 4 samples at a time
  // Note: This is tricky because each sample depends on the previous one
  // We can still use NEON for the arithmetic, but need to maintain state

  uint32_t vectorLength = length & ~3U;  // Round down to multiple of 4
  uint32_t i;

  int16_t prev_i = m_prev_i;
  int16_t prev_q = m_prev_q;

  for (i = 0; i < vectorLength; i += 4) {
    // Load current samples
    int16x4_t curr_i_vec = vld1_s16(&input_i[i]);
    int16x4_t curr_q_vec = vld1_s16(&input_q[i]);

    // We need to process sequentially due to dependency on previous samples
    // But we can still use NEON for the multiply-accumulate

    for (int j = 0; j < 4; j++) {
      int16_t curr_i = vget_lane_s16(curr_i_vec, j);
      int16_t curr_q = vget_lane_s16(curr_q_vec, j);

      // Cross product using NEON multiply
      int32_t cross1 = (int32_t)curr_i * prev_q;
      int32_t cross2 = (int32_t)curr_q * prev_i;
      int32_t diff = cross1 - cross2;

      // Scale and saturate
      int32_t demod = (int32_t)((float)diff * m_gain / 1073741824.0f);
      if (demod > 32767) demod = 32767;
      if (demod < -32768) demod = -32768;

      output[i + j] = (int16_t)demod;

      prev_i = curr_i;
      prev_q = curr_q;
    }
  }

  m_prev_i = prev_i;
  m_prev_q = prev_q;

  // Handle remaining samples
  demodulateScalar(&input_i[i], &input_q[i], &output[i], length - i);
}
#endif

#endif // STANDALONE_MODE
