/*
 *   Copyright (C) 2015,2016,2018,2020 by Jonathan Naylor G4KLX
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

#if !defined(POCSAGDEFINES_H)
#define  POCSAGDEFINES_H

// POCSAG Protocol Constants
const uint16_t POCSAG_FRAME_LENGTH_BYTES    = 17U * sizeof(uint32_t);  // 68 bytes
const uint16_t POCSAG_PREAMBLE_LENGTH_BYTES = 18U * sizeof(uint32_t);  // 72 bytes
const uint16_t POCSAG_RADIO_SYMBOL_LENGTH   = 20U;                     // Samples per symbol at 24kHz

// POCSAG Sync Pattern
const uint8_t  POCSAG_SYNC                  = 0xAAU;
const uint32_t POCSAG_SYNC_WORD             = 0x7CD215D8U;

// POCSAG Baud Rates (bits per second)
const uint16_t POCSAG_BAUD_512              = 512U;
const uint16_t POCSAG_BAUD_1200             = 1200U;
const uint16_t POCSAG_BAUD_2400             = 2400U;

// POCSAG Frame Structure
const uint8_t  POCSAG_BATCH_SIZE            = 16U;   // 16 batches per transmission
const uint8_t  POCSAG_CODEWORDS_PER_BATCH   = 8U;    // 8 codewords per batch
const uint8_t  POCSAG_BITS_PER_CODEWORD     = 32U;   // 32 bits per codeword

// POCSAG Address and Function Codes
const uint32_t POCSAG_ADDRESS_MASK          = 0xFFFFF800U;  // 21-bit address
const uint8_t  POCSAG_FUNCTION_MASK         = 0x03U;        // 2-bit function code

// Function codes (2 bits)
const uint8_t  POCSAG_FUNCTION_0            = 0U;  // Numeric
const uint8_t  POCSAG_FUNCTION_1            = 1U;  // Tone 1
const uint8_t  POCSAG_FUNCTION_2            = 2U;  // Tone 2
const uint8_t  POCSAG_FUNCTION_3            = 3U;  // Alphanumeric

// BCH Error Correction
const uint8_t  POCSAG_BCH_DATA_BITS         = 21U;  // 21 data bits
const uint8_t  POCSAG_BCH_PARITY_BITS       = 10U;  // 10 parity bits
const uint8_t  POCSAG_BCH_TOTAL_BITS        = 31U;  // Total BCH coded bits

// POCSAG Signal Levels
const q15_t    POCSAG_LEVEL_HIGH            = 1700;
const q15_t    POCSAG_LEVEL_LOW             = -1700;

// Shaping Filter
const uint16_t POCSAG_SHAPING_FILTER_LEN    = 6U;

#endif
