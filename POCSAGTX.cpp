/*
 *   Copyright (C) 2009-2018,2020 by Jonathan Naylor G4KLX
 *   Copyright (C) 2025 MMDVM-SDR Adaptation
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
#include "POCSAGTX.h"
#include "POCSAGDefines.h"

// POCSAG signal levels (20 samples per symbol at 1200 baud)
const q15_t POCSAG_LEVEL1[] = { 1700, 1700, 1700, 1700, 1700, 1700, 1700, 1700, 1700, 1700,
                                1700, 1700, 1700, 1700, 1700, 1700, 1700, 1700, 1700, 1700};
const q15_t POCSAG_LEVEL0[] = {-1700, -1700, -1700, -1700, -1700, -1700, -1700, -1700, -1700, -1700,
                               -1700, -1700, -1700, -1700, -1700, -1700, -1700, -1700, -1700, -1700};

// Shaping filter coefficients
static q15_t SHAPING_FILTER[] = {5461, 5461, 5461, 5461, 5461, 5461};
const uint16_t SHAPING_FILTER_LEN = 6U;

CPOCSAGTX::CPOCSAGTX() :
m_fifo(),
m_fifoHead(0U),
m_fifoTail(0U),
m_fifoSize(4000U),
m_modFilter(),
m_modState(),
m_poBuffer(),
m_poLen(0U),
m_poPtr(0U),
m_txDelay(POCSAG_PREAMBLE_LENGTH_BYTES)
{
  ::memset(m_modState, 0x00U, 170U * sizeof(q15_t));
  ::memset(m_fifo, 0x00U, 4000U * sizeof(uint8_t));

  m_modFilter.numTaps  = SHAPING_FILTER_LEN;
  m_modFilter.pState   = m_modState;
  m_modFilter.pCoeffs  = SHAPING_FILTER;

  fifoInit();
}

void CPOCSAGTX::fifoInit()
{
  m_fifoHead = 0U;
  m_fifoTail = 0U;
}

bool CPOCSAGTX::fifoPut(uint8_t data)
{
  uint16_t next = (m_fifoHead + 1U) % m_fifoSize;
  if (next == m_fifoTail)
    return false;  // FIFO full

  m_fifo[m_fifoHead] = data;
  m_fifoHead = next;
  return true;
}

bool CPOCSAGTX::fifoGet(uint8_t& data)
{
  if (m_fifoHead == m_fifoTail)
    return false;  // FIFO empty

  data = m_fifo[m_fifoTail];
  m_fifoTail = (m_fifoTail + 1U) % m_fifoSize;
  return true;
}

uint16_t CPOCSAGTX::fifoGetSpace() const
{
  if (m_fifoHead >= m_fifoTail)
    return m_fifoSize - (m_fifoHead - m_fifoTail) - 1U;
  else
    return m_fifoTail - m_fifoHead - 1U;
}

uint16_t CPOCSAGTX::fifoGetData() const
{
  if (m_fifoHead >= m_fifoTail)
    return m_fifoHead - m_fifoTail;
  else
    return m_fifoSize - (m_fifoTail - m_fifoHead);
}

void CPOCSAGTX::process()
{
  if (fifoGetData() == 0U && m_poLen == 0U)
    return;

  if (m_poLen == 0U) {
    if (!m_tx) {
      // Send preamble
      for (uint16_t i = 0U; i < m_txDelay; i++)
        m_poBuffer[m_poLen++] = POCSAG_SYNC;
    } else {
      // Load frame from FIFO
      for (uint8_t i = 0U; i < POCSAG_FRAME_LENGTH_BYTES; i++) {
        uint8_t c = 0U;
        fifoGet(c);
        m_poBuffer[m_poLen++] = c;
      }
    }

    m_poPtr = 0U;
  }

  if (m_poLen > 0U) {
    uint16_t space = io.getSpace();

    while (space > (8U * POCSAG_RADIO_SYMBOL_LENGTH)) {
      uint8_t c = m_poBuffer[m_poPtr++];
      writeByte(c);

      space -= 8U * POCSAG_RADIO_SYMBOL_LENGTH;

      if (m_poPtr >= m_poLen) {
        m_poPtr = 0U;
        m_poLen = 0U;
        return;
      }
    }
  }
}

bool CPOCSAGTX::busy()
{
  return (m_poLen > 0U || fifoGetData() > 0U);
}

uint8_t CPOCSAGTX::writeData(const uint8_t* data, uint16_t length)
{
  if (length != POCSAG_FRAME_LENGTH_BYTES)
    return 4U;  // Invalid length

  uint16_t space = fifoGetSpace();
  if (space < POCSAG_FRAME_LENGTH_BYTES)
    return 5U;  // No space

  for (uint8_t i = 0U; i < POCSAG_FRAME_LENGTH_BYTES; i++)
    fifoPut(data[i]);

  return 0U;
}

void CPOCSAGTX::writeByte(uint8_t c)
{
  q15_t inBuffer[POCSAG_RADIO_SYMBOL_LENGTH * 8U];
  q15_t outBuffer[POCSAG_RADIO_SYMBOL_LENGTH * 8U];

  const uint8_t MASK = 0x80U;

  uint8_t n = 0U;
  for (uint8_t i = 0U; i < 8U; i++, c <<= 1, n += POCSAG_RADIO_SYMBOL_LENGTH) {
    switch (c & MASK) {
      case 0x80U:
        ::memcpy(inBuffer + n, POCSAG_LEVEL1, POCSAG_RADIO_SYMBOL_LENGTH * sizeof(q15_t));
        break;
      default:
        ::memcpy(inBuffer + n, POCSAG_LEVEL0, POCSAG_RADIO_SYMBOL_LENGTH * sizeof(q15_t));
        break;
    }
  }

  ::arm_fir_fast_q15(&m_modFilter, inBuffer, outBuffer, POCSAG_RADIO_SYMBOL_LENGTH * 8U);

  io.write(STATE_POCSAG, outBuffer, POCSAG_RADIO_SYMBOL_LENGTH * 8U);
}

void CPOCSAGTX::setTXDelay(uint8_t delay)
{
  m_txDelay = POCSAG_PREAMBLE_LENGTH_BYTES + (delay * 3U) / 2U;

  if (m_txDelay > 150U)
    m_txDelay = 150U;
}

uint8_t CPOCSAGTX::getSpace() const
{
  return fifoGetSpace() / POCSAG_FRAME_LENGTH_BYTES;
}
