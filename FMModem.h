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

#if !defined(FMMODEM_H)
#define FMMODEM_H

#if defined(STANDALONE_MODE)

#include <stdint.h>

/**
 * FM Modulator - converts baseband audio to IQ samples
 * Uses phase accumulation with optional NEON optimization
 */
class CFMModulator {
public:
  CFMModulator();
  ~CFMModulator();

  /**
   * Initialize the FM modulator
   * @param sampleRate Output sample rate in Hz
   * @param deviation FM deviation in Hz (default: 5000)
   */
  void init(float sampleRate, float deviation = 5000.0f);

  /**
   * Modulate baseband samples to IQ
   * @param input Input baseband samples (Q15 format)
   * @param output_i Output I samples (Q15 format)
   * @param output_q Output Q samples (Q15 format)
   * @param length Number of samples to process
   */
  void modulate(const int16_t* input, int16_t* output_i, int16_t* output_q, uint32_t length);

  /**
   * Reset the modulator state
   */
  void reset();

private:
  float m_sensitivity;  // 2*pi*deviation/sampleRate
  float m_phase;        // Current phase accumulator
  float m_sampleRate;
  float m_deviation;

  // Sine/cosine lookup table for fast computation
  static const uint32_t LUT_SIZE = 4096;
  int16_t* m_sinLUT;
  int16_t* m_cosLUT;

  void initLUT();
  void modulateScalar(const int16_t* input, int16_t* output_i, int16_t* output_q, uint32_t length);
#if defined(USE_NEON)
  void modulateNEON(const int16_t* input, int16_t* output_i, int16_t* output_q, uint32_t length);
#endif
};

/**
 * FM Demodulator - converts IQ samples to baseband audio
 * Uses quadrature demodulation with optional NEON optimization
 */
class CFMDemodulator {
public:
  CFMDemodulator();
  ~CFMDemodulator();

  /**
   * Initialize the FM demodulator
   * @param sampleRate Input sample rate in Hz
   * @param deviation FM deviation in Hz (default: 5000)
   */
  void init(float sampleRate, float deviation = 5000.0f);

  /**
   * Demodulate IQ samples to baseband
   * @param input_i Input I samples (Q15 format)
   * @param input_q Input Q samples (Q15 format)
   * @param output Output baseband samples (Q15 format)
   * @param length Number of samples to process
   */
  void demodulate(const int16_t* input_i, const int16_t* input_q, int16_t* output, uint32_t length);

  /**
   * Reset the demodulator state
   */
  void reset();

private:
  float m_gain;         // sampleRate / (2*pi*deviation)
  int16_t m_prev_i;     // Previous I sample
  int16_t m_prev_q;     // Previous Q sample
  float m_sampleRate;
  float m_deviation;

  // Atan2 lookup table for fast approximation
  static const uint32_t ATAN2_LUT_SIZE = 256;
  int16_t* m_atan2LUT;

  void initLUT();
  int16_t fastAtan2(int16_t y, int16_t x);
  void demodulateScalar(const int16_t* input_i, const int16_t* input_q, int16_t* output, uint32_t length);
#if defined(USE_NEON)
  void demodulateNEON(const int16_t* input_i, const int16_t* input_q, int16_t* output, uint32_t length);
#endif
};

#endif // STANDALONE_MODE

#endif // FMMODEM_H
