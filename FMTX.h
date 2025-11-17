/*
 *   Copyright (C) 2025 by MMDVM-SDR Contributors
 *   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX (pattern)
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

#if !defined(FMTX_H)
#define  FMTX_H

#include "Config.h"
#include "FMDefines.h"

/**
 * FM Transmitter States
 */
enum FMTX_STATE {
  FMTXS_IDLE,        // Not transmitting
  FMTX_AUDIO,        // Transmitting audio
  FMTXS_SHUTDOWN     // Shutting down transmission
};

/**
 * CFMTX - FM Transmitter for SDR
 *
 * Accepts audio data from the host and generates 24 kHz baseband samples
 * for the IO layer to transmit via SDR.
 */
class CFMTX {
public:
  CFMTX();

  /**
   * Get next batch of samples for transmission
   * @param samples Output buffer for samples (Q15 format, 24 kHz)
   * @param length Number of samples requested
   * @return Number of samples actually written
   */
  uint8_t getSamples(q15_t* samples, uint8_t length);

  /**
   * Write audio data from host for transmission
   * @param data Audio data buffer (Q15 samples as bytes)
   * @param length Number of bytes (should be even, 2 bytes per sample)
   * @return Number of samples actually written to buffer
   */
  uint16_t writeData(const uint8_t* data, uint16_t length);

  /**
   * Reset the FM transmitter state
   */
  void reset();

  /**
   * Set pre-emphasis filter enable
   * @param enabled True to enable pre-emphasis
   */
  void setPreemphasis(bool enabled);

  /**
   * Set audio gain
   * @param gain Audio gain multiplier (Q15 format)
   */
  void setGain(q15_t gain);

  /**
   * Get available space in TX buffer
   * @return Number of samples that can be written
   */
  uint16_t getSpace() const;

  /**
   * Check if transmitter has data to send
   * @return True if TX buffer has data
   */
  bool hasData() const;

  /**
   * Set transmitter timeout
   * @param timeout Timeout in frames (20ms per frame)
   */
  void setTimeout(uint16_t timeout);

private:
  FMTX_STATE m_state;                           // Current transmitter state

  // TX buffer (ring buffer for audio samples)
  q15_t m_buffer[FM_TX_BUFFER_SIZE];            // Circular buffer
  uint16_t m_readPtr;                           // Read position
  uint16_t m_writePtr;                          // Write position
  uint16_t m_count;                             // Number of samples in buffer

  // Audio gain
  q15_t m_audioGain;                            // Audio gain setting

  // Pre-emphasis filter state (1st order IIR high-pass)
  bool m_preemphasisEnabled;                    // Pre-emphasis enable flag
  q31_t m_preemphasisState;                     // Pre-emphasis filter state
  q15_t m_preemphasisAlpha;                     // Pre-emphasis filter coefficient

  // DC blocking filter state
  q31_t m_dcBlockState;                         // DC blocker state
  q15_t m_dcBlockAlpha;                         // DC blocker coefficient

  // Timeout protection
  uint16_t m_timeoutFrames;                     // Timeout in frames
  uint16_t m_frameCounter;                      // Current frame count

  /**
   * Process a single audio sample
   * @param sample Input sample
   * @return Processed sample
   */
  q15_t processSample(q15_t sample);

  /**
   * Apply pre-emphasis filter
   * @param sample Input sample
   * @return Filtered sample
   */
  q15_t applyPreemphasis(q15_t sample);

  /**
   * Apply DC blocking filter
   * @param sample Input sample
   * @return Filtered sample
   */
  q15_t applyDCBlock(q15_t sample);

  /**
   * Generate silence samples
   * @param samples Output buffer
   * @param length Number of samples to generate
   */
  void generateSilence(q15_t* samples, uint8_t length);
};

#endif
