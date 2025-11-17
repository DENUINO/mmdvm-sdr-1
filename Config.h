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

// FM mode audio filtering (de-emphasis on RX, pre-emphasis on TX)
// Enabled by default for proper FM voice quality
#define FM_AUDIO_FILTER

// ==================== Protocol Version Configuration ====================
// MMDVM Serial Protocol Version Selection
//
// Protocol Version 1 (default): Compatible with older MMDVMHost versions
// Protocol Version 2: Enhanced status reporting, capability flags, extended error handling
//
// Uncomment one of the following:
#define PROTOCOL_VERSION 1
// #define PROTOCOL_VERSION 2
//
// Notes:
// - Protocol V1: Tested with MMDVMHost versions up to ~2020
// - Protocol V2: Compatible with latest MMDVMHost versions (2021+)
// - Both versions support: D-Star, DMR, YSF, P25, NXDN
// - Neither version supports: FM, POCSAG (not implemented in mmdvm-sdr)
// - V2 adds: Mode capability flags, extended status fields, improved error codes

// ==================== Standalone SDR Mode ====================
// Uncomment to enable standalone SDR operation (no GNU Radio required)
// This integrates PlutoSDR direct access, FM modem, and resampling
// #define STANDALONE_MODE

// PlutoSDR configuration (requires STANDALONE_MODE)
#if defined(STANDALONE_MODE)
  // SDR device type defaults (only used when building without CMake)
  // When using CMake, these are controlled by build options
  // #if !defined(PLUTO_SDR) && !defined(HACKRF) && !defined(LIMESDR)
  //   #define PLUTO_SDR
  // #endif

  // ARM NEON SIMD optimizations (controlled by CMake -DUSE_NEON=ON/OFF)
  // Recommended for PlutoSDR (Zynq7000 Cortex-A9) and RaspberryPi 2+

  // Text UI (controlled by CMake -DUSE_TEXT_UI=ON/OFF)
  // Requires ncurses library

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

// ==================== UDP Modem Transport ====================
// UDP-based modem communication (alternative to virtual PTY)
//
// Advantages over PTY:
// - No MMDVMHost code modification required (works with stock MMDVMHost)
// - Network-transparent (modem can be on different machine)
// - Easy debugging with standard network tools (tcpdump, wireshark)
// - Better error handling and reconnection logic
//
// Configuration:
// - Uncomment USE_UDP_MODEM to enable UDP transport
// - Configure addresses and ports below
// - Update MMDVMHost MMDVM.ini with matching UDP settings
//
// When USE_UDP_MODEM is NOT defined, traditional PTY mode is used
// (PTY mode requires MMDVMHost RTS patch)

// Uncomment to enable UDP modem transport
// #define USE_UDP_MODEM

#ifdef USE_UDP_MODEM
  // MMDVMHost connection (remote endpoint)
  #ifndef UDP_MODEM_ADDRESS
    #define UDP_MODEM_ADDRESS   "127.0.0.1"  // MMDVMHost IP address
  #endif

  #ifndef UDP_MODEM_PORT
    #define UDP_MODEM_PORT      3335         // MMDVMHost UDP port
  #endif

  // Local binding (this modem)
  #ifndef UDP_LOCAL_ADDRESS
    #define UDP_LOCAL_ADDRESS   "127.0.0.1"  // Local bind address
  #endif

  #ifndef UDP_LOCAL_PORT
    #define UDP_LOCAL_PORT      3334         // Local bind port
  #endif

  // Example configurations:
  //
  // Localhost (same machine):
  //   UDP_MODEM_ADDRESS  "127.0.0.1"
  //   UDP_MODEM_PORT     3335
  //   UDP_LOCAL_ADDRESS  "127.0.0.1"
  //   UDP_LOCAL_PORT     3334
  //
  // Network deployment (modem on Pi, host on server):
  //   UDP_MODEM_ADDRESS  "192.168.1.10"   // Server running MMDVMHost
  //   UDP_MODEM_PORT     3335
  //   UDP_LOCAL_ADDRESS  "192.168.1.100"  // This Raspberry Pi
  //   UDP_LOCAL_PORT     3334
  //
  // MMDVMHost MMDVM.ini configuration:
  //   [Modem]
  //   Protocol=udp
  //   ModemAddress=192.168.1.100  # This modem's IP
  //   ModemPort=3334              # This modem's port
  //   LocalAddress=192.168.1.10   # MMDVMHost's IP
  //   LocalPort=3335              # MMDVMHost's port
#endif

// ==================== Buffer Management ====================

// Standard MMDVM frame block size
// 720 samples = 30ms @ 24kHz sample rate
// Aligns with MMDVMHost frame timing and prevents missed starts
// Based on qradiolink optimization (commit 242705c)
#define MMDVM_FRAME_BLOCK_SIZE     720U

// TX Ring Buffer: ~300ms buffering (10 frames)
#define TX_RINGBUFFER_SIZE         (MMDVM_FRAME_BLOCK_SIZE * 10)  // 7200 samples

// RX Ring Buffer: ~266ms buffering (holds 2 DMR bursts)
// Increased from 4800 to 6400 to handle DMR duplex traffic
// Prevents buffer overflows during two-slot bursts
// Based on qradiolink optimization (commit 2398c56)
#define RX_RINGBUFFER_SIZE         6400U

// RSSI Buffer: Match RX buffer size
#define RSSI_RINGBUFFER_SIZE       RX_RINGBUFFER_SIZE

// ==================== Gain Controls ====================

// TX/RX gain in Q8 fixed-point format
// Q8 format: value = actual_gain * 128
// Examples:
//   128 = 1.0x = 0dB
//   256 = 2.0x = 6dB
//   640 = 5.0x = 14dB (matches qradiolink)
//   1024 = 8.0x = 18dB (maximum)

// Default TX gain: 5.0x = ~14dB
// Matches qradiolink's "sample *= 5" amplification
#define DEFAULT_TX_GAIN 640

// Default RX gain: 1.0x = 0dB
#define DEFAULT_RX_GAIN 128

// Per-mode TX gains (can be tuned for specific modes)
#define DSTAR_TX_GAIN   DEFAULT_TX_GAIN
#define DMR_TX_GAIN     DEFAULT_TX_GAIN
#define YSF_TX_GAIN     DEFAULT_TX_GAIN
#define P25_TX_GAIN     DEFAULT_TX_GAIN
#define NXDN_TX_GAIN    DEFAULT_TX_GAIN
#define FM_TX_GAIN      DEFAULT_TX_GAIN  // FM mode TX gain

// FM mode squelch threshold (0-32767, Q15 format)
// 328 = very sensitive (0.01), 1638 = medium (0.05), 3277 = high (0.10)
#define FM_SQUELCH_THRESHOLD 1638  // Medium sensitivity

#endif
