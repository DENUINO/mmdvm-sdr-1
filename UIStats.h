/*
 *   Copyright (C) 2024 by MMDVM-SDR Contributors
 *
 *   Statistics collection for text UI
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#if !defined(UISTATS_H)
#define UISTATS_H

#if defined(TEXT_UI)

#include <stdint.h>
#include <string>
#include "Globals.h"

/**
 * Statistics collection and management for text UI
 */
class CUIStats {
public:
  CUIStats();
  ~CUIStats();

  // System statistics
  void updateCPUUsage();
  float getCPUUsage() const { return m_cpuUsage; }

  void updateMemoryUsage();
  uint32_t getMemoryUsed() const { return m_memoryUsed; }

  float getTemperature() const { return m_temperature; }
  void setTemperature(float temp) { m_temperature = temp; }

  // Buffer statistics
  void setRXBufferLevel(uint32_t used, uint32_t total);
  void setTXBufferLevel(uint32_t used, uint32_t total);
  uint32_t getRXBufferUsed() const { return m_rxBufferUsed; }
  uint32_t getRXBufferTotal() const { return m_rxBufferTotal; }
  uint32_t getTXBufferUsed() const { return m_txBufferUsed; }
  uint32_t getTXBufferTotal() const { return m_txBufferTotal; }

  // Mode statistics
  void setCurrentMode(MMDVM_STATE mode);
  MMDVM_STATE getCurrentMode() const { return m_currentMode; }
  const char* getModeName() const;

  void setRXActive(bool active);
  void setTXActive(bool active);
  bool isRXActive() const { return m_rxActive; }
  bool isTXActive() const { return m_txActive; }

  void setRSSI(uint16_t rssi);
  int getRSSI_dBm() const;

  // Mode-specific statistics
  struct DMRStats {
    uint32_t slot1Frames;
    uint32_t slot1Errors;
    uint32_t slot2Frames;
    uint32_t slot2Errors;
    uint8_t colorCode;
    float getFER(uint8_t slot) const;
  };

  struct DStarStats {
    uint32_t frames;
    uint32_t errors;
    float getFER() const;
  };

  struct YSFStats {
    uint32_t frames;
    uint32_t errors;
    float getFER() const;
  };

  struct P25Stats {
    uint32_t frames;
    uint32_t errors;
    float getFER() const;
  };

  struct NXDNStats {
    uint32_t frames;
    uint32_t errors;
    float getFER() const;
  };

  void updateDMRStats(uint8_t slot, uint32_t frames, uint32_t errors);
  void updateDStarStats(uint32_t frames, uint32_t errors);
  void updateYSFStats(uint32_t frames, uint32_t errors);
  void updateP25Stats(uint32_t frames, uint32_t errors);
  void updateNXDNStats(uint32_t frames, uint32_t errors);

  const DMRStats& getDMRStats() const { return m_dmrStats; }
  const DStarStats& getDStarStats() const { return m_dstarStats; }
  const YSFStats& getYSFStats() const { return m_ysfStats; }
  const P25Stats& getP25Stats() const { return m_p25Stats; }
  const NXDNStats& getNXDNStats() const { return m_nxdnStats; }

  // Network statistics
  void setMMDVMHostConnected(bool connected);
  bool isMMDVMHostConnected() const { return m_mmdvmConnected; }

  void setMMDVMHostAddress(const char* address);
  const char* getMMDVMHostAddress() const { return m_mmdvmAddress.c_str(); }

  void setLastCommand(const char* cmd);
  const char* getLastCommand() const { return m_lastCommand.c_str(); }
  uint32_t getTimeSinceLastCommand() const;

  // Uptime
  void updateUptime();
  uint32_t getUptimeSeconds() const { return m_uptimeSeconds; }
  void getUptimeString(char* buf, size_t len) const;

  // Frequency settings
  void setRXFrequency(uint64_t freq) { m_rxFreq = freq; }
  void setTXFrequency(uint64_t freq) { m_txFreq = freq; }
  uint64_t getRXFrequency() const { return m_rxFreq; }
  uint64_t getTXFrequency() const { return m_txFreq; }

  void setRXGain(int gain) { m_rxGain = gain; }
  int getRXGain() const { return m_rxGain; }

  // Error counters
  void incrementADCOverflow() { m_adcOverflows++; }
  void incrementDACOverflow() { m_dacOverflows++; }
  void incrementRXUnderflow() { m_rxUnderflows++; }
  void incrementTXOverflow() { m_txOverflows++; }

  uint32_t getADCOverflows() const { return m_adcOverflows; }
  uint32_t getDACOverflows() const { return m_dacOverflows; }
  uint32_t getRXUnderflows() const { return m_rxUnderflows; }
  uint32_t getTXOverflows() const { return m_txOverflows; }

  // Sample rate accuracy
  void updateSampleRate(float measured);
  float getMeasuredSampleRate() const { return m_measuredSampleRate; }

private:
  // System
  float m_cpuUsage;
  uint32_t m_memoryUsed;
  float m_temperature;
  uint32_t m_uptimeSeconds;
  uint32_t m_startTime;

  // Buffers
  uint32_t m_rxBufferUsed;
  uint32_t m_rxBufferTotal;
  uint32_t m_txBufferUsed;
  uint32_t m_txBufferTotal;

  // Mode
  MMDVM_STATE m_currentMode;
  bool m_rxActive;
  bool m_txActive;
  uint16_t m_rssi;

  // Mode statistics
  DMRStats m_dmrStats;
  DStarStats m_dstarStats;
  YSFStats m_ysfStats;
  P25Stats m_p25Stats;
  NXDNStats m_nxdnStats;

  // Network
  bool m_mmdvmConnected;
  std::string m_mmdvmAddress;
  std::string m_lastCommand;
  uint32_t m_lastCommandTime;

  // RF settings
  uint64_t m_rxFreq;
  uint64_t m_txFreq;
  int m_rxGain;

  // Errors
  uint32_t m_adcOverflows;
  uint32_t m_dacOverflows;
  uint32_t m_rxUnderflows;
  uint32_t m_txOverflows;

  // Sample rate
  float m_measuredSampleRate;
};

#endif // TEXT_UI

#endif // UISTATS_H
