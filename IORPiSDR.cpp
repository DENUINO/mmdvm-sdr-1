/*
 *   Copyright (C) 2024 by MMDVM-SDR Contributors
 *
 *   Integrated I/O layer for standalone SDR operation
 *   Combines PlutoSDR, FM modem, and resampling
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "Config.h"

#if defined(STANDALONE_MODE) && defined(RPI)

#include "Globals.h"
#include "IO.h"
#include "Log.h"

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#if defined(PLUTO_SDR)
#include "PlutoSDR.h"
#include "FMModem.h"
#include "Resampler.h"
#endif

// ==================== Global SDR Components ====================

#if defined(PLUTO_SDR)
static CPlutoSDR g_plutoSDR;
static CFMModulator g_fmModulator;
static CFMDemodulator g_fmDemodulator;
static CInterpolatingResampler g_txResampler;  // 24kHz -> 1MHz
static CDecimatingResampler g_rxResampler;     // 1MHz -> 24kHz

// SDR I/Q buffers
static int16_t g_rxIQBuffer_I[SDR_RX_BUFFER_SIZE];
static int16_t g_rxIQBuffer_Q[SDR_RX_BUFFER_SIZE];
static int16_t g_txIQBuffer_I[SDR_TX_BUFFER_SIZE];
static int16_t g_txIQBuffer_Q[SDR_TX_BUFFER_SIZE];

// Intermediate buffers
static int16_t g_rxFMDemod[SDR_RX_BUFFER_SIZE];
static int16_t g_txFMMod_I[SDR_TX_BUFFER_SIZE];
static int16_t g_txFMMod_Q[SDR_TX_BUFFER_SIZE];
static int16_t g_rxBasebandTmp[SDR_RX_BUFFER_SIZE];
static int16_t g_txBasebandTmp[SDR_TX_BUFFER_SIZE];

// Resampler filter taps
// Low-pass FIR filter for resampling (42 taps, 5kHz cutoff @ 1MHz)
static const int16_t RESAMPLE_TAPS[] = {
  -45, -89, -101, -64, 35, 156, 254, 280, 194, 0,
  -250, -474, -563, -424, -0, 641, 1424, 2175, 2724, 2945,
  2724, 2175, 1424, 641, -0, -424, -563, -474, -250, 0,
  194, 280, 254, 156, 35, -64, -101, -89, -45, 0,
  0, 0
};
static const uint32_t RESAMPLE_TAP_LEN = 42;
#endif

// ==================== Initialization ====================

void CIO::initInt()
{
#if defined(PLUTO_SDR)
  DEBUG1("Initializing Standalone SDR mode");

  // Initialize PlutoSDR
  if (!g_plutoSDR.init(PLUTO_URI, SDR_SAMPLE_RATE, SDR_RX_BUFFER_SIZE)) {
    DEBUG1("ERROR: Failed to initialize PlutoSDR");
    exit(1);
  }

  // Set default frequencies (can be overridden by configuration)
  g_plutoSDR.setRXFrequency(435500000ULL);  // 435.5 MHz
  g_plutoSDR.setTXFrequency(435000000ULL);  // 435.0 MHz
  g_plutoSDR.setRXGain(64);
  g_plutoSDR.setTXAttenuation(0.0f);

  // Initialize FM modem
  g_fmModulator.init((float)SDR_SAMPLE_RATE, FM_DEVIATION);
  g_fmDemodulator.init((float)SDR_SAMPLE_RATE, FM_DEVIATION);

  // Initialize resamplers
  if (!g_txResampler.initInterpolator(125, RESAMPLE_TAPS, RESAMPLE_TAP_LEN)) {
    DEBUG1("ERROR: Failed to initialize TX resampler");
    exit(1);
  }

  if (!g_rxResampler.initDecimator(125, RESAMPLE_TAPS, RESAMPLE_TAP_LEN)) {
    DEBUG1("ERROR: Failed to initialize RX resampler");
    exit(1);
  }

  DEBUG1("SDR Init complete: %llu Hz RX, %llu Hz TX, %u Hz sample rate",
         (unsigned long long)g_plutoSDR.getRXFrequency(),
         (unsigned long long)g_plutoSDR.getTXFrequency(),
         g_plutoSDR.getSampleRate());
#else
  DEBUG1("IO Init done (ZMQ mode)");
#endif
}

void CIO::startInt()
{
  DEBUG1("IO Int start()");

#if defined(PLUTO_SDR)
  // Start PlutoSDR streaming
  if (!g_plutoSDR.start()) {
    DEBUG1("ERROR: Failed to start PlutoSDR");
    exit(1);
  }
#endif

  // Initialize mutexes
  if (::pthread_mutex_init(&m_TXlock, NULL) != 0) {
    printf("\n TX mutex init failed\n");
    exit(1);
  }
  if (::pthread_mutex_init(&m_RXlock, NULL) != 0) {
    printf("\n RX mutex init failed\n");
    exit(1);
  }

  // Start I/O threads
  ::pthread_create(&m_thread, NULL, helper, this);
  ::pthread_create(&m_threadRX, NULL, helperRX, this);

  DEBUG1("I/O threads started");
}

// ==================== Thread Helpers ====================

void* CIO::helper(void* arg)
{
  CIO* p = (CIO*)arg;

  while (1) {
    if (p->m_txBuffer.getData() < 1)
      usleep(20);
    p->interrupt();
  }

  return NULL;
}

void* CIO::helperRX(void* arg)
{
  CIO* p = (CIO*)arg;

  while (1) {
    usleep(20);
    p->interruptRX();
  }

  return NULL;
}

// ==================== TX Interrupt (Standalone SDR) ====================

void CIO::interrupt()
{
#if defined(PLUTO_SDR)
  uint16_t sample = DC_OFFSET;
  uint8_t control = MARK_NONE;

  ::pthread_mutex_lock(&m_TXlock);

  // Get samples from TX buffer (24kHz baseband)
  uint32_t basebandSampleCount = 0;
  while (m_txBuffer.get(sample, control) && basebandSampleCount < 720) {
    // Convert from DC offset format to signed
    int16_t signed_sample = (int16_t)(sample - DC_OFFSET);
    g_txBasebandTmp[basebandSampleCount++] = signed_sample;
  }

  ::pthread_mutex_unlock(&m_TXlock);

  if (basebandSampleCount == 0)
    return;

  // Upsample from 24kHz to 1MHz
  uint32_t resampledCount;
  if (!g_txResampler.interpolate(g_txBasebandTmp, basebandSampleCount,
                                  g_rxBasebandTmp, SDR_TX_BUFFER_SIZE, &resampledCount)) {
    DEBUG1("TX resampler error");
    return;
  }

  // FM modulate (baseband -> IQ)
  g_fmModulator.modulate(g_rxBasebandTmp, g_txIQBuffer_I, g_txIQBuffer_Q, resampledCount);

  // Send to PlutoSDR
  int sent = g_plutoSDR.writeTXSamples(g_txIQBuffer_I, g_txIQBuffer_Q, resampledCount);
  if (sent < 0) {
    DEBUG1("PlutoSDR TX error");
  }

#if defined(DEBUG_SDR_IO)
  DEBUG1("TX: %u baseband -> %u IQ samples (%d sent)", basebandSampleCount, resampledCount, sent);
#endif

#else
  // ZMQ mode (existing code from IORPi.cpp)
  uint16_t sample = DC_OFFSET;
  uint8_t control = MARK_NONE;

  ::pthread_mutex_lock(&m_TXlock);
  while (m_txBuffer.get(sample, control)) {
    sample *= 5;  // amplify by 12dB
    short signed_sample = (short)sample;

    if (m_audiobuf.size() >= 720) {
      zmq::message_t reply(720 * sizeof(short));
      memcpy(reply.data(), (unsigned char *)m_audiobuf.data(), 720 * sizeof(short));
      m_zmqsocket.send(reply, zmq::send_flags::dontwait);
      usleep(9600 * 3);
      m_audiobuf.erase(m_audiobuf.begin(), m_audiobuf.begin() + 720);
      m_audiobuf.push_back(signed_sample);
    } else {
      m_audiobuf.push_back(signed_sample);
    }
  }
  ::pthread_mutex_unlock(&m_TXlock);
#endif
}

// ==================== RX Interrupt (Standalone SDR) ====================

void CIO::interruptRX()
{
#if defined(PLUTO_SDR)
  uint8_t control = MARK_NONE;

  // Read IQ samples from PlutoSDR
  int received = g_plutoSDR.readRXSamples(g_rxIQBuffer_I, g_rxIQBuffer_Q, SDR_RX_BUFFER_SIZE);
  if (received <= 0)
    return;

#if defined(DEBUG_SDR_IO)
  DEBUG1("RX: %d IQ samples from SDR", received);
#endif

  // FM demodulate (IQ -> baseband)
  g_fmDemodulator.demodulate(g_rxIQBuffer_I, g_rxIQBuffer_Q, g_rxFMDemod, (uint32_t)received);

  // Downsample from 1MHz to 24kHz
  uint32_t basebandCount;
  if (!g_rxResampler.decimate(g_rxFMDemod, (uint32_t)received,
                               g_rxBasebandTmp, SDR_RX_BUFFER_SIZE, &basebandCount)) {
    DEBUG1("RX resampler error");
    return;
  }

#if defined(DEBUG_SDR_IO)
  DEBUG1("RX: %d IQ -> %u baseband samples", received, basebandCount);
#endif

  // Put samples into RX ring buffer
  ::pthread_mutex_lock(&m_RXlock);

  for (uint32_t i = 0; i < basebandCount; i++) {
    // Convert signed to DC offset format
    uint16_t sample = (uint16_t)(g_rxBasebandTmp[i] + DC_OFFSET);
    m_rxBuffer.put(sample, control);
    m_rssiBuffer.put(3U);  // Placeholder RSSI
  }

  ::pthread_mutex_unlock(&m_RXlock);

#else
  // ZMQ mode (existing code from IORPi.cpp)
  uint16_t sample = DC_OFFSET;
  uint8_t control = MARK_NONE;

  zmq::message_t mq_message;
  zmq::recv_result_t recv_result = m_zmqsocketRX.recv(mq_message, zmq::recv_flags::none);
  int size = mq_message.size();
  if (size < 1)
    return;

  ::pthread_mutex_lock(&m_RXlock);
  u_int16_t rx_buf_space = m_rxBuffer.getSpace();

  for (int i = 0; i < size; i += 2) {
    short signed_sample = 0;
    memcpy(&signed_sample, (unsigned char*)mq_message.data() + i, sizeof(short));
    m_rxBuffer.put((uint16_t)signed_sample, control);
    m_rssiBuffer.put(3U);
  }
  ::pthread_mutex_unlock(&m_RXlock);
#endif
}

// ==================== Platform-Specific Stubs ====================

bool CIO::getCOSInt()
{
  return m_COSint;
}

void CIO::setLEDInt(bool on)
{
  // No LED control in standalone mode
}

void CIO::setPTTInt(bool on)
{
  // PTT handled by PlutoSDR TX enable/disable
#if defined(PLUTO_SDR)
  // Could implement TX/RX switching logic here if needed
#endif
}

void CIO::setCOSInt(bool on)
{
  m_COSint = on;
}

void CIO::setDStarInt(bool on)
{
}

void CIO::setDMRInt(bool on)
{
}

void CIO::setYSFInt(bool on)
{
}

void CIO::setP25Int(bool on)
{
}

void CIO::setNXDNInt(bool on)
{
}

void CIO::delayInt(unsigned int dly)
{
  usleep(dly * 1000);
}

#endif // STANDALONE_MODE && RPI
