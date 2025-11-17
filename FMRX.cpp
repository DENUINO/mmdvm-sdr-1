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

#include "Config.h"
#include "Globals.h"
#include "FMRX.h"
#include "Utils.h"

// De-emphasis filter coefficient: alpha = exp(-1/(tau * fs))
// For tau = 530us and fs = 24kHz: alpha ≈ 0.924
const q15_t DEEMPHASIS_ALPHA = 30277;  // Q15(0.924)

// DC blocking filter coefficient
const q15_t DC_BLOCK_ALPHA = 31130;    // Q15(0.95)

// Audio level decay for squelch (slow attack, fast decay)
const q15_t AUDIO_LEVEL_ATTACK = 32440;   // Q15(0.99) - slow attack
const q15_t AUDIO_LEVEL_DECAY = 29491;    // Q15(0.90) - faster decay

CFMRX::CFMRX() :
m_state(FMRXS_NONE),
m_buffer(),
m_bufferPtr(0U),
m_squelchThreshold(FM_SQUELCH_THRESHOLD_DEFAULT),
m_squelchOpen(false),
m_hangCounter(0U),
m_audioLevel(0),
m_audioGain(FM_AUDIO_GAIN_DEFAULT),
m_rssiAccum(0U),
m_rssiCount(0U),
m_rssiAverage(0U),
m_deemphasisEnabled(true),
m_deemphasisState(0),
m_deemphasisAlpha(DEEMPHASIS_ALPHA),
m_dcBlockState(0),
m_dcBlockAlpha(DC_BLOCK_ALPHA),
m_frameCounter(0U)
{
}

void CFMRX::reset()
{
  m_state = FMRXS_NONE;
  m_bufferPtr = 0U;
  m_squelchOpen = false;
  m_hangCounter = 0U;
  m_audioLevel = 0;
  m_rssiAccum = 0U;
  m_rssiCount = 0U;
  m_rssiAverage = 0U;
  m_deemphasisState = 0;
  m_dcBlockState = 0;
  m_frameCounter = 0U;
}

void CFMRX::setSquelch(q15_t threshold)
{
  m_squelchThreshold = threshold;
}

void CFMRX::setGain(q15_t gain)
{
  // Clamp gain to valid range
  if (gain < FM_AUDIO_GAIN_MIN)
    gain = FM_AUDIO_GAIN_MIN;
  if (gain > FM_AUDIO_GAIN_MAX)
    gain = FM_AUDIO_GAIN_MAX;

  m_audioGain = gain;
}

void CFMRX::setDeemphasis(bool enabled)
{
  m_deemphasisEnabled = enabled;
  if (!enabled)
    m_deemphasisState = 0;
}

void CFMRX::samples(const q15_t* samples, const uint16_t* rssi, uint8_t length)
{
  for (uint8_t i = 0U; i < length; i++) {
    q15_t sample = samples[i];

    // Accumulate RSSI for averaging
    if (rssi != NULL) {
      m_rssiAccum += rssi[i];
      m_rssiCount++;
    }

    // Update audio level for squelch
    updateAudioLevel(sample);

    // Update squelch state
    updateSquelch(sample);

    // Process sample through filters
    q15_t processed = processSample(sample);

    // Store in buffer
    m_buffer[m_bufferPtr++] = processed;

    // Check if we have a complete frame
    if (m_bufferPtr >= FM_FRAME_LENGTH_SAMPLES) {
      // Process the frame
      if (m_squelchOpen) {
        // Send audio to host
        writeAudioFrame();
      } else {
        // Squelch closed - send silence or no data
        // For now, we still send data but it will be muted
      }

      // Reset buffer pointer
      m_bufferPtr = 0U;
      m_frameCounter++;

      // Calculate average RSSI
      if (m_rssiCount > 0U) {
        m_rssiAverage = (uint16_t)(m_rssiAccum / m_rssiCount);
        m_rssiAccum = 0U;
        m_rssiCount = 0U;
      }
    }
  }
}

q15_t CFMRX::processSample(q15_t sample)
{
  q15_t output = sample;

  // Apply DC blocking filter
  output = applyDCBlock(output);

  // Apply de-emphasis if enabled
  if (m_deemphasisEnabled) {
    output = applyDeemphasis(output);
  }

  // Apply audio gain
  // Use Q15 multiplication: result = (output * gain) >> 15
  q31_t gained = ((q31_t)output * m_audioGain) >> 15;

  // Saturate to Q15 range
  if (gained > 32767)
    gained = 32767;
  else if (gained < -32768)
    gained = -32768;

  output = (q15_t)gained;

  // Apply soft limiting to prevent clipping
  if (output > FM_AUDIO_LIMIT)
    output = FM_AUDIO_LIMIT;
  else if (output < -FM_AUDIO_LIMIT)
    output = -FM_AUDIO_LIMIT;

  return output;
}

void CFMRX::updateAudioLevel(q15_t sample)
{
  // Calculate absolute value
  q15_t absValue = sample >= 0 ? sample : -sample;

  // Exponential moving average
  // If current sample is higher, use fast attack
  // If lower, use slower decay
  q31_t level = (q31_t)absValue;

  if (level > (m_audioLevel >> 15)) {
    // Attack: fast response to increasing level
    m_audioLevel = ((q31_t)AUDIO_LEVEL_ATTACK * (m_audioLevel >> 15) +
                    (32768 - AUDIO_LEVEL_ATTACK) * level) >> 15 << 15;
  } else {
    // Decay: slower response to decreasing level
    m_audioLevel = ((q31_t)AUDIO_LEVEL_DECAY * (m_audioLevel >> 15) +
                    (32768 - AUDIO_LEVEL_DECAY) * level) >> 15 << 15;
  }
}

void CFMRX::updateSquelch(q15_t sample)
{
  // Get current audio level (in Q15 format)
  q15_t currentLevel = (q15_t)(m_audioLevel >> 15);

  // Compare with threshold
  if (currentLevel > m_squelchThreshold) {
    // Signal present - open squelch
    m_squelchOpen = true;
    m_hangCounter = FM_SQUELCH_HANG_FRAMES;

    // Update state
    if (m_state == FMRXS_NONE || m_state == FMRXS_LISTENING) {
      m_state = FMRXS_AUDIO;
    }
  } else {
    // Signal weak or absent
    if (m_hangCounter > 0U) {
      m_hangCounter--;
      // Keep squelch open during hang time
      m_squelchOpen = true;
    } else {
      m_squelchOpen = false;

      // Update state
      if (m_state == FMRXS_AUDIO) {
        m_state = FMRXS_LISTENING;
      }
    }
  }
}

q15_t CFMRX::applyDeemphasis(q15_t sample)
{
  // 1st order IIR low-pass filter: y[n] = alpha * y[n-1] + (1-alpha) * x[n]
  // This implements de-emphasis for NBFM

  q31_t input = (q31_t)sample;
  q31_t alpha = (q31_t)m_deemphasisAlpha;

  // y[n] = alpha * y[n-1] + (1-alpha) * x[n]
  m_deemphasisState = ((alpha * m_deemphasisState) >> 15) +
                       (((32768 - alpha) * (input << 15)) >> 15);

  // Extract Q15 result
  q31_t output = m_deemphasisState >> 15;

  // Saturate
  if (output > 32767)
    output = 32767;
  else if (output < -32768)
    output = -32768;

  return (q15_t)output;
}

q15_t CFMRX::applyDCBlock(q15_t sample)
{
  // DC blocking filter: y[n] = x[n] - x[n-1] + alpha * y[n-1]
  // Implemented as 1st order high-pass IIR filter

  q31_t input = (q31_t)sample << 15;  // Convert to Q30
  q31_t alpha = (q31_t)m_dcBlockAlpha;

  // y[n] = alpha * y[n-1] + x[n] - x[n-1]
  // Simplified: y[n] = alpha * y[n-1] + (1-alpha) * x[n]
  // This is equivalent to de-emphasis formula but serves as HPF

  q31_t prevState = m_dcBlockState;
  m_dcBlockState = ((alpha * prevState) >> 15) +
                    (((32768 - alpha) * input) >> 15);

  // Output is current input minus DC component
  q31_t output = input - ((prevState + m_dcBlockState) >> 1);

  // Extract Q15 result
  output = output >> 15;

  // Saturate
  if (output > 32767)
    output = 32767;
  else if (output < -32768)
    output = -32768;

  return (q15_t)output;
}

void CFMRX::writeAudioFrame()
{
  // Prepare audio data packet for host
  // Format: Audio samples as 16-bit signed integers

  // For now, we'll send via the serial port using FM data format
  // The actual protocol integration will be handled in SerialPort.cpp

  // Create a buffer to send (simplified - full implementation would use proper protocol)
  uint8_t data[FM_FRAME_LENGTH_SAMPLES * 2];  // 2 bytes per Q15 sample

  for (uint16_t i = 0; i < FM_FRAME_LENGTH_SAMPLES; i++) {
    q15_t sample = m_buffer[i];

    // Convert Q15 to little-endian bytes
    data[i * 2 + 0] = (uint8_t)(sample & 0xFF);
    data[i * 2 + 1] = (uint8_t)((sample >> 8) & 0xFF);
  }

  // Send to host via serial port
  // This would call serial.writeFMData() in full implementation
  // For now, commented out as protocol support will be added later
  // serial.writeFMData(data, FM_FRAME_LENGTH_SAMPLES * 2);

  // Debug: Count frames received
  DEBUG2("FMRX: Frame", m_frameCounter);
}
