# POCSAG Implementation for MMDVM-SDR

**Date:** 2025-11-17
**Version:** 1.0
**Author:** MMDVM-SDR Integration

---

## Overview

This document describes the implementation of POCSAG (Post Office Code Standardisation Advisory Group) pager protocol support in the MMDVM-SDR project. POCSAG support has been ported from the upstream g4klx/MMDVM repository and adapted for the Linux/SDR environment.

## POCSAG Protocol Specifications

### Basic Parameters
- **Baud Rates:** 512, 1200, 2400 baud (1200 baud most common)
- **Modulation:** FSK (Frequency Shift Keying)
- **Error Correction:** BCH (31,21) code
- **Frame Structure:** Batch code sync with 16 batches per transmission
- **Addressing:** 21-bit address with 2-bit function code

### Protocol Constants
- **Sync Word:** 0x7CD215D8
- **Frame Length:** 68 bytes (17 x 32-bit words)
- **Preamble Length:** 72 bytes (18 x 32-bit words)
- **Symbol Length:** 20 samples per symbol at 24kHz baseband
- **Function Codes:** 0=Numeric, 1=Tone1, 2=Tone2, 3=Alphanumeric

## Implementation Files

### Core POCSAG Files

#### POCSAGDefines.h
Defines all POCSAG protocol constants and specifications.

**Location:** `/home/user/mmdvm-sdr/POCSAGDefines.h`

**Key Definitions:**
```cpp
const uint16_t POCSAG_FRAME_LENGTH_BYTES    = 68U;
const uint16_t POCSAG_PREAMBLE_LENGTH_BYTES = 72U;
const uint16_t POCSAG_RADIO_SYMBOL_LENGTH   = 20U;
const uint8_t  POCSAG_SYNC                  = 0xAAU;
const uint32_t POCSAG_SYNC_WORD             = 0x7CD215D8U;
```

#### POCSAGTX.h / POCSAGTX.cpp
POCSAG transmitter implementation.

**Location:** `/home/user/mmdvm-sdr/POCSAGTX.h`, `/home/user/mmdvm-sdr/POCSAGTX.cpp`

**Key Features:**
- Manual FIFO ring buffer (4000 bytes) for frame queuing
- Shaping filter for clean FSK modulation
- TX delay configuration
- Byte-level modulation with symbol mapping

**Public Methods:**
```cpp
uint8_t writeData(const uint8_t* data, uint16_t length);  // Write POCSAG frame
void writeByte(uint8_t c);                                 // Write single byte
void process();                                            // Process TX queue
void setTXDelay(uint8_t delay);                           // Set preamble delay
uint8_t getSpace() const;                                  // Get buffer space
bool busy();                                               // Check if transmitting
```

#### CalPOCSAG.h / CalPOCSAG.cpp
POCSAG calibration mode for transmitter testing.

**Location:** `/home/user/mmdvm-sdr/CalPOCSAG.h`, `/home/user/mmdvm-sdr/CalPOCSAG.cpp`

**Calibration States:**
- `POCSAGCAL_IDLE`: No calibration active
- `POCSAGCAL_TX`: Continuous 0xAA pattern transmission

## System Integration

### Globals.h
Added POCSAG state and global objects:

```cpp
enum MMDVM_STATE {
  // ... existing states ...
  STATE_POCSAG    = 6,      // POCSAG transmit mode
  STATE_POCSAGCAL = 100     // POCSAG calibration mode
};

extern bool m_pocsagEnable;   // POCSAG enable flag
extern CPOCSAGTX pocsagTX;    // POCSAG transmitter
extern CCalPOCSAG calPOCSAG;  // POCSAG calibration
```

### IO Layer Integration

#### IO.h
Added POCSAG TX level parameter:

```cpp
q15_t m_pocsagTXLevel;  // POCSAG transmit level

void setParameters(..., uint8_t pocsagTXLevel, ...);
```

#### IO.cpp
Integrated POCSAG TX level handling:

```cpp
case STATE_POCSAG:
  txLevel = m_pocsagTXLevel;
  break;
```

### Main Loop (MMDVM.cpp)
Added POCSAG processing:

```cpp
bool m_pocsagEnable = true;
CPOCSAGTX  pocsagTX;
CCalPOCSAG calPOCSAG;

// In loop():
if (m_pocsagEnable && m_modemState == STATE_POCSAG)
  pocsagTX.process();

if (m_modemState == STATE_POCSAGCAL)
  calPOCSAG.process();
```

### Serial Protocol (SerialPort.cpp)

#### Protocol Support
- **Protocol Version:** Compatible with both V1 and V2
- **Command Code:** 0x50 (MMDVM_POCSAG_DATA)
- **Capability Flag:** CAP_POCSAG (0x20)

#### Configuration
POCSAG enable flag in SET_CONFIG command (byte 1, bit 5):

```cpp
bool pocsagEnable = (data[1U] & 0x20U) == 0x20U;
```

#### Command Handling
```cpp
case MMDVM_POCSAG_DATA:
  if (m_pocsagEnable) {
    if (m_modemState == STATE_IDLE || m_modemState == STATE_POCSAG)
      err = pocsagTX.writeData(m_buffer + 3U, m_len - 3U);
  }
  if (err == 0U) {
    if (m_modemState == STATE_IDLE)
      setMode(STATE_POCSAG);
  }
  break;
```

## Signal Processing

### Modulation Path

```
POCSAG Frame (68 bytes)
    ↓
FIFO Buffer (4000 bytes)
    ↓
Byte Modulator → Symbol Mapping (20 samples/symbol)
    ↓
Shaping Filter (6-tap FIR)
    ↓
IO Write (STATE_POCSAG, samples, length)
    ↓
TX Level Scaling (m_pocsagTXLevel)
    ↓
SDR Output (via FM modulation & resampler)
```

### Symbol Mapping
- **Logic 1:** +1700 level (20 samples)
- **Logic 0:** -1700 level (20 samples)
- **Shaping:** 6-tap FIR filter for bandwidth limiting

### Timing
- **Baseband Rate:** 24 kHz
- **Symbol Rate (1200 baud):** 1200 symbols/sec
- **Samples per Symbol:** 20 (24000 / 1200)
- **Byte Time:** 6.67 ms (8 symbols × 833 µs/symbol)

## Configuration

### MMDVMHost Configuration

In `MMDVM.ini`:

```ini
[Modem]
Port=/path/to/ttyMMDVM0
Protocol=2                # Use Protocol V2 for POCSAG support

[POCSAG]
Enable=1                  # Enable POCSAG mode
Frequency=439987500       # POCSAG frequency (Hz)
TXLevel=50                # TX level (0-100, default 50)
```

### TX Level Calibration

POCSAG TX level is configurable via SET_CONFIG command:
- **Range:** 0-100
- **Default:** 50 (if not provided in extended config)
- **Scaling:** Multiplied by 128 for q15_t format

### TX Delay Configuration

Preamble length calculation:
```cpp
m_txDelay = POCSAG_PREAMBLE_LENGTH_BYTES + (delay * 3U) / 2U;
if (m_txDelay > 150U)
  m_txDelay = 150U;
```

## Testing & Calibration

### Calibration Mode (STATE_POCSAGCAL)

Continuous 0xAA pattern transmission for:
- Frequency deviation calibration
- Power level adjustment
- Modulation quality testing

**Activation:**
```cpp
calPOCSAG.write(&enable, 1U);  // enable = 1 for TX, 0 for IDLE
```

### Verification Checklist

- [ ] POCSAG frames queued without errors
- [ ] TX buffer space reported correctly
- [ ] Mode switching (IDLE ↔ POCSAG) works
- [ ] Calibration mode generates continuous pattern
- [ ] TX level scaling applied correctly
- [ ] No buffer overflows during transmission

## Known Limitations

1. **Transmit Only:** No POCSAG receiver implemented (POCSAG is TX-only protocol)
2. **Single Baud Rate:** Currently supports 1200 baud (20 samples/symbol at 24kHz)
3. **Fixed Deviation:** Uses system-wide FM deviation setting
4. **Protocol V1 Compatibility:** Full support requires Protocol V2

## Compatibility with Upstream

### Differences from g4klx/MMDVM

| Feature | Upstream | MMDVM-SDR |
|---------|----------|-----------|
| **Ring Buffer** | Template `CRingBuffer<uint8_t>` | Manual FIFO implementation |
| **Compilation** | `#if defined(MODE_POCSAG)` | Unconditional compilation |
| **Platform** | STM32/Arduino | Linux/SDR |
| **I/O Model** | Interrupt-driven | Pthread multi-threaded |

### Maintained Compatibility

- Frame format and length (68 bytes)
- Protocol command code (0x50)
- Sync pattern (0xAA preamble)
- Symbol timing (20 samples @ 24kHz)
- Modulation levels (±1700)

## Future Enhancements

### Potential Improvements

1. **Multi-Baud Support:**
   - 512 baud (46.875 samples/symbol)
   - 2400 baud (10 samples/symbol)
   - Runtime baud rate selection

2. **Protocol V2 Full Support:**
   - Enhanced status reporting
   - Extended configuration parameters
   - Capability negotiation

3. **Performance Optimization:**
   - NEON-optimized filtering
   - Direct memory access (DMA) where applicable
   - Reduced latency TX buffering

4. **Advanced Features:**
   - Automatic frequency correction
   - Transmit power ramping
   - Frame error statistics

## Troubleshooting

### Common Issues

**Issue:** POCSAG data rejected with NAK
- **Cause:** Frame length != 68 bytes
- **Solution:** Verify MMDVMHost sends correct frame size

**Issue:** No POCSAG capability reported
- **Cause:** Protocol V1 in use
- **Solution:** Update to Protocol V2 or check `CAP_POCSAG` flag manually

**Issue:** Buffer overflow errors
- **Cause:** TX queue full (4000 bytes)
- **Solution:** Reduce transmission rate or increase FIFO size

**Issue:** Modulation quality poor
- **Cause:** Incorrect TX level or deviation
- **Solution:** Use calibration mode to adjust settings

## References

### POCSAG Protocol
- CCIR Radiopaging Code No. 1 (RPC1)
- ITU-R M.584-2 specification
- British Post Office specification

### Upstream Repository
- **URL:** https://github.com/g4klx/MMDVM
- **Files:** POCSAGTX.cpp, POCSAGTX.h, CalPOCSAG.cpp, CalPOCSAG.h
- **Version:** 20240113 (Protocol Version 2)

### MMDVM-SDR Documentation
- `/home/user/mmdvm-sdr/docs/upstream/UPSTREAM_COMPARISON.md`
- `/home/user/mmdvm-sdr/POCSAGDefines.h`

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-17 | Initial POCSAG implementation |

---

**Implementation Status:** ✅ Complete
**Protocol Support:** V1 (Full), V2 (Partial - POCSAG only)
**Testing Status:** ⚠️ Requires validation with POCSAG encoder

---
