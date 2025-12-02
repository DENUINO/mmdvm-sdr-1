/*
 *   Copyright (C) 2015,2016,2017,2018 by Jonathan Naylor G4KLX
 *   Copyright (C) 2015 by Jim Mclaughlin KI6ZUM
 *   Copyright (C) 2016 by Colin Durbridge G4EML
 * 
 *   GNU radio integration code written by Adrian Musceac YO8RZZ 2021
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
#include "IO.h"
#include <pthread.h>
#include <vector>
#include <algorithm>
#include <complex>

#if defined(RPI)

#include "Log.h"

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>


const uint16_t DC_OFFSET = 2048U;

unsigned char wavheader[] = {0x52,0x49,0x46,0x46,0xb8,0xc0,0x8f,0x00,0x57,0x41,0x56,0x45,0x66,0x6d,0x74,0x20,0x10,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0xc0,0x5d,0x00,0x00,0x80,0xbb,0x00,0x00,0x02,0x00,0x10,0x00,0x64,0x61,0x74,0x61,0xff,0xff,0xff,0xff};

void CIO::initInt()
{

//	std::cout << "IO Init" << std::endl;
	DEBUG1("IO Init done! Thread Started!");

}

void CIO::startInt()
{

        DEBUG1("IO Int start()");
    if (::pthread_mutex_init(&m_TXlock, NULL) != 0)
    {
        printf("\n Tx mutex init failed\n");
        exit(1);;
    }
    if (::pthread_mutex_init(&m_RXlock, NULL) != 0)
    {
        printf("\n RX mutex init failed\n");
        exit(1);;
    }

    if (!m_frontend.open()) {
        LogError("Failed to open SoapySX frontend");
    } else {
        if (!m_frontend.startRx())
            LogError("Failed to start RX stream");
        if (!m_frontend.startTx())
            LogError("Failed to start TX stream");

        m_sdrSampleRate = m_frontend.getSampleRate();
        m_rxResampleRatio = m_sdrSampleRate / double(MODEM_SAMPLE_RATE);
        m_txResampleRatio = m_sdrSampleRate / double(MODEM_SAMPLE_RATE);
    }

    ::pthread_create(&m_thread, NULL, helper, this);
    ::pthread_create(&m_threadRX, NULL, helperRX, this);
}

void* CIO::helper(void* arg)
{
  CIO* p = (CIO*)arg;

  while (1)
  {
      if(p->m_txBuffer.getData() < 1)
        usleep(20);
    p->interrupt();
  }

  return NULL;
}

void* CIO::helperRX(void* arg)
{
  CIO* p = (CIO*)arg;

  while (1)
  {

    usleep(20);
    p->interruptRX();
  }

  return NULL;
}


void CIO::interrupt()
{
    std::vector<std::complex<float>> iqOut;
    iqOut.reserve(512);

    ::pthread_mutex_lock(&m_TXlock);
    while (iqOut.size() < 512 && m_txBuffer.getData() > 0) {
        uint16_t sample = 0;
        uint8_t control = MARK_NONE;
        m_txBuffer.get(sample, control);

        q15_t current = q15_t(sample);
        double step = m_txResampleRatio;
        double pos = m_txFrac;
        while (pos < step && iqOut.size() < 512) {
            double interp = m_prevTxSample + (current - m_prevTxSample) * (pos / step);
            float scaled = float(std::clamp(interp / 32768.0, -1.0, 1.0));
            iqOut.emplace_back(scaled, 0.0f);
            pos += 1.0;
        }
        m_txFrac = pos - step;
        m_prevTxSample = current;
    }
    ::pthread_mutex_unlock(&m_TXlock);

    if (!iqOut.empty())
        m_frontend.writeIq(iqOut.data(), iqOut.size());
}

void CIO::interruptRX()
{
    std::complex<float> rxBuf[512];
    int got = m_frontend.readIq(rxBuf, 512);
    if (got <= 0)
        return;

    double step = m_rxResampleRatio;
    double acc = m_rxFrac;

    ::pthread_mutex_lock(&m_RXlock);
    for (int i = 0; i < got; ++i) {
        float realVal = rxBuf[i].real();
        realVal = std::clamp(realVal, -1.0f, 1.0f);
        q15_t current = q15_t(realVal * 32767.0f);

        acc += 1.0;
        if (acc >= step) {
            m_rxBuffer.put(uint16_t(current), MARK_NONE);
            m_rssiBuffer.put(uint16_t(std::abs(current)));
            acc -= step;
        }
        m_prevRxSample = current;
    }
    m_rxFrac = acc;
    ::pthread_mutex_unlock(&m_RXlock);
    return;
}

bool CIO::getCOSInt()

{
	return m_COSint;
}

void CIO::setLEDInt(bool on)
{
}

void CIO::setPTTInt(bool on)
{
	//handle enable clock gpio4

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
  usleep(dly*1000);
}



#endif

