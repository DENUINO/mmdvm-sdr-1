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

#if !defined(FMRX_H)
#define  FMRX_H

#include "Config.h"
#include "FMDefines.h"

/**
 * FM Receiver States
 * Simplified state machine for SDR FM reception
 */
enum FMRX_STATE {
  FMRXS_NONE,        // Idle, waiting for signal
  FMRXS_LISTENING,   // Monitoring for audio above squelch
  FMRXS_AUDIO        // Receiving active audio
};

/**
 * CFMRX - FM Receiver for SDR
 *
 * Processes 24 kHz baseband audio samples from the IO layer.
 * Unlike digital modes, FM doesn't have sync words or framing -
 * it's continuous audio with squelch-based carrier detection.
 */
class CFMRX {
public:
  CFMRX();

  /**
   * Process incoming audio samples
   * @param samples Input audio samples (Q15 format, 24 kHz)
   * @param rssi RSSI values for each sample
   * @param length Number of samples to process
   */
  void samples(const q15_t* samples, const uint16_t* rssi, uint8_t length);

  /**
   * Reset the FM receiver state
   */
  void reset();

  /**
   * Set squelch threshold
   * @param threshold Squelch level (Q15 format, 0-32767)
   */
  void setSquelch(q15_t threshold);

  /**
   * Set audio gain
   * @param gain Audio gain multiplier (Q15 format, typically 8192-32767)
   */
  void setGain(q15_t gain);

  /**
   * Enable/disable de-emphasis filter
   * @param enabled True to enable de-emphasis
   */
  void setDeemphasis(bool enabled);

private:
  FMRX_STATE m_state;                           // Current receiver state

  // Audio buffer for frame assembly
  q15_t m_buffer[FM_RX_BUFFER_SIZE];            // Sample buffer
  uint16_t m_bufferPtr;                         // Current position in buffer

  // Squelch state
  q15_t m_squelchThreshold;                     // Squelch threshold level
  bool m_squelchOpen;                           // Squelch gate state
  uint8_t m_hangCounter;                        // Squelch hang timer

  // Audio level tracking
  q31_t m_audioLevel;                           // Running average of audio level
  q15_t m_audioGain;                            // Audio gain setting

  // RSSI tracking
  uint32_t m_rssiAccum;                         // RSSI accumulator
  uint16_t m_rssiCount;                         // Number of RSSI samples
  uint16_t m_rssiAverage;                       // Average RSSI value

  // De-emphasis filter state (1st order IIR)
  bool m_deemphasisEnabled;                     // De-emphasis enable flag
  q31_t m_deemphasisState;                      // De-emphasis filter state
  q15_t m_deemphasisAlpha;                      // De-emphasis filter coefficient

  // DC blocking filter state
  q31_t m_dcBlockState;                         // DC blocker state
  q15_t m_dcBlockAlpha;                         // DC blocker coefficient

  // Frame counter for status reporting
  uint16_t m_frameCounter;                      // Counts frames for status updates

  /**
   * Process a single audio sample
   * @param sample Input sample
   * @return Processed sample
   */
  q15_t processSample(q15_t sample);

  /**
   * Update squelch state based on audio level
   * @param sample Current audio sample
   */
  void updateSquelch(q15_t sample);

  /**
   * Apply de-emphasis filter
   * @param sample Input sample
   * @return Filtered sample
   */
  q15_t applyDeemphasis(q15_t sample);

  /**
   * Apply DC blocking filter
   * @param sample Input sample
   * @return Filtered sample
   */
  q15_t applyDCBlock(q15_t sample);

  /**
   * Send audio frame to host via serial
   */
  void writeAudioFrame();

  /**
   * Calculate audio level (RMS-like)
   * @param sample Input sample
   */
  void updateAudioLevel(q15_t sample);
};

#endif
