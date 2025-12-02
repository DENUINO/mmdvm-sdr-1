/*
 *   SoapySX frontend wrapper for mmdvm-sdr
 *
 *   Provides a thin abstraction around SoapySDR to talk to the SX1255
 *   via the SoapySX driver (driver key: "sx"). IQ is exchanged as
 *   CF32 samples and resampled to the modem's internal rate upstream.
 */

#ifndef SOAPY_SX_FRONTEND_H
#define SOAPY_SX_FRONTEND_H

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <complex>
#include <cstddef>

class SoapySxFrontend {
public:
  SoapySxFrontend();
  ~SoapySxFrontend();

  // Configuration prior to opening
  void setFrequency(double freqHz);
  void setSampleRate(double sampleRate);
  void setRxGain(double gainDb);
  void setTxGain(double gainDb);

  bool open();
  void close();

  bool startRx();
  void stopRx();

  bool startTx();
  void stopTx();

  // Returns number of complex samples read, or negative on error
  int readIq(std::complex<float> *buf, size_t len, long long *timestamp = nullptr);
  // Returns number of complex samples written, or negative on error
  int writeIq(const std::complex<float> *buf, size_t len, bool withEOM = false);

  double getSampleRate() const { return m_sampleRate; }

private:
  SoapySDR::Device *m_device;
  SoapySDR::Stream *m_rxStream;
  SoapySDR::Stream *m_txStream;

  double m_centerFreq;
  double m_sampleRate;
  double m_rxGain;
  double m_txGain;
};

#endif // SOAPY_SX_FRONTEND_H

