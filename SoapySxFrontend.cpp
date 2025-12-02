#include "SoapySxFrontend.h"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Version.hpp>
#include <cstring>

SoapySxFrontend::SoapySxFrontend()
    : m_device(nullptr), m_rxStream(nullptr), m_txStream(nullptr),
      m_centerFreq(446000000.0), m_sampleRate(125000.0), m_rxGain(20.0),
      m_txGain(0.0) {}

SoapySxFrontend::~SoapySxFrontend() { close(); }

void SoapySxFrontend::setFrequency(double freqHz) { m_centerFreq = freqHz; }
void SoapySxFrontend::setSampleRate(double sampleRate) { m_sampleRate = sampleRate; }
void SoapySxFrontend::setRxGain(double gainDb) { m_rxGain = gainDb; }
void SoapySxFrontend::setTxGain(double gainDb) { m_txGain = gainDb; }

bool SoapySxFrontend::open() {
  if (m_device)
    return true;

  SoapySDR::Kwargs args;
  args["driver"] = "sx";

  m_device = SoapySDR::Device::make(args);
  if (m_device == nullptr)
    return false;

  m_device->setFrequency(SOAPY_SDR_RX, 0, m_centerFreq);
  m_device->setFrequency(SOAPY_SDR_TX, 0, m_centerFreq);

  m_device->setSampleRate(SOAPY_SDR_RX, 0, m_sampleRate);
  m_device->setSampleRate(SOAPY_SDR_TX, 0, m_sampleRate);

  m_device->setGain(SOAPY_SDR_RX, 0, m_rxGain);
  m_device->setGain(SOAPY_SDR_TX, 0, m_txGain);

  return true;
}

void SoapySxFrontend::close() {
  stopRx();
  stopTx();

  if (m_device != nullptr) {
    SoapySDR::Device::unmake(m_device);
    m_device = nullptr;
  }
}

bool SoapySxFrontend::startRx() {
  if (!m_device && !open())
    return false;

  if (m_rxStream == nullptr)
    m_rxStream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);

  if (m_rxStream == nullptr)
    return false;

  return m_device->activateStream(m_rxStream) == 0;
}

void SoapySxFrontend::stopRx() {
  if (m_device != nullptr && m_rxStream != nullptr) {
    m_device->deactivateStream(m_rxStream);
    m_device->closeStream(m_rxStream);
    m_rxStream = nullptr;
  }
}

bool SoapySxFrontend::startTx() {
  if (!m_device && !open())
    return false;

  if (m_txStream == nullptr)
    m_txStream = m_device->setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32);

  if (m_txStream == nullptr)
    return false;

  return m_device->activateStream(m_txStream) == 0;
}

void SoapySxFrontend::stopTx() {
  if (m_device != nullptr && m_txStream != nullptr) {
    m_device->deactivateStream(m_txStream);
    m_device->closeStream(m_txStream);
    m_txStream = nullptr;
  }
}

int SoapySxFrontend::readIq(std::complex<float> *buf, size_t len,
                             long long *timestamp) {
  if (m_device == nullptr || m_rxStream == nullptr)
    return -1;

  void *buffs[] = {buf};
  int flags = 0;
  long long ts = 0;
  int ret = m_device->readStream(m_rxStream, buffs, len, flags, ts, 100000);
  if (timestamp)
    *timestamp = ts;
  return ret;
}

int SoapySxFrontend::writeIq(const std::complex<float> *buf, size_t len,
                              bool withEOM) {
  if (m_device == nullptr || m_txStream == nullptr)
    return -1;

  const void *buffs[] = {buf};
  int flags = withEOM ? SOAPY_SDR_END_BURST : 0;
  long long ts = 0;
  return m_device->writeStream(m_txStream, buffs, len, flags, ts, 100000);
}

