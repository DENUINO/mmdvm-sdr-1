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

#include "Resampler.h"
#include <cstring>
#include <cstdlib>

#if defined(USE_NEON)
#include <arm_neon.h>
#endif

// ==================== CRationalResampler ====================

CRationalResampler::CRationalResampler() :
m_interp(1),
m_decim(1),
m_taps(nullptr),
m_tapLen(0),
m_phaseLen(0),
m_state(nullptr),
m_stateLen(0),
m_phase(0),
m_offset(0)
{
}

CRationalResampler::~CRationalResampler()
{
  if (m_taps)
    delete[] m_taps;
  if (m_state)
    delete[] m_state;
}

bool CRationalResampler::init(uint32_t interp, uint32_t decim, const int16_t* taps, uint32_t tapLen)
{
  if (interp == 0 || decim == 0 || taps == nullptr || tapLen == 0)
    return false;

  m_interp = interp;
  m_decim = decim;
  m_tapLen = tapLen;

  // For polyphase implementation, taps should be divisible by interp
  m_phaseLen = (tapLen + interp - 1) / interp;

  // Allocate and copy filter taps
  if (m_taps)
    delete[] m_taps;
  m_taps = new int16_t[tapLen];
  memcpy(m_taps, taps, tapLen * sizeof(int16_t));

  // Allocate state buffer (needs to hold phaseLen - 1 samples)
  m_stateLen = m_phaseLen;
  if (m_state)
    delete[] m_state;
  m_state = new int16_t[m_stateLen];
  memset(m_state, 0, m_stateLen * sizeof(int16_t));

  m_phase = 0;
  m_offset = 0;

  return true;
}

uint32_t CRationalResampler::getOutputLength(uint32_t inputLen) const
{
  // Output length = floor((inputLen * interp + phase) / decim)
  // This is an approximation; actual length may vary slightly
  return (inputLen * m_interp + m_decim - 1) / m_decim;
}

void CRationalResampler::reset()
{
  if (m_state)
    memset(m_state, 0, m_stateLen * sizeof(int16_t));
  m_phase = 0;
  m_offset = 0;
}

bool CRationalResampler::resample(const int16_t* input, uint32_t inputLen,
                                   int16_t* output, uint32_t maxOutputLen, uint32_t* outputLen)
{
  if (!m_taps || !m_state || !input || !output || !outputLen)
    return false;

  uint32_t outIdx = 0;
  uint32_t inIdx = 0;

  while (inIdx < inputLen && outIdx < maxOutputLen) {
    // Update state with new input sample
    if (m_phase == 0) {
      // Shift state buffer
      for (uint32_t i = m_stateLen - 1; i > 0; i--) {
        m_state[i] = m_state[i - 1];
      }
      m_state[0] = input[inIdx];
      inIdx++;
    }

    // Process polyphase filter for current phase
    int16_t outSample;
    processPolyphase(m_state, outSample, m_phase);
    output[outIdx++] = outSample;

    // Advance phase
    m_phase += m_decim;
    while (m_phase >= m_interp) {
      m_phase -= m_interp;

      // Need new input sample
      if (inIdx >= inputLen)
        break;

      // Shift state buffer
      for (uint32_t i = m_stateLen - 1; i > 0; i--) {
        m_state[i] = m_state[i - 1];
      }
      m_state[0] = input[inIdx];
      inIdx++;
    }
  }

  *outputLen = outIdx;
  return true;
}

void CRationalResampler::processPolyphase(const int16_t* input, int16_t& output, uint32_t phase)
{
#if defined(USE_NEON)
  processPolyphaseNEON(input, output, phase);
#else
  // Scalar polyphase FIR implementation
  int32_t acc = 0;

  // For polyphase filtering, we use every M-th tap starting from phase
  for (uint32_t i = 0; i < m_phaseLen && (phase + i * m_interp) < m_tapLen; i++) {
    int32_t tapIdx = phase + i * m_interp;
    acc += (int32_t)input[i] * m_taps[tapIdx];
  }

  // Convert from Q30 to Q15 (with saturation)
  acc >>= 15;
  if (acc > 32767) acc = 32767;
  if (acc < -32768) acc = -32768;

  output = (int16_t)acc;
#endif
}

#if defined(USE_NEON)
void CRationalResampler::processPolyphaseNEON(const int16_t* input, int16_t& output, uint32_t phase)
{
  // NEON-optimized polyphase FIR
  // Process 4 taps at a time

  int32x4_t acc_vec = vdupq_n_s32(0);
  uint32_t i = 0;

  // Process in blocks of 4
  for (; i + 3 < m_phaseLen; i += 4) {
    // Check if all tap indices are valid
    uint32_t tap0 = phase + (i + 0) * m_interp;
    uint32_t tap1 = phase + (i + 1) * m_interp;
    uint32_t tap2 = phase + (i + 2) * m_interp;
    uint32_t tap3 = phase + (i + 3) * m_interp;

    if (tap3 >= m_tapLen)
      break;

    // Load 4 input samples
    int16_t in_vals[4] = {input[i], input[i+1], input[i+2], input[i+3]};
    int16x4_t in_vec = vld1_s16(in_vals);

    // Load 4 taps
    int16_t tap_vals[4] = {m_taps[tap0], m_taps[tap1], m_taps[tap2], m_taps[tap3]};
    int16x4_t tap_vec = vld1_s16(tap_vals);

    // Multiply-accumulate (widens to 32-bit)
    acc_vec = vmlal_s16(acc_vec, in_vec, tap_vec);
  }

  // Horizontal add
  int32x2_t sum_pair = vadd_s32(vget_low_s32(acc_vec), vget_high_s32(acc_vec));
  int32_t acc = vget_lane_s32(vpadd_s32(sum_pair, sum_pair), 0);

  // Process remaining taps
  for (; i < m_phaseLen; i++) {
    uint32_t tapIdx = phase + i * m_interp;
    if (tapIdx >= m_tapLen)
      break;
    acc += (int32_t)input[i] * m_taps[tapIdx];
  }

  // Convert from Q30 to Q15 with saturation
  acc >>= 15;
  if (acc > 32767) acc = 32767;
  if (acc < -32768) acc = -32768;

  output = (int16_t)acc;
}
#endif

// ==================== CDecimatingResampler ====================

CDecimatingResampler::CDecimatingResampler()
{
}

CDecimatingResampler::~CDecimatingResampler()
{
}

bool CDecimatingResampler::initDecimator(uint32_t decim, const int16_t* taps, uint32_t tapLen)
{
  return init(1, decim, taps, tapLen);
}

bool CDecimatingResampler::decimate(const int16_t* input, uint32_t inputLen,
                                     int16_t* output, uint32_t maxOutputLen, uint32_t* outputLen)
{
  return resample(input, inputLen, output, maxOutputLen, outputLen);
}

// ==================== CInterpolatingResampler ====================

CInterpolatingResampler::CInterpolatingResampler()
{
}

CInterpolatingResampler::~CInterpolatingResampler()
{
}

bool CInterpolatingResampler::initInterpolator(uint32_t interp, const int16_t* taps, uint32_t tapLen)
{
  return init(interp, 1, taps, tapLen);
}

bool CInterpolatingResampler::interpolate(const int16_t* input, uint32_t inputLen,
                                           int16_t* output, uint32_t maxOutputLen, uint32_t* outputLen)
{
  return resample(input, inputLen, output, maxOutputLen, outputLen);
}

#endif // STANDALONE_MODE
