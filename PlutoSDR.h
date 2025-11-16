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

#if !defined(PLUTOSDR_H)
#define PLUTOSDR_H

#if defined(STANDALONE_MODE) && defined(PLUTO_SDR)

#include <iio.h>
#include <stdint.h>
#include <string>

class CPlutoSDR {
public:
  CPlutoSDR();
  ~CPlutoSDR();

  /**
   * Initialize PlutoSDR device
   * @param uri Device URI (e.g., "ip:192.168.2.1", "usb:", "local:")
   * @param sampleRate Sample rate in Hz (default: 1000000)
   * @param bufferSize Buffer size in samples (default: 32768)
   * @return true on success, false on failure
   */
  bool init(const char* uri, uint32_t sampleRate = 1000000U, uint32_t bufferSize = 32768U);

  /**
   * Start RX and TX streaming
   * @return true on success
   */
  bool start();

  /**
   * Stop RX and TX streaming
   */
  void stop();

  /**
   * Set RX center frequency
   * @param freq Frequency in Hz
   * @return true on success
   */
  bool setRXFrequency(uint64_t freq);

  /**
   * Set TX center frequency
   * @param freq Frequency in Hz
   * @return true on success
   */
  bool setTXFrequency(uint64_t freq);

  /**
   * Set RX gain
   * @param gain Gain in dB (0-73 for manual mode)
   * @return true on success
   */
  bool setRXGain(int gain);

  /**
   * Set TX attenuation
   * @param atten Attenuation in dB (0.0 to 89.75, step 0.25)
   * @return true on success
   */
  bool setTXAttenuation(float atten);

  /**
   * Set RX bandwidth
   * @param bw Bandwidth in Hz
   * @return true on success
   */
  bool setRXBandwidth(uint32_t bw);

  /**
   * Set TX bandwidth
   * @param bw Bandwidth in Hz
   * @return true on success
   */
  bool setTXBandwidth(uint32_t bw);

  /**
   * Read RX samples (non-blocking)
   * @param i_samples Output buffer for I samples (16-bit signed)
   * @param q_samples Output buffer for Q samples (16-bit signed)
   * @param maxSamples Maximum number of samples to read
   * @return Number of samples actually read, or -1 on error
   */
  int readRXSamples(int16_t* i_samples, int16_t* q_samples, uint32_t maxSamples);

  /**
   * Write TX samples (non-blocking)
   * @param i_samples Input buffer for I samples (16-bit signed)
   * @param q_samples Input buffer for Q samples (16-bit signed)
   * @param numSamples Number of samples to write
   * @return Number of samples actually written, or -1 on error
   */
  int writeTXSamples(const int16_t* i_samples, const int16_t* q_samples, uint32_t numSamples);

  /**
   * Get current RX frequency
   * @return Frequency in Hz
   */
  uint64_t getRXFrequency() const { return m_rxFreq; }

  /**
   * Get current TX frequency
   * @return Frequency in Hz
   */
  uint64_t getTXFrequency() const { return m_txFreq; }

  /**
   * Get current sample rate
   * @return Sample rate in Hz
   */
  uint32_t getSampleRate() const { return m_sampleRate; }

  /**
   * Check if device is running
   * @return true if streaming
   */
  bool isRunning() const { return m_running; }

  /**
   * Get device temperature (if available)
   * @return Temperature in Celsius, or -273.15 if not available
   */
  float getTemperature();

private:
  bool configureRX();
  bool configureTX();
  bool setDeviceAttribute(const char* device, const char* channel, const char* attr, const char* value);
  bool setDeviceAttribute(const char* device, const char* channel, const char* attr, int64_t value);
  bool getDeviceAttribute(const char* device, const char* channel, const char* attr, char* value, size_t len);

  struct iio_context* m_ctx;
  struct iio_device* m_rxDev;
  struct iio_device* m_txDev;
  struct iio_device* m_rxPhy;
  struct iio_device* m_txPhy;

  struct iio_channel* m_rxI;
  struct iio_channel* m_rxQ;
  struct iio_channel* m_txI;
  struct iio_channel* m_txQ;

  struct iio_buffer* m_rxBuf;
  struct iio_buffer* m_txBuf;

  uint32_t m_sampleRate;
  uint32_t m_bufferSize;
  uint64_t m_rxFreq;
  uint64_t m_txFreq;
  int m_rxGain;
  float m_txAtten;
  bool m_running;
  bool m_initialized;

  // Statistics
  uint64_t m_rxSampleCount;
  uint64_t m_txSampleCount;
  uint32_t m_rxUnderflows;
  uint32_t m_txOverflows;
};

#endif // STANDALONE_MODE && PLUTO_SDR

#endif // PLUTOSDR_H
