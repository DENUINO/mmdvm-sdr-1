# FM Mode Implementation for MMDVM-SDR

**Date:** 2025-11-17
**Author:** MMDVM-SDR Contributors
**Version:** 1.0

## Overview

This document describes the implementation of FM (Frequency Modulation) mode for the MMDVM-SDR project. Unlike the complex FM repeater implementation in the upstream g4klx/MMDVM repository, this implementation is simplified for SDR use and focuses on basic FM voice communication suitable for analog repeater and simplex operation.

## Architecture

### Design Philosophy

The FM mode implementation follows the existing MMDVM-SDR pattern used for digital modes (D-Star, DMR, P25, etc.) rather than implementing the full upstream FM repeater functionality. This provides:

1. **Simplicity** - No complex state machines for repeater operation
2. **SDR Focus** - Designed for software-defined radio baseband processing
3. **Consistency** - Follows the same RX/TX pattern as other modes
4. **Integration** - Works seamlessly with existing IO layer and protocol

### Key Differences from Upstream

| Feature | Upstream g4klx/MMDVM | MMDVM-SDR Implementation |
|---------|---------------------|-------------------------|
| **Purpose** | Hardware FM repeater | SDR FM transceiver |
| **Complexity** | Full repeater state machine | Simple RX/TX passthrough |
| **CTCSS** | RX detection & TX generation | Not implemented (future) |
| **Keyer** | CW ID, acks, timeout tones | Not implemented (future) |
| **Timers** | Hang time, timeout, kerchunk | Basic timeout only |
| **Filters** | Multiple cascaded stages | De-emphasis/pre-emphasis only |
| **I/O** | Hardware ADC/DAC | 24 kHz SDR baseband |

## File Structure

### Core FM Mode Files

```
mmdvm-sdr/
├── FMDefines.h       # FM mode constants and parameters
├── FMRX.h           # FM receiver header
├── FMRX.cpp         # FM receiver implementation
├── FMTX.h           # FM transmitter header
└── FMTX.cpp         # FM transmitter implementation
```

### Modified System Files

```
mmdvm-sdr/
├── Globals.h        # Added STATE_FM, m_fmEnable, fmRX, fmTX
├── Config.h         # Added FM configuration parameters
├── IO.h             # Added m_fmTXLevel, setFMInt()
└── IO.cpp           # Added FM TX level handling
```

## Component Details

### 1. FMDefines.h

Defines constants and parameters for FM mode operation:

**Key Constants:**
- `FM_SAMPLE_RATE` = 24000 Hz (MMDVM baseband rate)
- `FM_FRAME_LENGTH_SAMPLES` = 480 samples (20 ms frames)
- `FM_MAX_DEVIATION` = 5000 Hz (NBFM)
- `FM_SQUELCH_THRESHOLD_DEFAULT` = 1638 (Q15, medium sensitivity)

**Audio Parameters:**
- De-emphasis time constant: 530 µs (North America standard)
- Pre-emphasis time constant: 530 µs
- Audio bandwidth: 300 Hz to 3000 Hz

### 2. FMRX - FM Receiver

**Class: `CFMRX`**

Processes 24 kHz baseband audio samples from the IO layer.

**Key Features:**
- Squelch-based carrier detection
- De-emphasis filter for NBFM reception
- DC blocking filter
- Audio level control
- RSSI tracking

**State Machine:**
```
FMRXS_NONE       -> FMRXS_LISTENING -> FMRXS_AUDIO
     ^                                      |
     +--------------------------------------+
                (squelch closes)
```

**Signal Processing Chain:**
```
Input Samples (Q15, 24kHz)
    ↓
DC Blocker (1st order IIR HPF)
    ↓
De-emphasis Filter (1st order IIR LPF, 530µs)
    ↓
Audio Gain (Q15 multiply)
    ↓
Soft Limiting (prevent clipping)
    ↓
Output to Serial Protocol
```

**Methods:**
- `samples()` - Process incoming audio samples
- `reset()` - Reset receiver state
- `setSquelch()` - Configure squelch threshold
- `setGain()` - Set audio gain
- `setDeemphasis()` - Enable/disable de-emphasis filter

### 3. FMTX - FM Transmitter

**Class: `CFMTX`**

Accepts audio data from the host and generates 24 kHz baseband samples for transmission.

**Key Features:**
- Circular buffer for audio samples
- Pre-emphasis filter for NBFM transmission
- DC blocking filter
- Audio gain control
- Timeout protection

**State Machine:**
```
FMTXS_IDLE -> FMTX_AUDIO -> FMTXS_SHUTDOWN -> FMTXS_IDLE
    ^                                             |
    +---------------------------------------------+
```

**Signal Processing Chain:**
```
Input Samples from Host (Q15)
    ↓
Audio Gain (Q15 multiply)
    ↓
Pre-emphasis Filter (1st order IIR HPF, 530µs)
    ↓
DC Blocker (1st order IIR HPF)
    ↓
Soft Limiting (prevent over-deviation)
    ↓
Output to IO Layer (24kHz baseband)
```

**Methods:**
- `getSamples()` - Get samples for transmission
- `writeData()` - Write audio data from host
- `reset()` - Reset transmitter state
- `setPreemphasis()` - Enable/disable pre-emphasis
- `setGain()` - Set audio gain
- `setTimeout()` - Configure timeout protection

## Integration with MMDVM-SDR

### 1. Global State

**Globals.h additions:**
```cpp
enum MMDVM_STATE {
  // ...
  STATE_FM = 7,    // Added FM state
  // ...
};

extern bool m_fmEnable;  // FM mode enable flag
extern CFMRX fmRX;       // FM receiver instance
extern CFMTX fmTX;       // FM transmitter instance
```

### 2. Configuration

**Config.h additions:**
```cpp
// FM mode audio filtering
#define FM_AUDIO_FILTER

// Per-mode TX gains
#define FM_TX_GAIN DEFAULT_TX_GAIN  // 640 (5.0x = 14dB)

// FM squelch threshold
#define FM_SQUELCH_THRESHOLD 1638   // Medium sensitivity
```

### 3. IO Layer Integration

**IO.h/IO.cpp modifications:**

**Added member:**
```cpp
q15_t m_fmTXLevel;  // FM TX gain level
```

**Updated setParameters():**
```cpp
void setParameters(..., uint8_t fmTXLevel, ...);
```

**Added write() case:**
```cpp
case STATE_FM:
  txLevel = m_fmTXLevel;
  break;
```

### 4. Main Loop Integration

The main MMDVM loop (MMDVM.cpp) will need to:

1. Call `fmRX.samples()` during RX processing
2. Call `fmTX.getSamples()` during TX processing
3. Handle `STATE_FM` mode switching
4. Process FM data from serial protocol

**Example integration (pseudocode):**
```cpp
void loop() {
  // ... existing code ...

  // FM RX processing
  if (m_fmEnable && io.getSpace() > FM_FRAME_LENGTH_SAMPLES) {
    q15_t samples[FM_FRAME_LENGTH_SAMPLES];
    uint16_t rssi[FM_FRAME_LENGTH_SAMPLES];

    // Get samples from IO layer
    io.getSamples(samples, rssi, FM_FRAME_LENGTH_SAMPLES);

    // Process through FM RX
    fmRX.samples(samples, rssi, FM_FRAME_LENGTH_SAMPLES);
  }

  // FM TX processing
  if (m_modemState == STATE_FM && fmTX.hasData()) {
    q15_t samples[FM_AUDIO_BLOCK_SIZE];
    uint8_t count = fmTX.getSamples(samples, FM_AUDIO_BLOCK_SIZE);

    if (count > 0) {
      io.write(STATE_FM, samples, count);
    }
  }
}
```

## SDR Specifics

### Baseband vs RF

The FM mode operates at the **24 kHz baseband level**, not at RF:

```
SDR Flow (Receive):
RF → SDR RX → FM Demod (IQ→Audio) → Resampler → 24kHz Baseband
                                                       ↓
                                                   FMRX.samples()
                                                       ↓
                                                  Serial Protocol

SDR Flow (Transmit):
Serial Protocol
    ↓
FMTX.getSamples()
    ↓
24kHz Baseband → Resampler → FM Mod (Audio→IQ) → SDR TX → RF
```

**Note:** The actual FM modulation/demodulation (IQ conversion) is handled by:
- **FMModem.h/cpp** - For SDR I/Q conversion (when STANDALONE_MODE is defined)
- **GNU Radio flowgraph** - For external SDR processing
- **PlutoSDR** - For direct SDR hardware access

This FM mode implementation works with **already-demodulated audio** at baseband.

## Audio Processing

### De-emphasis Filter (RX)

**Purpose:** Roll off high frequencies boosted during FM transmission
**Type:** 1st order IIR low-pass filter
**Time Constant:** 530 µs (North America standard)
**Implementation:**
```cpp
y[n] = alpha * y[n-1] + (1-alpha) * x[n]
where alpha = exp(-1/(tau * fs)) ≈ 0.924
```

### Pre-emphasis Filter (TX)

**Purpose:** Boost high frequencies to improve SNR
**Type:** 1st order IIR high-pass characteristic
**Time Constant:** 530 µs
**Implementation:**
```cpp
y[n] = (1-alpha) * y[n-1] + alpha * x[n] + 50% boost
where alpha = 1 - exp(-1/(tau * fs)) ≈ 0.076
```

### DC Blocking Filter

**Purpose:** Remove DC offset from audio
**Type:** 1st order IIR high-pass filter
**Alpha:** 0.95
**Implementation:**
```cpp
y[n] = alpha * y[n-1] + (1-alpha) * x[n]
DC_out = (y[n-1] + y[n]) / 2
output = input - DC_out
```

### Squelch

**Type:** Level-based squelch with hysteresis
**Threshold:** Configurable (Q15 format, 0-32767)
**Hang Time:** 100 ms (5 frames at 20 ms/frame)
**Audio Level Tracking:** Exponential moving average with attack/decay

## Protocol Integration

### Serial Protocol Support

The FM mode will integrate with the MMDVM serial protocol once protocol support is added:

**Commands (future implementation):**
- `CMD_FM_PARAMS1` (0x60) - FM configuration
- `CMD_FM_PARAMS2` (0x61) - Extended parameters
- `CMD_FM_DATA` (0x65) - FM audio data
- `CMD_FM_STATUS` (0x66) - FM status reporting
- `CMD_FM_EOT` (0x67) - End of transmission

**Note:** Current implementation has stub protocol integration. Full protocol support requires updates to SerialPort.cpp/h.

## Configuration Parameters

### Compile-Time Configuration

**Config.h parameters:**
```cpp
#define FM_AUDIO_FILTER           // Enable de-emphasis/pre-emphasis
#define FM_TX_GAIN 640            // TX gain (5.0x = 14dB)
#define FM_SQUELCH_THRESHOLD 1638 // Squelch sensitivity
```

### Runtime Configuration

**Via setParameters():**
- FM TX level (0-255, scaled to Q15)
- Invert TX (polarity inversion)
- DC offset compensation

**Via FM-specific methods:**
- `fmRX.setSquelch()` - Adjust squelch threshold
- `fmRX.setGain()` - Set RX audio gain
- `fmTX.setGain()` - Set TX audio gain
- `fmRX.setDeemphasis()` - Enable/disable de-emphasis
- `fmTX.setPreemphasis()` - Enable/disable pre-emphasis

## Performance Considerations

### CPU Usage

**Processing per 20 ms frame (480 samples):**
- DC blocking: ~480 multiply-add operations
- De-emphasis/Pre-emphasis: ~480 multiply-add operations
- Audio gain: ~480 multiplications
- Squelch level tracking: ~480 operations
- **Total:** ~2000-3000 operations per 20 ms = **100-150k ops/sec**

**Compared to digital modes:**
- FM: ~100-150k ops/sec
- DMR: ~500k ops/sec (includes FEC, interleaving)
- P25: ~800k ops/sec (includes vocoder simulation)

FM mode is **less CPU-intensive** than digital modes.

### Memory Usage

**Static allocation:**
- RX buffer: 960 samples * 2 bytes = 1.9 KB
- TX buffer: 960 samples * 2 bytes = 1.9 KB
- Filter states: ~40 bytes
- **Total:** ~4 KB per FM instance

## Testing

### Unit Tests

**Recommended tests (not yet implemented):**
1. Filter frequency response verification
2. Squelch threshold accuracy
3. Audio level tracking
4. Buffer management (overflow/underflow)
5. TX timeout functionality

### Integration Tests

**Test scenarios:**
1. **RX Path:** SDR RX → FM Demod → FMRX → Serial → MMDVMHost
2. **TX Path:** MMDVMHost → Serial → FMTX → FM Mod → SDR TX
3. **Loopback:** TX → RX with varying signal levels
4. **Squelch:** Verify open/close thresholds and hang time
5. **Timeout:** Verify TX timeout protection

### Real-World Testing

**Test with:**
- Analog FM repeaters (de-emphasis/pre-emphasis verification)
- Simplex FM contacts (audio quality assessment)
- Varying signal strengths (squelch performance)
- Extended transmissions (timeout and buffer stability)

## Future Enhancements

### Short-Term

1. **Protocol Integration**
   - Implement SerialPort FM commands
   - Add FM status reporting
   - Test with MMDVMHost FM mode

2. **Audio Improvements**
   - Adjustable bandwidth filters (300-3000 Hz)
   - Noise reduction
   - AGC (Automatic Gain Control)

### Medium-Term

3. **CTCSS Support**
   - CTCSS tone detection (RX)
   - CTCSS tone generation (TX)
   - Configurable tone frequencies

4. **Advanced Features**
   - Voice compander
   - Deviation limiting
   - Audio level metering

### Long-Term

5. **Repeater Functionality**
   - Hang time support
   - Timeout timers
   - CW ID generation
   - Courtesy tones

6. **Digital Squelch**
   - DCS (Digital Coded Squelch)
   - Selective calling

## Troubleshooting

### Common Issues

**Problem:** No audio output on RX
**Solutions:**
- Check squelch threshold (try lowering)
- Verify de-emphasis is enabled
- Check RX gain setting
- Verify SDR is properly demodulating FM

**Problem:** Distorted TX audio
**Solutions:**
- Reduce TX gain to prevent over-deviation
- Enable pre-emphasis filter
- Check for clipping in audio chain
- Verify FM deviation setting on SDR

**Problem:** High CPU usage
**Solutions:**
- Disable debug logging
- Reduce frame size if modified
- Check for buffer overflows
- Verify filter implementations

**Problem:** Squelch chattering
**Solutions:**
- Increase hang time
- Adjust threshold to account for noise floor
- Check audio level tracking parameters

## References

### Related Files

- `FMModem.h/cpp` - SDR I/Q FM modulation/demodulation
- `Resampler.h/cpp` - Sample rate conversion (SDR ↔ 24kHz)
- `IO.h/cpp` - I/O layer integration
- `SerialPort.h/cpp` - Protocol handling (future FM support)

### Standards

- **NBFM:** Narrowband FM (±5 kHz deviation)
- **De-emphasis:** 530 µs (North America), 750 µs (Europe - not implemented)
- **CTCSS:** Sub-audible tones 67.0 - 254.1 Hz (future)
- **Audio Bandwidth:** 300 Hz - 3000 Hz (standard voice)

### Upstream References

- g4klx/MMDVM FM.cpp/h - Full repeater implementation
- g4klx/MMDVM FMCTCSSRX.cpp/h - CTCSS detection reference
- g4klx/MMDVM FMCTCSSTX.cpp/h - CTCSS generation reference

## Changelog

**v1.0 - 2025-11-17**
- Initial FM mode implementation
- Basic RX/TX with de-emphasis/pre-emphasis
- Squelch-based carrier detection
- Integration with MMDVM-SDR infrastructure

---

**Document Maintainer:** MMDVM-SDR Contributors
**Last Updated:** 2025-11-17
**Status:** Active Development
