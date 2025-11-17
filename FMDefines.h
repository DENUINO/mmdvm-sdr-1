/*
 *   Copyright (C) 2025 by MMDVM-SDR Contributors
 *   Copyright (C) 2020,2021 by Jonathan Naylor G4KLX (upstream FM mode)
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

#if !defined(FMDEFINES_H)
#define  FMDEFINES_H

// FM mode constants for SDR implementation
// Based on 24 kHz sample rate (MMDVM baseband rate)

// Audio bandwidth: 300 Hz to 3000 Hz (standard voice bandwidth)
const unsigned int FM_SAMPLE_RATE = 24000U;           // 24 kHz baseband

// Frame timing (at 24 kHz)
const unsigned int FM_FRAME_LENGTH_MS = 20U;          // 20 ms audio frames
const unsigned int FM_FRAME_LENGTH_SAMPLES = 480U;    // 24000 * 0.020 = 480 samples

// Buffer sizes
const unsigned int FM_RX_BUFFER_SIZE = 960U;          // 40 ms buffer (2 frames)
const unsigned int FM_TX_BUFFER_SIZE = 960U;          // 40 ms buffer (2 frames)

// Audio processing parameters
const unsigned int FM_AUDIO_BLOCK_SIZE = 160U;        // Process 160 samples at a time (~6.67 ms)

// De-emphasis/pre-emphasis time constants (for NBFM)
const float FM_DEEMPHASIS_TAU = 0.000530f;            // 530 µs (North America standard)
const float FM_PREEMPHASIS_TAU = 0.000530f;           // 530 µs

// Audio level control
const q15_t FM_AUDIO_GAIN_MIN = 8192;                 // Q15(0.25) - minimum audio gain
const q15_t FM_AUDIO_GAIN_MAX = 32767;                // Q15(1.0) - maximum audio gain
const q15_t FM_AUDIO_GAIN_DEFAULT = 16384;            // Q15(0.5) - default audio gain

// DC blocking filter coefficient (very slow time constant for audio)
const float FM_DC_BLOCK_ALPHA = 0.95f;

// Squelch levels (Q15 format)
const q15_t FM_SQUELCH_THRESHOLD_LOW = 328;           // Q15(0.01) - very sensitive
const q15_t FM_SQUELCH_THRESHOLD_MED = 1638;          // Q15(0.05) - medium
const q15_t FM_SQUELCH_THRESHOLD_HIGH = 3277;         // Q15(0.10) - less sensitive
const q15_t FM_SQUELCH_THRESHOLD_DEFAULT = FM_SQUELCH_THRESHOLD_MED;

// Hang time for squelch (in frames)
const unsigned int FM_SQUELCH_HANG_FRAMES = 5U;       // 100 ms hang time

// CTCSS/PL tone parameters (if implemented in future)
const unsigned int FM_CTCSS_SAMPLE_RATE = FM_SAMPLE_RATE;
const unsigned int FM_CTCSS_FREQ_MIN = 67U;           // 67.0 Hz
const unsigned int FM_CTCSS_FREQ_MAX = 254U;          // 254.1 Hz

// Audio filter parameters (for 300-3000 Hz bandpass)
// Using simple 1st order high-pass at 300 Hz and low-pass at 3000 Hz
const float FM_HPF_CUTOFF = 300.0f;                   // 300 Hz high-pass
const float FM_LPF_CUTOFF = 3000.0f;                  // 3000 Hz low-pass

// Status reporting interval
const unsigned int FM_STATUS_INTERVAL_MS = 250U;      // Report status every 250 ms

// Maximum deviation (for SDR FM modulation)
// This matches Config.h FM_DEVIATION setting
const float FM_MAX_DEVIATION = 5000.0f;               // 5 kHz deviation (NBFM)

// Audio sample format
const uint8_t FM_SAMPLE_SIZE_BYTES = 2U;              // Q15 format = 2 bytes per sample

// State machine timeouts (in frames at 20 ms/frame)
const unsigned int FM_TIMEOUT_FRAMES = 300U;          // 6 seconds (300 * 20ms)
const unsigned int FM_KERCHUNK_FRAMES = 25U;          // 500 ms minimum transmission

// Protocol constants (matching SerialPort.cpp expectations)
const uint8_t FM_MODE_IDLE = 0x00U;
const uint8_t FM_MODE_RX = 0x01U;
const uint8_t FM_MODE_TX = 0x02U;

// Audio compression/limiting
const q15_t FM_AUDIO_LIMIT = 29491;                   // Q15(0.9) - soft limit to prevent clipping

#endif
