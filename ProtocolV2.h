/*
 *   Copyright (C) 2025 by MMDVM-SDR Contributors
 *   Copyright (C) 2015,2016,2017,2018,2020,2021 by Jonathan Naylor G4KLX
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

#if !defined(PROTOCOLV2_H)
#define PROTOCOLV2_H

// Protocol Version 2 Extensions
// Based on upstream g4klx/MMDVM Protocol Version 2
// Adapted for mmdvm-sdr (Linux/SDR platform)

// Mode Capability Flags (for GET_VERSION response)
#define CAP_DSTAR     0x01U
#define CAP_DMR       0x02U
#define CAP_YSF       0x04U
#define CAP_P25       0x08U
#define CAP_NXDN      0x10U
#define CAP_POCSAG    0x20U  // Not supported in mmdvm-sdr
#define CAP_FM        0x40U  // Not supported in mmdvm-sdr

// Extended Capabilities (future use)
#define EXCAP_NONE    0x00U

// FM Mode Commands (not supported, but defined for compatibility)
const uint8_t MMDVM_FM_PARAMS1  = 0x60U;
const uint8_t MMDVM_FM_PARAMS2  = 0x61U;
const uint8_t MMDVM_FM_PARAMS3  = 0x62U;
const uint8_t MMDVM_FM_DATA     = 0x65U;

// POCSAG Commands (not supported, but defined for compatibility)
const uint8_t MMDVM_POCSAG_DATA = 0x50U;

// Protocol V2 Status Response Structure
// GET_STATUS response format (Protocol V2):
// Byte 0: MMDVM_FRAME_START (0xE0)
// Byte 1: Length
// Byte 2: MMDVM_GET_STATUS (0x01)
// Byte 3: Mode enable flags (D-Star, DMR, YSF, P25, NXDN, POCSAG, FM)
// Byte 4: Current mode state
// Byte 5: Status flags (TX, ADC overflow, RX overflow, TX overflow, lockout, DAC overflow, DCD)
// Byte 6: D-Star TX buffer space
// Byte 7: DMR TX buffer 1 space
// Byte 8: DMR TX buffer 2 space
// Byte 9: YSF TX buffer space
// Byte 10: P25 TX buffer space
// Byte 11: NXDN TX buffer space
// Byte 12: POCSAG TX buffer space (not used)
// Byte 13: FM TX buffer space (not used)

// Protocol V2 Version Response Structure
// GET_VERSION response format (Protocol V2):
// Byte 0: MMDVM_FRAME_START (0xE0)
// Byte 1: Length
// Byte 2: MMDVM_GET_VERSION (0x00)
// Byte 3: Protocol version (2)
// Byte 4: Mode capability flags
// Byte 5: Extended capability flags (future use)
// Byte 6+: Hardware description string (null-terminated)

// Error Codes (enhanced for Protocol V2)
const uint8_t ERR_INVALID_COMMAND    = 1U;
const uint8_t ERR_WRONG_MODE         = 2U;
const uint8_t ERR_BUFFER_OVERFLOW    = 3U;
const uint8_t ERR_INVALID_PARAMS     = 4U;
const uint8_t ERR_NOT_SUPPORTED      = 5U;  // New in V2
const uint8_t ERR_TX_OVERFLOW        = 6U;  // New in V2

#endif // PROTOCOLV2_H
