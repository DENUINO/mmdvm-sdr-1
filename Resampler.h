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

#if !defined(RESAMPLER_H)
#define RESAMPLER_H

#if defined(STANDALONE_MODE)

#include <stdint.h>

/**
 * Rational Resampler - converts sample rate by rational factor M/N
 * Uses polyphase FIR filter structure for efficiency
 * Optimized with NEON intrinsics when available
 */
class CRationalResampler {
public:
  CRationalResampler();
  ~CRationalResampler();

  /**
   * Initialize the resampler
   * @param interp Interpolation factor (M)
   * @param decim Decimation factor (N)
   * @param taps Filter coefficients (Q15 format)
   * @param tapLen Number of filter taps
   * @return true on success
   */
  bool init(uint32_t interp, uint32_t decim, const int16_t* taps, uint32_t tapLen);

  /**
   * Resample input samples
   * @param input Input samples (Q15 format)
   * @param inputLen Number of input samples
   * @param output Output samples (Q15 format)
   * @param maxOutputLen Maximum output buffer size
   * @param outputLen Actual number of output samples produced
   * @return true on success
   */
  bool resample(const int16_t* input, uint32_t inputLen,
                int16_t* output, uint32_t maxOutputLen, uint32_t* outputLen);

  /**
   * Get output length for given input length
   * @param inputLen Input sample count
   * @return Expected output sample count
   */
  uint32_t getOutputLength(uint32_t inputLen) const;

  /**
   * Reset the resampler state
   */
  void reset();

private:
  uint32_t m_interp;       // Interpolation factor (M)
  uint32_t m_decim;        // Decimation factor (N)
  int16_t* m_taps;         // Filter coefficients
  uint32_t m_tapLen;       // Total number of taps
  uint32_t m_phaseLen;     // Taps per polyphase filter (tapLen / interp)
  int16_t* m_state;        // State buffer
  uint32_t m_stateLen;     // State buffer length
  uint32_t m_phase;        // Current phase counter
  uint32_t m_offset;       // Current sample offset

  void processPolyphase(const int16_t* input, int16_t& output, uint32_t phase);

#if defined(USE_NEON)
  void processPolyphaseNEON(const int16_t* input, int16_t& output, uint32_t phase);
#endif
};

/**
 * Decimating Resampler - specialized for downsampling (M < N)
 * Optimized for 1MHz → 24kHz conversion (M=3, N=125)
 */
class CDecimatingResampler : public CRationalResampler {
public:
  CDecimatingResampler();
  ~CDecimatingResampler();

  /**
   * Initialize for decimation
   * @param decim Decimation factor
   * @param taps Low-pass filter coefficients
   * @param tapLen Number of taps
   * @return true on success
   */
  bool initDecimator(uint32_t decim, const int16_t* taps, uint32_t tapLen);

  /**
   * Decimate input samples
   * @param input Input samples at high rate
   * @param inputLen Number of input samples
   * @param output Output samples at low rate
   * @param maxOutputLen Maximum output buffer size
   * @param outputLen Actual output samples produced
   * @return true on success
   */
  bool decimate(const int16_t* input, uint32_t inputLen,
                int16_t* output, uint32_t maxOutputLen, uint32_t* outputLen);
};

/**
 * Interpolating Resampler - specialized for upsampling (M > N)
 * Optimized for 24kHz → 1MHz conversion (M=125, N=3)
 */
class CInterpolatingResampler : public CRationalResampler {
public:
  CInterpolatingResampler();
  ~CInterpolatingResampler();

  /**
   * Initialize for interpolation
   * @param interp Interpolation factor
   * @param taps Low-pass filter coefficients (scaled by interp)
   * @param tapLen Number of taps
   * @return true on success
   */
  bool initInterpolator(uint32_t interp, const int16_t* taps, uint32_t tapLen);

  /**
   * Interpolate input samples
   * @param input Input samples at low rate
   * @param inputLen Number of input samples
   * @param output Output samples at high rate
   * @param maxOutputLen Maximum output buffer size
   * @param outputLen Actual output samples produced
   * @return true on success
   */
  bool interpolate(const int16_t* input, uint32_t inputLen,
                   int16_t* output, uint32_t maxOutputLen, uint32_t* outputLen);
};

#endif // STANDALONE_MODE

#endif // RESAMPLER_H
