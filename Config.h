/*
 *   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
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

#if !defined(CONFIG_H)
#define  CONFIG_H

// Allow for the use of high quality external clock oscillators
// The number is the frequency of the oscillator in Hertz.
//
// The frequency of the TCXO must be an integer multiple of 48000.
// Frequencies such as 12.0 Mhz (48000 * 250) and 14.4 Mhz (48000 * 300) are suitable.
// Frequencies such as 10.0 Mhz (48000 * 208.333) or 20 Mhz (48000 * 416.666) are not suitable.
//
// For 12 MHz
#define EXTERNAL_OSC 12000000
// For 12.288 MHz
// #define EXTERNAL_OSC 12288000
// For 14.4 MHz
// #define EXTERNAL_OSC 14400000
// For 19.2 MHz
// #define EXTERNAL_OSC 19200000

// Allow the use of the COS line to lockout the modem
// #define USE_COS_AS_LOCKOUT

// Use pins to output the current mode
// #define ARDUINO_MODE_PINS

// For the original Arduino Due pin layout
// #define ARDUINO_DUE_PAPA

// For the ZUM V1.0 and V1.0.1 boards pin layout
// #define ARDUINO_DUE_ZUM_V10

// For the SQ6POG board
// #define STM32F1_POG

// For the SP8NTH board
// #define ARDUINO_DUE_NTH

// For ST Nucleo-64 STM32F446RE board
// #define STM32F4_NUCLEO_MORPHO_HEADER
// #define STM32F4_NUCLEO_ARDUINO_HEADER

// Use separate mode pins to switch external filters/bandwidth for example
// #define STM32F4_NUCLEO_MODE_PINS

// Pass RSSI information to the host
// #define SEND_RSSI_DATA

// Use the modem as a serial repeater for Nextion displays
// #define SERIAL_REPEATER

// To reduce CPU load, you can remove the DC blocker by commenting out the next line
#define USE_DCBLOCKER

// ==================== Standalone SDR Mode ====================
// Uncomment to enable standalone SDR operation (no GNU Radio required)
// This integrates PlutoSDR direct access, FM modem, and resampling
// #define STANDALONE_MODE

// PlutoSDR configuration (requires STANDALONE_MODE)
#if defined(STANDALONE_MODE)
  // Define SDR device type
  #define PLUTO_SDR
  // #define HACKRF
  // #define LIMESDR

  // Enable ARM NEON SIMD optimizations
  // Recommended for PlutoSDR (Zynq7000 Cortex-A9) and RaspberryPi 2+
  #define USE_NEON

  // Enable text UI for standalone operation
  // Requires ncurses library
  #define TEXT_UI

  // SDR sample rate (Hz) - PlutoSDR supports 520kHz to 61.44MHz
  #define SDR_SAMPLE_RATE 1000000U

  // MMDVM baseband rate (fixed by protocol)
  #define BASEBAND_RATE 24000U

  // FM deviation for PlutoSDR (Hz)
  #define FM_DEVIATION 5000.0f

  // PlutoSDR default URI
  #define PLUTO_URI "ip:192.168.2.1"
  // #define PLUTO_URI "usb:"
  // #define PLUTO_URI "local:"

  // Buffer sizes for SDR I/O
  #define SDR_RX_BUFFER_SIZE 32768U
  #define SDR_TX_BUFFER_SIZE 32768U

  // Text UI update rate (Hz)
  #define UI_UPDATE_RATE 10U

  // Debug options for standalone mode
  // #define DEBUG_SDR_IO
  // #define DEBUG_RESAMPLER
  // #define DEBUG_FM_MODEM
#endif

#endif
