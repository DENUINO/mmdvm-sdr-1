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
#include "FMTX.h"

// Pre-emphasis filter coefficient: alpha = 1 - exp(-1/(tau * fs))
// For tau = 530us and fs = 24kHz: alpha ≈ 0.076
const q15_t PREEMPHASIS_ALPHA = 2490;  // Q15(0.076)

// DC blocking filter coefficient
const q15_t DC_BLOCK_ALPHA = 31130;    // Q15(0.95)

CFMTX::CFMTX() :
m_state(FMTXS_IDLE),
m_buffer(),
m_readPtr(0U),
m_writePtr(0U),
m_count(0U),
m_audioGain(FM_AUDIO_GAIN_DEFAULT),
m_preemphasisEnabled(true),
m_preemphasisState(0),
m_preemphasisAlpha(PREEMPHASIS_ALPHA),
m_dcBlockState(0),
m_dcBlockAlpha(DC_BLOCK_ALPHA),
m_timeoutFrames(FM_TIMEOUT_FRAMES),
m_frameCounter(0U)
{
}

void CFMTX::reset()
{
  m_state = FMTXS_IDLE;
  m_readPtr = 0U;
  m_writePtr = 0U;
  m_count = 0U;
  m_preemphasisState = 0;
  m_dcBlockState = 0;
  m_frameCounter = 0U;
}

void CFMTX::setPreemphasis(bool enabled)
{
  m_preemphasisEnabled = enabled;
  if (!enabled)
    m_preemphasisState = 0;
}

void CFMTX::setGain(q15_t gain)
{
  // Clamp gain to valid range
  if (gain < FM_AUDIO_GAIN_MIN)
    gain = FM_AUDIO_GAIN_MIN;
  if (gain > FM_AUDIO_GAIN_MAX)
    gain = FM_AUDIO_GAIN_MAX;

  m_audioGain = gain;
}

void CFMTX::setTimeout(uint16_t timeout)
{
  m_timeoutFrames = timeout;
}

uint16_t CFMTX::getSpace() const
{
  return FM_TX_BUFFER_SIZE - m_count;
}

bool CFMTX::hasData() const
{
  return m_count > 0U;
}

uint16_t CFMTX::writeData(const uint8_t* data, uint16_t length)
{
  // Convert byte data to Q15 samples
  // Data format: little-endian 16-bit signed integers

  if (data == NULL || length == 0U)
    return 0U;

  // Ensure even number of bytes (2 bytes per sample)
  if (length & 1U)
    length--;

  uint16_t samplesWritten = 0U;
  uint16_t numSamples = length / 2U;

  for (uint16_t i = 0U; i < numSamples; i++) {
    // Check if buffer is full
    if (m_count >= FM_TX_BUFFER_SIZE)
      break;

    // Extract Q15 sample from little-endian bytes
    uint8_t low = data[i * 2];
    uint8_t high = data[i * 2 + 1];
    q15_t sample = (q15_t)((high << 8) | low);

    // Store in buffer
    m_buffer[m_writePtr] = sample;
    m_writePtr = (m_writePtr + 1U) % FM_TX_BUFFER_SIZE;
    m_count++;
    samplesWritten++;

    // Update state
    if (m_state == FMTXS_IDLE && m_count > 0U) {
      m_state = FMTX_AUDIO;
      m_frameCounter = 0U;
    }
  }

  return samplesWritten;
}

uint8_t CFMTX::getSamples(q15_t* samples, uint8_t length)
{
  if (samples == NULL || length == 0U)
    return 0U;

  uint8_t samplesRead = 0U;

  // Check timeout
  if (m_frameCounter >= m_timeoutFrames && m_state == FMTX_AUDIO) {
    m_state = FMTXS_SHUTDOWN;
    DEBUG1("FMTX: Timeout exceeded");
  }

  switch (m_state) {
    case FMTXS_IDLE:
      // No data to transmit - generate silence
      generateSilence(samples, length);
      samplesRead = length;
      break;

    case FMTX_AUDIO:
      // Transmit audio from buffer
      for (uint8_t i = 0U; i < length; i++) {
        if (m_count == 0U) {
          // Buffer empty - fill rest with silence
          for (uint8_t j = i; j < length; j++) {
            samples[j] = 0;
          }
          samplesRead = length;
          m_state = FMTXS_IDLE;
          break;
        }

        // Get sample from buffer
        q15_t sample = m_buffer[m_readPtr];
        m_readPtr = (m_readPtr + 1U) % FM_TX_BUFFER_SIZE;
        m_count--;

        // Process sample
        samples[i] = processSample(sample);
        samplesRead++;
      }

      // Increment frame counter
      if (samplesRead > 0U) {
        m_frameCounter += (samplesRead / FM_AUDIO_BLOCK_SIZE);
      }
      break;

    case FMTXS_SHUTDOWN:
      // Generate silence and transition to idle
      generateSilence(samples, length);
      samplesRead = length;
      m_state = FMTXS_IDLE;
      break;
  }

  return samplesRead;
}

q15_t CFMTX::processSample(q15_t sample)
{
  q15_t output = sample;

  // Apply audio gain
  // Use Q15 multiplication: result = (output * gain) >> 15
  q31_t gained = ((q31_t)output * m_audioGain) >> 15;

  // Saturate to Q15 range
  if (gained > 32767)
    gained = 32767;
  else if (gained < -32768)
    gained = -32768;

  output = (q15_t)gained;

  // Apply pre-emphasis if enabled
  if (m_preemphasisEnabled) {
    output = applyPreemphasis(output);
  }

  // Apply DC blocking filter
  output = applyDCBlock(output);

  // Apply soft limiting to prevent over-deviation
  if (output > FM_AUDIO_LIMIT)
    output = FM_AUDIO_LIMIT;
  else if (output < -FM_AUDIO_LIMIT)
    output = -FM_AUDIO_LIMIT;

  return output;
}

q15_t CFMTX::applyPreemphasis(q15_t sample)
{
  // 1st order IIR high-pass filter for pre-emphasis
  // y[n] = x[n] - x[n-1] + alpha * y[n-1]
  // This boosts high frequencies to compensate for de-emphasis at receiver

  q31_t input = (q31_t)sample << 15;  // Convert to Q30
  q31_t alpha = (q31_t)m_preemphasisAlpha;

  // Simplified pre-emphasis: y[n] = (1-alpha) * y[n-1] + alpha * x[n]
  // This creates a high-pass characteristic
  m_preemphasisState = (((32768 - alpha) * m_preemphasisState) >> 15) +
                        ((alpha * input) >> 15);

  // Output is input plus high-frequency boost
  q31_t output = input + (m_preemphasisState >> 1);  // Add 50% of HPF output

  // Extract Q15 result
  output = output >> 15;

  // Saturate
  if (output > 32767)
    output = 32767;
  else if (output < -32768)
    output = -32768;

  return (q15_t)output;
}

q15_t CFMTX::applyDCBlock(q15_t sample)
{
  // DC blocking filter: y[n] = x[n] - x[n-1] + alpha * y[n-1]
  // Implemented as 1st order high-pass IIR filter

  q31_t input = (q31_t)sample << 15;  // Convert to Q30
  q31_t alpha = (q31_t)m_dcBlockAlpha;

  // y[n] = alpha * y[n-1] + (1-alpha) * x[n]
  // This removes DC offset
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

void CFMTX::generateSilence(q15_t* samples, uint8_t length)
{
  for (uint8_t i = 0U; i < length; i++) {
    samples[i] = 0;
  }
}
