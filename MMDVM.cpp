/*
 *   Copyright (C) 2015,2016,2017,2018 by Jonathan Naylor G4KLX
 *   Copyright (C) 2016 by Mathis Schmieder DB9MAT
 *   Copyright (C) 2016 by Colin Durbridge G4EML
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

#if defined(STM32F4XX) || defined(STM32F7XX) || defined(STM32F105xC) || defined(RPI)

#include "Config.h"
#include "Globals.h"

#include "Log.h"
#include "unistd.h"

// Transport layer includes
#ifdef USE_UDP_MODEM
  #include "UDPModemPort.h"
#endif

// Global variables
MMDVM_STATE m_modemState = STATE_IDLE;

bool m_dstarEnable  = true;
bool m_dmrEnable    = true;
bool m_ysfEnable    = true;
bool m_p25Enable    = true;
bool m_nxdnEnable   = true;
bool m_pocsagEnable = true;

bool m_duplex = true;

bool m_tx  = false;
bool m_dcd = false;

CDStarRX   dstarRX;
CDStarTX   dstarTX;

CDMRIdleRX dmrIdleRX;
CDMRRX     dmrRX;
CDMRTX     dmrTX;

CDMRDMORX  dmrDMORX;
CDMRDMOTX  dmrDMOTX;

CYSFRX     ysfRX;
CYSFTX     ysfTX;

CP25RX     p25RX;
CP25TX     p25TX;

CNXDNRX    nxdnRX;
CNXDNTX    nxdnTX;

CPOCSAGTX  pocsagTX;

CCalDStarRX calDStarRX;
CCalDStarTX calDStarTX;
CCalDMR     calDMR;
CCalP25     calP25;
CCalNXDN    calNXDN;
CCalPOCSAG  calPOCSAG;
CCalRSSI    calRSSI;

CCWIdTX cwIdTX;

CSerialPort serial;
CIO io;

void setup()
{
  LogDebug("MMDVM modem setup()");

#if defined(RPI) && defined(USE_UDP_MODEM)
  // UDP transport mode
  LogMessage("Initializing UDP modem transport");
  LogMessage("  Remote: %s:%u", UDP_MODEM_ADDRESS, UDP_MODEM_PORT);
  LogMessage("  Local:  %s:%u", UDP_LOCAL_ADDRESS, UDP_LOCAL_PORT);

  static CUDPModemPort* udpPort = new CUDPModemPort(
    UDP_MODEM_ADDRESS,
    UDP_MODEM_PORT,
    UDP_LOCAL_ADDRESS,
    UDP_LOCAL_PORT
  );

  if (!udpPort->open()) {
    LogError("Failed to open UDP modem port");
    return;
  }

  // Inject UDP port into serial handler
  serial.setPort(udpPort);
  LogMessage("UDP modem port initialized successfully");
#else
  // PTY transport mode (default)
  LogMessage("Using PTY transport (traditional mode)");
#endif

  serial.start();
}

void loop()
{
  serial.process();
  
  io.process();

  // The following is for transmitting
  if (m_dstarEnable && m_modemState == STATE_DSTAR)
    dstarTX.process();

  if (m_dmrEnable && m_modemState == STATE_DMR) {

    //LogDebug("Invoking DMR TX process()");

    if (m_duplex)
      dmrTX.process();
    else
      dmrDMOTX.process();
  }
  //else
  // DEBUG4("Modem state: %d | Dmr Enable: %d | m_duplex: %d", m_modemState, m_dmrEnable,m_duplex);

  if (m_ysfEnable && m_modemState == STATE_YSF)
    ysfTX.process();

  if (m_p25Enable && m_modemState == STATE_P25)
    p25TX.process();

  if (m_nxdnEnable && m_modemState == STATE_NXDN)
    nxdnTX.process();

  if (m_pocsagEnable && m_modemState == STATE_POCSAG)
    pocsagTX.process();

  if (m_modemState == STATE_DSTARCAL)
    calDStarTX.process();

  if (m_modemState == STATE_DMRCAL || m_modemState == STATE_LFCAL || m_modemState == STATE_DMRCAL1K || m_modemState == STATE_DMRDMO1K)
    calDMR.process();

  if (m_modemState == STATE_P25CAL1K)
    calP25.process();

  if (m_modemState == STATE_NXDNCAL1K)
    calNXDN.process();

  if (m_modemState == STATE_POCSAGCAL)
    calPOCSAG.process();

  if (m_modemState == STATE_IDLE)
    cwIdTX.process();
  
  usleep(20);
}

int main()
{
  setup();

  for (;;)
    loop();
}

#endif
