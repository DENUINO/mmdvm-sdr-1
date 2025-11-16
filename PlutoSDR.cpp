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

#if defined(STANDALONE_MODE) && defined(PLUTO_SDR)

#include "PlutoSDR.h"
#include "Log.h"
#include <cstring>
#include <cstdio>
#include <errno.h>

CPlutoSDR::CPlutoSDR() :
m_ctx(nullptr),
m_rxDev(nullptr),
m_txDev(nullptr),
m_rxPhy(nullptr),
m_txPhy(nullptr),
m_rxI(nullptr),
m_rxQ(nullptr),
m_txI(nullptr),
m_txQ(nullptr),
m_rxBuf(nullptr),
m_txBuf(nullptr),
m_sampleRate(1000000U),
m_bufferSize(32768U),
m_rxFreq(435500000ULL),
m_txFreq(435000000ULL),
m_rxGain(64),
m_txAtten(0.0f),
m_running(false),
m_initialized(false),
m_rxSampleCount(0ULL),
m_txSampleCount(0ULL),
m_rxUnderflows(0U),
m_txOverflows(0U)
{
}

CPlutoSDR::~CPlutoSDR()
{
  stop();

  if (m_rxBuf)
    iio_buffer_destroy(m_rxBuf);
  if (m_txBuf)
    iio_buffer_destroy(m_txBuf);
  if (m_ctx)
    iio_context_destroy(m_ctx);
}

bool CPlutoSDR::init(const char* uri, uint32_t sampleRate, uint32_t bufferSize)
{
  if (m_initialized) {
    DEBUG1("PlutoSDR already initialized");
    return true;
  }

  m_sampleRate = sampleRate;
  m_bufferSize = bufferSize;

  DEBUG1("Initializing PlutoSDR: URI=%s, SampleRate=%u, BufferSize=%u", uri, sampleRate, bufferSize);

  // Create IIO context
  m_ctx = iio_create_context_from_uri(uri);
  if (!m_ctx) {
    m_ctx = iio_create_default_context();
    if (!m_ctx) {
      DEBUG1("PlutoSDR: Failed to create IIO context");
      return false;
    }
    DEBUG1("PlutoSDR: Using default context");
  }

  // Get RX device (cf-ad9361-lpc)
  m_rxDev = iio_context_find_device(m_ctx, "cf-ad9361-lpc");
  if (!m_rxDev) {
    DEBUG1("PlutoSDR: RX device not found");
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }

  // Get TX device (cf-ad9361-dds-core-lpc)
  m_txDev = iio_context_find_device(m_ctx, "cf-ad9361-dds-core-lpc");
  if (!m_txDev) {
    DEBUG1("PlutoSDR: TX device not found");
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }

  // Get PHY devices
  m_rxPhy = iio_context_find_device(m_ctx, "ad9361-phy");
  if (!m_rxPhy) {
    DEBUG1("PlutoSDR: PHY device not found");
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }
  m_txPhy = m_rxPhy;  // Same PHY for TX and RX on PlutoSDR

  // Configure RX
  if (!configureRX()) {
    DEBUG1("PlutoSDR: RX configuration failed");
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }

  // Configure TX
  if (!configureTX()) {
    DEBUG1("PlutoSDR: TX configuration failed");
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }

  // Get RX channels
  m_rxI = iio_device_find_channel(m_rxDev, "voltage0", false);
  m_rxQ = iio_device_find_channel(m_rxDev, "voltage1", false);
  if (!m_rxI || !m_rxQ) {
    DEBUG1("PlutoSDR: RX channels not found");
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }

  // Get TX channels
  m_txI = iio_device_find_channel(m_txDev, "voltage0", true);
  m_txQ = iio_device_find_channel(m_txDev, "voltage1", true);
  if (!m_txI || !m_txQ) {
    DEBUG1("PlutoSDR: TX channels not found");
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }

  // Enable RX channels
  iio_channel_enable(m_rxI);
  iio_channel_enable(m_rxQ);

  // Enable TX channels
  iio_channel_enable(m_txI);
  iio_channel_enable(m_txQ);

  // Create RX buffer
  m_rxBuf = iio_device_create_buffer(m_rxDev, m_bufferSize, false);
  if (!m_rxBuf) {
    DEBUG1("PlutoSDR: Failed to create RX buffer");
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }

  // Create TX buffer
  m_txBuf = iio_device_create_buffer(m_txDev, m_bufferSize, false);
  if (!m_txBuf) {
    DEBUG1("PlutoSDR: Failed to create TX buffer");
    iio_buffer_destroy(m_rxBuf);
    m_rxBuf = nullptr;
    iio_context_destroy(m_ctx);
    m_ctx = nullptr;
    return false;
  }

  m_initialized = true;
  DEBUG1("PlutoSDR initialized successfully");
  return true;
}

bool CPlutoSDR::configureRX()
{
  // Set RX sample rate
  char buf[64];
  snprintf(buf, sizeof(buf), "%u", m_sampleRate);
  if (!setDeviceAttribute("ad9361-phy", "voltage0", "sampling_frequency", buf)) {
    DEBUG1("PlutoSDR: Failed to set RX sample rate");
    return false;
  }

  // Set RX bandwidth
  uint32_t bandwidth = m_sampleRate;  // Same as sample rate for maximum bandwidth
  snprintf(buf, sizeof(buf), "%u", bandwidth);
  if (!setDeviceAttribute("ad9361-phy", "voltage0", "rf_bandwidth", buf)) {
    DEBUG1("PlutoSDR: Failed to set RX bandwidth");
    return false;
  }

  // Set RX frequency
  if (!setRXFrequency(m_rxFreq)) {
    return false;
  }

  // Set RX gain mode to manual
  if (!setDeviceAttribute("ad9361-phy", "voltage0", "gain_control_mode", "manual")) {
    DEBUG1("PlutoSDR: Failed to set RX gain mode");
    return false;
  }

  // Set RX gain
  if (!setRXGain(m_rxGain)) {
    return false;
  }

  // Enable RX port
  if (!setDeviceAttribute("ad9361-phy", nullptr, "ensm_mode", "fdd")) {
    DEBUG1("PlutoSDR: Failed to set FDD mode");
    return false;
  }

  DEBUG1("PlutoSDR RX configured: freq=%llu Hz, gain=%d dB, bw=%u Hz",
         (unsigned long long)m_rxFreq, m_rxGain, bandwidth);
  return true;
}

bool CPlutoSDR::configureTX()
{
  // Set TX sample rate
  char buf[64];
  snprintf(buf, sizeof(buf), "%u", m_sampleRate);
  if (!setDeviceAttribute("ad9361-phy", "voltage0", "sampling_frequency_out", buf)) {
    DEBUG1("PlutoSDR: Failed to set TX sample rate");
    return false;
  }

  // Set TX bandwidth
  uint32_t bandwidth = m_sampleRate;
  snprintf(buf, sizeof(buf), "%u", bandwidth);
  if (!setDeviceAttribute("ad9361-phy", "voltage0", "rf_bandwidth_out", buf)) {
    DEBUG1("PlutoSDR: Failed to set TX bandwidth");
    return false;
  }

  // Set TX frequency
  if (!setTXFrequency(m_txFreq)) {
    return false;
  }

  // Set TX attenuation
  if (!setTXAttenuation(m_txAtten)) {
    return false;
  }

  DEBUG1("PlutoSDR TX configured: freq=%llu Hz, atten=%.2f dB, bw=%u Hz",
         (unsigned long long)m_txFreq, m_txAtten, bandwidth);
  return true;
}

bool CPlutoSDR::start()
{
  if (!m_initialized) {
    DEBUG1("PlutoSDR not initialized");
    return false;
  }

  if (m_running) {
    DEBUG1("PlutoSDR already running");
    return true;
  }

  m_running = true;
  m_rxSampleCount = 0ULL;
  m_txSampleCount = 0ULL;
  m_rxUnderflows = 0U;
  m_txOverflows = 0U;

  DEBUG1("PlutoSDR started");
  return true;
}

void CPlutoSDR::stop()
{
  if (!m_running)
    return;

  m_running = false;
  DEBUG1("PlutoSDR stopped (RX: %llu samples, TX: %llu samples, Underflows: %u, Overflows: %u)",
         (unsigned long long)m_rxSampleCount, (unsigned long long)m_txSampleCount,
         m_rxUnderflows, m_txOverflows);
}

bool CPlutoSDR::setRXFrequency(uint64_t freq)
{
  m_rxFreq = freq;

  if (!m_initialized)
    return true;  // Will be set during init

  return setDeviceAttribute("ad9361-phy", "altvoltage0", "frequency", (int64_t)freq);
}

bool CPlutoSDR::setTXFrequency(uint64_t freq)
{
  m_txFreq = freq;

  if (!m_initialized)
    return true;  // Will be set during init

  return setDeviceAttribute("ad9361-phy", "altvoltage1", "frequency", (int64_t)freq);
}

bool CPlutoSDR::setRXGain(int gain)
{
  m_rxGain = gain;

  if (!m_initialized)
    return true;  // Will be set during init

  return setDeviceAttribute("ad9361-phy", "voltage0", "hardwaregain", (int64_t)gain);
}

bool CPlutoSDR::setTXAttenuation(float atten)
{
  m_txAtten = atten;

  if (!m_initialized)
    return true;  // Will be set during init

  // Convert to millidB
  int64_t atten_mdb = (int64_t)(atten * 1000.0f);
  return setDeviceAttribute("ad9361-phy", "voltage0", "hardwaregain_out", -atten_mdb);
}

bool CPlutoSDR::setRXBandwidth(uint32_t bw)
{
  if (!m_initialized)
    return false;

  char buf[64];
  snprintf(buf, sizeof(buf), "%u", bw);
  return setDeviceAttribute("ad9361-phy", "voltage0", "rf_bandwidth", buf);
}

bool CPlutoSDR::setTXBandwidth(uint32_t bw)
{
  if (!m_initialized)
    return false;

  char buf[64];
  snprintf(buf, sizeof(buf), "%u", bw);
  return setDeviceAttribute("ad9361-phy", "voltage0", "rf_bandwidth_out", buf);
}

int CPlutoSDR::readRXSamples(int16_t* i_samples, int16_t* q_samples, uint32_t maxSamples)
{
  if (!m_running || !m_rxBuf)
    return -1;

  // Refill buffer
  ssize_t ret = iio_buffer_refill(m_rxBuf);
  if (ret < 0) {
    DEBUG1("PlutoSDR RX buffer refill error: %d", (int)ret);
    m_rxUnderflows++;
    return -1;
  }

  // Get buffer pointers
  void* buf_start = iio_buffer_start(m_rxBuf);
  void* buf_end = iio_buffer_end(m_rxBuf);
  ptrdiff_t buf_size = (char*)buf_end - (char*)buf_start;

  // Calculate number of samples (I/Q pairs)
  uint32_t num_samples = buf_size / 4;  // 2 bytes per I, 2 bytes per Q
  if (num_samples > maxSamples)
    num_samples = maxSamples;

  // Deinterleave I/Q samples
  int16_t* src = (int16_t*)buf_start;
  for (uint32_t i = 0; i < num_samples; i++) {
    i_samples[i] = src[i * 2];
    q_samples[i] = src[i * 2 + 1];
  }

  m_rxSampleCount += num_samples;
  return (int)num_samples;
}

int CPlutoSDR::writeTXSamples(const int16_t* i_samples, const int16_t* q_samples, uint32_t numSamples)
{
  if (!m_running || !m_txBuf)
    return -1;

  // Get buffer pointers
  void* buf_start = iio_buffer_start(m_txBuf);
  void* buf_end = iio_buffer_end(m_txBuf);
  ptrdiff_t buf_size = (char*)buf_end - (char*)buf_start;

  // Calculate maximum samples
  uint32_t max_samples = buf_size / 4;  // 2 bytes per I, 2 bytes per Q
  if (numSamples > max_samples) {
    DEBUG1("PlutoSDR TX: truncating %u samples to %u", numSamples, max_samples);
    numSamples = max_samples;
  }

  // Interleave I/Q samples
  int16_t* dst = (int16_t*)buf_start;
  for (uint32_t i = 0; i < numSamples; i++) {
    dst[i * 2] = i_samples[i];
    dst[i * 2 + 1] = q_samples[i];
  }

  // Push buffer
  ssize_t ret = iio_buffer_push(m_txBuf);
  if (ret < 0) {
    DEBUG1("PlutoSDR TX buffer push error: %d", (int)ret);
    m_txOverflows++;
    return -1;
  }

  m_txSampleCount += numSamples;
  return (int)numSamples;
}

float CPlutoSDR::getTemperature()
{
  if (!m_initialized)
    return -273.15f;

  char buf[64];
  if (getDeviceAttribute("ad9361-phy", nullptr, "in_temp0_input", buf, sizeof(buf))) {
    return atof(buf) / 1000.0f;  // Convert millidegrees to degrees
  }

  return -273.15f;  // Not available
}

bool CPlutoSDR::setDeviceAttribute(const char* device, const char* channel, const char* attr, const char* value)
{
  if (!m_ctx)
    return false;

  struct iio_device* dev = iio_context_find_device(m_ctx, device);
  if (!dev) {
    DEBUG1("PlutoSDR: Device '%s' not found", device);
    return false;
  }

  struct iio_channel* chn = nullptr;
  if (channel) {
    chn = iio_device_find_channel(dev, channel, false);
    if (!chn) {
      chn = iio_device_find_channel(dev, channel, true);
      if (!chn) {
        DEBUG1("PlutoSDR: Channel '%s' not found in device '%s'", channel, device);
        return false;
      }
    }

    int ret = iio_channel_attr_write(chn, attr, value);
    if (ret < 0) {
      DEBUG1("PlutoSDR: Failed to set %s/%s/%s = %s (error %d)", device, channel, attr, value, ret);
      return false;
    }
  } else {
    int ret = iio_device_attr_write(dev, attr, value);
    if (ret < 0) {
      DEBUG1("PlutoSDR: Failed to set %s/%s = %s (error %d)", device, attr, value, ret);
      return false;
    }
  }

  return true;
}

bool CPlutoSDR::setDeviceAttribute(const char* device, const char* channel, const char* attr, int64_t value)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "%lld", (long long)value);
  return setDeviceAttribute(device, channel, attr, buf);
}

bool CPlutoSDR::getDeviceAttribute(const char* device, const char* channel, const char* attr, char* value, size_t len)
{
  if (!m_ctx)
    return false;

  struct iio_device* dev = iio_context_find_device(m_ctx, device);
  if (!dev)
    return false;

  if (channel) {
    struct iio_channel* chn = iio_device_find_channel(dev, channel, false);
    if (!chn)
      return false;

    int ret = iio_channel_attr_read(chn, attr, value, len);
    return (ret >= 0);
  } else {
    int ret = iio_device_attr_read(dev, attr, value, len);
    return (ret >= 0);
  }
}

#endif // STANDALONE_MODE && PLUTO_SDR
