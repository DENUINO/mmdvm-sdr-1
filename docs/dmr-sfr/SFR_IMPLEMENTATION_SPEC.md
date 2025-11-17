# DMR Single Frequency Repeater - Implementation Specification

**Document Version:** 1.0
**Date:** 2025-11-16
**Project:** mmdvm-sdr DMR SFR Implementation

---

## Executive Summary

This document provides detailed implementation specifications for DMR Single Frequency Repeater (SFR) functionality in the mmdvm-sdr project. Based on standards analysis, commercial product research, technical requirements, and feasibility study, this specification defines the architecture, algorithms, interfaces, and implementation approach.

**Implementation Strategy**: Phased development with incremental feature addition and validation.

**Target Platform**: PlutoSDR (AD9363) with ARM Cortex-A9 dual-core processing.

**Target Performance**: 5-10 km range with 1-5W external PA, comparable to commercial handheld SFR products.

---

## 1. System Architecture

### 1.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        DMR SFR SYSTEM                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐  │
│  │   Antenna    │◄────►│   AD9363 RF  │◄────►│  Zynq FPGA   │  │
│  │              │      │  Transceiver │      │   (DDC/DUC)  │  │
│  └──────────────┘      └──────────────┘      └──────┬───────┘  │
│                                                      │          │
│                                              ┌───────▼───────┐  │
│                                              │   ARM CPU     │  │
│                                              │  Processing   │  │
│                                              └───────┬───────┘  │
│                                                      │          │
│  ┌───────────────────────────────────────────────────▼────────┐│
│  │               DMR SFR SOFTWARE STACK                       ││
│  ├────────────────────────────────────────────────────────────┤│
│  │  Timeslot 1 (RX)          │          Timeslot 2 (TX)      ││
│  │                           │                                ││
│  │  RX Path:                 │          TX Path:              ││
│  │  - Demodulator            │          - Modulator           ││
│  │  - Frame Sync             │          - Frame Assembly      ││
│  │  - Slot Decoder           │          - Slot Encoder        ││
│  │  - AMBE+2 Decoder         │          - AMBE+2 Encoder      ││
│  │                           │                                ││
│  │         Audio Buffers ◄───┴───► Echo Cancellation          ││
│  │                                                             ││
│  │         Timing Synchronization (PLL)                       ││
│  │         DMR Protocol Stack                                 ││
│  │         Configuration & Control                            ││
│  └────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 Software Component Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Application Layer                     │
│                 (Configuration, Logging)                │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│                    SFR Controller                       │
│              (State Machine, Coordination)              │
└──────────┬───────────────────────────────┬──────────────┘
           │                               │
┌──────────▼──────────┐         ┌──────────▼──────────────┐
│   RX Path Thread    │         │   TX Path Thread        │
│   (Core 0)          │         │   (Core 1)              │
│                     │         │                         │
│ - DMR Demodulator   │         │ - DMR Modulator         │
│ - Frame Decoder     │         │ - Frame Encoder         │
│ - AMBE Decoder      │         │ - AMBE Encoder          │
└──────────┬──────────┘         └──────────▲──────────────┘
           │                               │
           │      ┌────────────────────┐   │
           └─────►│  Audio Buffer &    │───┘
                  │ Echo Cancellation  │
                  └────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│              Timing Synchronization                     │
│          (Software PLL, Timeslot Scheduler)             │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│                  Hardware Interface                     │
│          (PlutoSDR IIO, GPIO, Timers)                   │
└─────────────────────────────────────────────────────────┘
```

### 1.3 Data Flow

**Receive Path (Timeslot 1: 0-30 ms)**:
```
Antenna → AD9363 → FPGA (DDC) → ARM CPU → Demodulator →
Frame Decoder → AMBE Decoder → PCM Audio → Audio Buffer
```

**Processing (30-60 ms)**:
```
Audio Buffer → Echo Cancellation → PCM Audio →
AMBE Encoder → Frame Encoder
```

**Transmit Path (Timeslot 2: 60-90 ms)**:
```
Frame Encoder → Modulator → ARM CPU → FPGA (DUC) →
AD9363 → Antenna
```

---

## 2. Timing and Synchronization

### 2.1 Timeslot Structure

**TDMA Frame Format** (60 ms):
```
Frame:
├─ Timeslot 1 (0-30 ms):    RX slot
│  ├─ Data: 0-27.5 ms
│  └─ Guard: 27.5-30 ms
│
└─ Timeslot 2 (30-60 ms):   TX slot
   ├─ Guard: 30-32.5 ms
   ├─ Data: 32.5-57.5 ms
   └─ Guard: 57.5-60 ms
```

**Critical Timing Points**:
- **0 ms**: RX slot start, RX active
- **27.5 ms**: RX data end, prepare to switch
- **30 ms**: RX slot end, timeslot boundary
- **32.5 ms**: TX data start
- **57.5 ms**: TX data end
- **60 ms**: TX slot end, frame boundary, loop to 0 ms

### 2.2 Software PLL Implementation

**Objective**: Lock to received DMR signal and maintain precise timeslot timing.

#### 2.2.1 PLL Algorithm

**Components**:
1. **Phase Detector**: Detect DMR sync pattern, measure time offset
2. **Loop Filter**: Low-pass filter to smooth phase measurements
3. **VCO (Numerically Controlled)**: Adjust timeslot timer based on filtered phase

**Pseudocode**:
```c
// PLL State
struct DMR_PLL {
    int64_t timeslot_period_ns;    // Nominal: 30,000,000 ns
    int64_t current_phase_ns;      // Current phase offset
    int64_t target_phase_ns;       // Target phase (0 = aligned)
    float integral_error;          // Integrator for PI controller
    float proportional_gain;       // Kp
    float integral_gain;           // Ki
};

// Phase Detector
int64_t detect_sync_phase(dmr_frame_t *frame) {
    // Search for DMR sync pattern (BS Sourced Voice: 0x755FD7DF75F7)
    int sync_position = find_sync_pattern(frame->data);

    if (sync_position >= 0) {
        // Measure actual time of sync detection
        int64_t actual_time_ns = get_high_res_time();

        // Expected time based on current timeslot
        int64_t expected_time_ns = pll.current_timeslot_start;

        // Phase error
        return actual_time_ns - expected_time_ns;
    }

    return INT64_MAX; // No sync found
}

// Loop Filter (PI Controller)
void update_pll(int64_t phase_error_ns) {
    if (phase_error_ns == INT64_MAX) return; // No update

    // Proportional term
    float correction_p = pll.proportional_gain * phase_error_ns;

    // Integral term
    pll.integral_error += phase_error_ns;
    float correction_i = pll.integral_gain * pll.integral_error;

    // Total correction
    float correction = correction_p + correction_i;

    // Update timeslot period
    pll.timeslot_period_ns -= (int64_t)correction;

    // Clamp to reasonable range (±100 ppm)
    const int64_t min_period = 29970000; // -1000 ppm
    const int64_t max_period = 30030000; // +1000 ppm
    pll.timeslot_period_ns = CLAMP(pll.timeslot_period_ns,
                                   min_period, max_period);
}
```

**PLL Parameters**:
- **Kp (Proportional Gain)**: 0.01 (fast response)
- **Ki (Integral Gain)**: 0.0001 (slow tracking)
- **Natural Frequency**: ~1 Hz (smooth tracking)
- **Damping Factor**: 0.707 (critically damped)

#### 2.2.2 Hardware Timer Configuration

**ARM Generic Timer** (Zynq 7010):
- Resolution: 1 μs (1 MHz counter)
- Width: 64-bit (no overflow concerns)
- Interrupt generation: On compare match

**Timer Interrupt Handler**:
```c
void timeslot_timer_isr(void) {
    static int current_slot = 0;

    // Determine current timeslot
    if (current_slot == 0) {
        // Timeslot 1 (RX) just ended, start TX
        start_tx_slot();
        current_slot = 1;
    } else {
        // Timeslot 2 (TX) just ended, start RX
        start_rx_slot();
        current_slot = 0;
    }

    // Schedule next interrupt
    uint64_t next_interrupt = get_timer() + pll.timeslot_period_ns / 1000;
    set_timer_compare(next_interrupt);
}
```

### 2.3 TX/RX Switching

**Switching Sequence**:

**RX → TX (at 30 ms boundary)**:
```c
void switch_rx_to_tx(void) {
    // 1. Stop RX (disable AGC, ADC path)
    ad9363_rx_disable();

    // 2. Configure for TX
    ad9363_tx_enable();

    // 3. Ramp up PA (if external)
    gpio_set(PA_ENABLE_PIN, HIGH);

    // 4. Start TX modulator
    tx_modulator_start();

    // Total time: <500 μs (well within 2.5 ms guard time)
}
```

**TX → RX (at 60 ms boundary)**:
```c
void switch_tx_to_rx(void) {
    // 1. Stop TX modulator
    tx_modulator_stop();

    // 2. Ramp down PA
    gpio_set(PA_ENABLE_PIN, LOW);

    // 3. Disable TX
    ad9363_tx_disable();

    // 4. Configure for RX
    ad9363_rx_enable();

    // 5. Reset AGC
    ad9363_agc_reset();

    // Total time: <500 μs
}
```

**Timing Verification**:
- Use GPIO toggle to mark switching events
- Oscilloscope measurement: Verify <1 ms switching time
- Ensure no glitches during transition

---

## 3. DMR Modulation and Demodulation

### 3.1 4-FSK Modulation

**DMR Uses 4-level FSK**:
- Symbol rate: 4,800 symbols/s
- Bit rate: 9,600 bit/s (2 bits per symbol)
- Deviation: ±648 Hz (4-level, relative to carrier)

**Symbol Mapping**:
| Dibits | Symbol | Frequency Offset |
|--------|--------|-----------------|
| 01     | +3     | +1,944 Hz       |
| 00     | +1     | +648 Hz         |
| 10     | -1     | -648 Hz         |
| 11     | -3     | -1,944 Hz       |

#### 3.1.1 Modulator Implementation

**Approach**: Generate baseband 4-FSK, upconvert in FPGA/AD9363

**Algorithm**:
```c
// 4-FSK Modulator
void modulate_4fsk(uint8_t *bits, int num_bits,
                   float complex *output, int sample_rate) {
    const float symbol_rate = 4800.0f;
    const float samples_per_symbol = sample_rate / symbol_rate;
    const float freq_deviation = 648.0f; // Hz

    float phase = 0.0f;
    int out_idx = 0;

    for (int i = 0; i < num_bits; i += 2) {
        // Map dibits to symbol
        uint8_t dibit = (bits[i] << 1) | bits[i+1];
        int symbol;

        switch (dibit) {
            case 0b01: symbol = +3; break;
            case 0b00: symbol = +1; break;
            case 0b10: symbol = -1; break;
            case 0b11: symbol = -3; break;
        }

        // Generate samples for this symbol
        float frequency = symbol * freq_deviation;

        for (int s = 0; s < samples_per_symbol; s++) {
            // Update phase
            phase += 2.0f * M_PI * frequency / sample_rate;

            // Wrap phase to [-π, π]
            while (phase > M_PI) phase -= 2.0f * M_PI;
            while (phase < -M_PI) phase += 2.0f * M_PI;

            // Generate complex sample
            output[out_idx++] = cosf(phase) + I * sinf(phase);
        }
    }
}
```

**Optimization**: Use NEON SIMD for complex arithmetic, pre-computed sine/cosine tables

#### 3.1.2 Demodulator Implementation

**Approach**: FM discriminator + symbol decision

**Algorithm**:
```c
// FM Discriminator
float fm_discriminator(float complex *samples, int num_samples,
                       float *output, float sample_rate) {
    for (int i = 1; i < num_samples; i++) {
        // Complex conjugate multiplication
        float complex diff = samples[i] * conjf(samples[i-1]);

        // Extract instantaneous frequency
        float phase_diff = atan2f(cimagf(diff), crealf(diff));

        // Convert to frequency (Hz)
        output[i-1] = phase_diff * sample_rate / (2.0f * M_PI);
    }
}

// Symbol Decision
void decide_symbols(float *frequencies, int num_symbols,
                    uint8_t *bits) {
    const float threshold1 = -1296.0f; // Between -3 and -1
    const float threshold2 = 0.0f;     // Between -1 and +1
    const float threshold3 = 1296.0f;  // Between +1 and +3

    for (int i = 0; i < num_symbols; i++) {
        float freq = frequencies[i];
        uint8_t dibit;

        if (freq < threshold1) {
            dibit = 0b11; // -3
        } else if (freq < threshold2) {
            dibit = 0b10; // -1
        } else if (freq < threshold3) {
            dibit = 0b00; // +1
        } else {
            dibit = 0b01; // +3
        }

        bits[2*i] = (dibit >> 1) & 1;
        bits[2*i+1] = dibit & 1;
    }
}
```

**Optimization**:
- NEON SIMD for vector operations
- Fixed-point arithmetic for embedded targets
- Symbol timing recovery (Gardner algorithm or early-late gate)

### 3.2 DMR Frame Structure

**DMR Timeslot (30 ms) Structure**:
```
┌─────────────────────────────────────────────────────────┐
│              DMR Timeslot (264 bits, 27.5 ms)           │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌────────┬────────┬────────┬────────┬────────┬─────┐  │
│  │ Sync   │ Voice/│ Sync   │ Voice/│ Sync   │Voice│  │
│  │ or     │ Data  │ or     │ Data  │ or     │Data │  │
│  │ CACH   │ Frame │ CACH   │ Frame │ CACH   │Frame│  │
│  │ (12b)  │ (108b)│ (12b)  │ (108b)│ (12b)  │(108b│  │
│  └────────┴────────┴────────┴────────┴────────┴─────┘  │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

**Voice Burst Structure** (Total: 264 bits):
- Bits 0-47: CACH (Common Announcement Channel) + Sync
- Bits 48-155: Voice Frame 1 (108 bits)
- Bits 156-203: Embedded Signaling + Sync
- Bits 204-263: Voice Frame 2 (108 bits)

**Sync Patterns**:
- **BS Sourced Voice**: 0x755FD7DF75F7 (48 bits)
- **BS Sourced Data**: 0xDFF57D75DF5D
- **MS Sourced Voice**: 0x7F7D5DD57DFD
- **MS Sourced Data**: 0xD5D7F77FD757

---

## 4. Vocoder Integration

### 4.1 AMBE+2 Vocoder

**DMR Vocoder Parameters**:
- **Mode**: AMBE+2 2450 bps voice + 1150 bps FEC
- **Frame Length**: 20 ms
- **Sample Rate**: 8 kHz
- **Input/Output**: 160 samples (20 ms × 8 kHz)
- **Encoded Frame**: 49 bits voice + 23 bits FEC = 72 bits total

#### 4.1.1 Hardware Codec Integration (DVSI)

**USB-3000 Dongle Interface**:

**Initialization**:
```c
// Open USB-3000 device
int ambe_fd = open("/dev/ttyUSB0", O_RDWR);

// Configure serial port (460,800 baud, 8N1)
struct termios tty;
tcgetattr(ambe_fd, &tty);
cfsetspeed(&tty, B460800);
tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit
tty.c_cflag &= ~(PARENB | PSTOP);           // No parity, 1 stop bit
tcsetattr(ambe_fd, TCSANOW, &tty);

// Send init packet to AMBE chip
uint8_t init_packet[] = {0x61, 0x00, 0x0D, 0x00, 0x0A,
                          0x00, 0x00, 0x30, 0x78, 0x08};
write(ambe_fd, init_packet, sizeof(init_packet));
```

**Encode**:
```c
// Encode PCM audio to AMBE
void ambe_encode(int16_t *pcm_in, uint8_t *ambe_out) {
    // Construct encode packet
    uint8_t packet[326];
    packet[0] = 0x61; // Start byte
    packet[1] = 0x01; // Command: encode
    packet[2] = 0x42; // Length MSB (320 + 6)
    packet[3] = 0x02; // Length LSB

    // Copy PCM samples (160 samples × 2 bytes)
    memcpy(&packet[4], pcm_in, 320);

    // Send to AMBE chip
    write(ambe_fd, packet, 326);

    // Read response (9 bytes header + 9 bytes AMBE data)
    uint8_t response[18];
    read(ambe_fd, response, 18);

    // Extract AMBE data (72 bits = 9 bytes)
    memcpy(ambe_out, &response[9], 9);
}
```

**Decode**:
```c
// Decode AMBE to PCM audio
void ambe_decode(uint8_t *ambe_in, int16_t *pcm_out) {
    // Construct decode packet
    uint8_t packet[15];
    packet[0] = 0x61; // Start byte
    packet[1] = 0x00; // Command: decode
    packet[2] = 0x00; // Length MSB
    packet[3] = 0x0B; // Length LSB (9 + 2)

    // Copy AMBE data
    memcpy(&packet[4], ambe_in, 9);

    // Send to AMBE chip
    write(ambe_fd, packet, 15);

    // Read response (326 bytes: 6 header + 320 PCM)
    uint8_t response[326];
    read(ambe_fd, response, 326);

    // Extract PCM samples
    memcpy(pcm_out, &response[6], 320);
}
```

#### 4.1.2 Software Codec Integration (md380_vocoder)

**Library Interface**:
```c
#include "md380_vocoder.h"

// Initialize
void *vocoder_state = md380_vocoder_init();

// Encode
void software_encode(int16_t *pcm_in, uint8_t *ambe_out) {
    md380_vocoder_encode(vocoder_state, pcm_in, ambe_out);
}

// Decode
void software_decode(uint8_t *ambe_in, int16_t *pcm_out) {
    md380_vocoder_decode(vocoder_state, ambe_in, pcm_out);
}

// Cleanup
void vocoder_cleanup(void) {
    md380_vocoder_free(vocoder_state);
}
```

**Note**: md380_vocoder licensing is unclear; use at your own risk.

### 4.2 Audio Buffering

**Ring Buffer Implementation**:
```c
typedef struct {
    int16_t *buffer;     // Audio samples
    int capacity;        // Total buffer size (samples)
    int write_ptr;       // Write position
    int read_ptr;        // Read position
    int available;       // Samples available to read
    pthread_mutex_t lock;
} audio_ringbuffer_t;

// Initialize
audio_ringbuffer_t* audio_ringbuffer_create(int capacity) {
    audio_ringbuffer_t *rb = malloc(sizeof(audio_ringbuffer_t));
    rb->buffer = malloc(capacity * sizeof(int16_t));
    rb->capacity = capacity;
    rb->write_ptr = 0;
    rb->read_ptr = 0;
    rb->available = 0;
    pthread_mutex_init(&rb->lock, NULL);
    return rb;
}

// Write samples
int audio_ringbuffer_write(audio_ringbuffer_t *rb,
                           int16_t *samples, int count) {
    pthread_mutex_lock(&rb->lock);

    int space = rb->capacity - rb->available;
    if (count > space) {
        // Buffer overflow, drop oldest samples
        rb->read_ptr = (rb->read_ptr + (count - space)) % rb->capacity;
        rb->available = rb->capacity;
    }

    for (int i = 0; i < count; i++) {
        rb->buffer[rb->write_ptr] = samples[i];
        rb->write_ptr = (rb->write_ptr + 1) % rb->capacity;
    }

    rb->available += count;
    if (rb->available > rb->capacity) rb->available = rb->capacity;

    pthread_mutex_unlock(&rb->lock);
    return count;
}

// Read samples
int audio_ringbuffer_read(audio_ringbuffer_t *rb,
                          int16_t *samples, int count) {
    pthread_mutex_lock(&rb->lock);

    if (count > rb->available) count = rb->available;

    for (int i = 0; i < count; i++) {
        samples[i] = rb->buffer[rb->read_ptr];
        rb->read_ptr = (rb->read_ptr + 1) % rb->capacity;
    }

    rb->available -= count;

    pthread_mutex_unlock(&rb->lock);
    return count;
}
```

**Buffer Size**:
- Capacity: 960 samples (120 ms at 8 kHz)
- Provides jitter buffer and processing headroom

---

## 5. Echo Cancellation Implementation

### 5.1 NLMS Adaptive Filter

**Algorithm**:
```c
typedef struct {
    float *coefficients;  // Filter coefficients (h)
    float *input_buffer;  // Circular buffer of input samples (x)
    int filter_length;    // Number of taps (N)
    float step_size;      // Normalized step size (μ)
    float regularization; // δ (prevent division by zero)
    int buffer_pos;       // Current position in input buffer
} nlms_filter_t;

// Initialize NLMS filter
nlms_filter_t* nlms_create(int filter_length,
                           float step_size,
                           float regularization) {
    nlms_filter_t *nlms = malloc(sizeof(nlms_filter_t));
    nlms->filter_length = filter_length;
    nlms->step_size = step_size;
    nlms->regularization = regularization;
    nlms->buffer_pos = 0;

    // Allocate and zero-initialize
    nlms->coefficients = calloc(filter_length, sizeof(float));
    nlms->input_buffer = calloc(filter_length, sizeof(float));

    return nlms;
}

// NLMS filter processing (single sample)
float nlms_process(nlms_filter_t *nlms,
                   float input,        // TX signal (reference)
                   float desired) {    // RX signal (desired + echo)
    // 1. Store input in circular buffer
    nlms->input_buffer[nlms->buffer_pos] = input;
    nlms->buffer_pos = (nlms->buffer_pos + 1) % nlms->filter_length;

    // 2. Compute filter output (estimate of echo)
    float output = 0.0f;
    int idx = nlms->buffer_pos;

    for (int i = 0; i < nlms->filter_length; i++) {
        idx = (idx - 1 + nlms->filter_length) % nlms->filter_length;
        output += nlms->coefficients[i] * nlms->input_buffer[idx];
    }

    // 3. Compute error signal
    float error = desired - output;

    // 4. Compute input power (normalization)
    float power = nlms->regularization;
    idx = nlms->buffer_pos;

    for (int i = 0; i < nlms->filter_length; i++) {
        idx = (idx - 1 + nlms->filter_length) % nlms->filter_length;
        float x = nlms->input_buffer[idx];
        power += x * x;
    }

    // 5. Update filter coefficients
    float step = nlms->step_size * error / power;
    idx = nlms->buffer_pos;

    for (int i = 0; i < nlms->filter_length; i++) {
        idx = (idx - 1 + nlms->filter_length) % nlms->filter_length;
        nlms->coefficients[i] += step * nlms->input_buffer[idx];
    }

    // 6. Return echo-cancelled signal
    return error;
}
```

### 5.2 NEON SIMD Optimization

**Optimized Power Calculation**:
```c
#ifdef __ARM_NEON
#include <arm_neon.h>

float compute_power_neon(float *buffer, int length) {
    float32x4_t sum_vec = vdupq_n_f32(0.0f);

    // Process 4 samples at a time
    int i;
    for (i = 0; i < length - 3; i += 4) {
        float32x4_t x = vld1q_f32(&buffer[i]);
        sum_vec = vmlaq_f32(sum_vec, x, x); // sum += x * x
    }

    // Horizontal sum
    float32x2_t sum_pair = vadd_f32(vget_low_f32(sum_vec),
                                    vget_high_f32(sum_vec));
    float sum = vget_lane_f32(sum_pair, 0) + vget_lane_f32(sum_pair, 1);

    // Handle remaining samples
    for (; i < length; i++) {
        sum += buffer[i] * buffer[i];
    }

    return sum;
}
#endif
```

**Expected Speedup**: 3-4× for vector operations

### 5.3 Echo Cancellation Integration

**Integration with Audio Path**:
```c
void sfr_audio_process(int16_t *tx_audio,    // TX signal (reference)
                       int16_t *rx_audio,    // RX signal (with echo)
                       int16_t *out_audio,   // Echo-cancelled output
                       int num_samples) {

    for (int i = 0; i < num_samples; i++) {
        // Convert to float
        float tx_sample = (float)tx_audio[i] / 32768.0f;
        float rx_sample = (float)rx_audio[i] / 32768.0f;

        // Apply NLMS echo cancellation
        float clean_sample = nlms_process(echo_canceller,
                                          tx_sample,
                                          rx_sample);

        // Convert back to int16
        out_audio[i] = (int16_t)(clean_sample * 32768.0f);
    }
}
```

**Parameters**:
- **Filter Length**: 512 taps
- **Step Size (μ)**: 0.3 (fast convergence)
- **Regularization (δ)**: 0.001

**Expected ERLE**: 40-50 dB after convergence (<500 ms)

---

## 6. DMR Protocol Implementation

### 6.1 Frame Encoding/Decoding

**DMR Frame Encoder**:
```c
typedef struct {
    uint8_t sync_pattern[6];      // 48-bit sync
    uint8_t voice_frame_1[14];    // 108-bit voice (rounded to bytes)
    uint8_t embedded_signaling[6];// 48-bit embedded LC
    uint8_t voice_frame_2[14];    // 108-bit voice
} dmr_voice_burst_t;

void dmr_encode_voice_burst(dmr_voice_burst_t *burst,
                            uint8_t *ambe_frame_1,
                            uint8_t *ambe_frame_2,
                            uint8_t *embedded_lc,
                            bool is_bs_sourced) {
    // Set sync pattern
    if (is_bs_sourced) {
        const uint8_t bs_sync[] = {0x75, 0x5F, 0xD7, 0xDF, 0x75, 0xF7};
        memcpy(burst->sync_pattern, bs_sync, 6);
    } else {
        const uint8_t ms_sync[] = {0x7F, 0x7D, 0x5D, 0xD5, 0x7D, 0xFD};
        memcpy(burst->sync_pattern, ms_sync, 6);
    }

    // Copy voice frames (with FEC encoding)
    dmr_fec_encode(ambe_frame_1, burst->voice_frame_1);
    dmr_fec_encode(ambe_frame_2, burst->voice_frame_2);

    // Embed signaling
    memcpy(burst->embedded_signaling, embedded_lc, 6);
}
```

**DMR Frame Decoder**:
```c
int dmr_decode_voice_burst(uint8_t *raw_bits,
                           dmr_voice_burst_t *burst,
                           uint8_t *ambe_frame_1,
                           uint8_t *ambe_frame_2) {
    // Detect and verify sync pattern
    if (!dmr_detect_sync(raw_bits, burst->sync_pattern)) {
        return -1; // Sync error
    }

    // Extract voice frames
    dmr_extract_bits(raw_bits, 48, 108, burst->voice_frame_1);
    dmr_extract_bits(raw_bits, 204, 108, burst->voice_frame_2);

    // FEC decode
    int errors1 = dmr_fec_decode(burst->voice_frame_1, ambe_frame_1);
    int errors2 = dmr_fec_decode(burst->voice_frame_2, ambe_frame_2);

    if (errors1 < 0 || errors2 < 0) {
        return -1; // FEC failure
    }

    return errors1 + errors2; // Total corrected errors
}
```

### 6.2 Forward Error Correction (FEC)

**DMR Uses Golay(20,8) and Hamming Codes**:

**Simplified FEC Encode** (actual implementation uses lookup tables):
```c
void dmr_fec_encode(uint8_t *data_in, uint8_t *coded_out) {
    // DMR voice uses Golay(20,8) for vocoder frames
    // and rate-3/4 Hamming for other bits

    // Golay encoding (8 data bits → 20 coded bits)
    for (int i = 0; i < 3; i++) {
        uint8_t data_byte = data_in[i * 3];
        uint32_t codeword = golay_20_8_encode(data_byte);

        // Pack into output
        coded_out[i * 3] = (codeword >> 16) & 0xFF;
        coded_out[i * 3 + 1] = (codeword >> 8) & 0xFF;
        coded_out[i * 3 + 2] = codeword & 0xFF;
    }
}

// Golay(20,8) encode using generator matrix
uint32_t golay_20_8_encode(uint8_t data) {
    // Generator matrix multiply
    uint32_t codeword = data;
    uint32_t parity = 0;

    // Compute 12 parity bits
    for (int i = 0; i < 12; i++) {
        // XOR data bits according to generator matrix row i
        parity |= (golay_parity_bit(data, i) << i);
    }

    return (parity << 8) | data;
}
```

**FEC Decode** (with error correction):
```c
int dmr_fec_decode(uint8_t *coded_in, uint8_t *data_out) {
    int total_errors = 0;

    for (int i = 0; i < 3; i++) {
        // Unpack codeword
        uint32_t codeword = (coded_in[i*3] << 16) |
                            (coded_in[i*3+1] << 8) |
                            coded_in[i*3+2];

        // Golay decode with error correction (up to 3 errors)
        uint8_t data;
        int errors = golay_20_8_decode(codeword, &data);

        if (errors < 0) return -1; // Uncorrectable

        data_out[i] = data;
        total_errors += errors;
    }

    return total_errors;
}
```

---

## 7. Multi-Core Processing

### 7.1 Thread Architecture

**Core 0: RX Processing Thread**
```c
void *rx_thread_main(void *arg) {
    // Set real-time priority
    struct sched_param param;
    param.sched_priority = 80; // High priority
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    // Pin to Core 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    while (sfr_running) {
        // Wait for RX timeslot
        wait_for_timeslot(TIMESLOT_RX);

        // 1. Demodulate DMR signal
        demodulate_4fsk(rx_samples, demod_bits, NUM_SAMPLES);

        // 2. Decode DMR frame
        dmr_voice_burst_t burst;
        uint8_t ambe_frames[2][9];
        if (dmr_decode_voice_burst(demod_bits, &burst,
                                   ambe_frames[0], ambe_frames[1]) >= 0) {
            // 3. Decode AMBE
            int16_t pcm_1[160], pcm_2[160];
            ambe_decode(ambe_frames[0], pcm_1);
            ambe_decode(ambe_frames[1], pcm_2);

            // 4. Write to audio buffer
            audio_ringbuffer_write(rx_audio_buffer, pcm_1, 160);
            audio_ringbuffer_write(rx_audio_buffer, pcm_2, 160);
        }
    }

    return NULL;
}
```

**Core 1: TX Processing Thread**
```c
void *tx_thread_main(void *arg) {
    // Set real-time priority
    struct sched_param param;
    param.sched_priority = 80; // High priority
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    // Pin to Core 1
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    while (sfr_running) {
        // Wait for TX timeslot
        wait_for_timeslot(TIMESLOT_TX);

        // 1. Read audio from buffer
        int16_t pcm_1[160], pcm_2[160];
        if (audio_ringbuffer_read(tx_audio_buffer, pcm_1, 160) == 160 &&
            audio_ringbuffer_read(tx_audio_buffer, pcm_2, 160) == 160) {

            // 2. Encode AMBE
            uint8_t ambe_frames[2][9];
            ambe_encode(pcm_1, ambe_frames[0]);
            ambe_encode(pcm_2, ambe_frames[1]);

            // 3. Encode DMR frame
            dmr_voice_burst_t burst;
            uint8_t embedded_lc[6] = {0}; // Placeholder
            dmr_encode_voice_burst(&burst, ambe_frames[0], ambe_frames[1],
                                   embedded_lc, true); // BS sourced

            // 4. Modulate
            uint8_t frame_bits[264];
            dmr_burst_to_bits(&burst, frame_bits);
            modulate_4fsk(frame_bits, 264, tx_samples, SAMPLE_RATE);

            // 5. Transmit
            transmit_samples(tx_samples, NUM_SAMPLES);
        } else {
            // No audio, transmit idle burst
            transmit_idle_burst();
        }
    }

    return NULL;
}
```

### 7.2 Synchronization

**Timeslot Semaphores**:
```c
sem_t rx_timeslot_sem;
sem_t tx_timeslot_sem;

// Initialize
sem_init(&rx_timeslot_sem, 0, 0);
sem_init(&tx_timeslot_sem, 0, 0);

// Timeslot timer ISR
void timeslot_timer_isr(void) {
    if (current_slot == TIMESLOT_RX) {
        sem_post(&rx_timeslot_sem); // Signal RX thread
    } else {
        sem_post(&tx_timeslot_sem); // Signal TX thread
    }
}

// Wait in thread
void wait_for_timeslot(int slot) {
    if (slot == TIMESLOT_RX) {
        sem_wait(&rx_timeslot_sem);
    } else {
        sem_wait(&tx_timeslot_sem);
    }
}
```

---

## 8. Configuration and Control

### 8.1 Configuration File

**YAML Configuration Example**:
```yaml
sfr:
  enabled: true
  frequency: 446.00625  # MHz (PMR446 channel 1)
  color_code: 1

hardware:
  plutosdr:
    uri: "ip:192.168.2.1"
    rx_gain: 50        # dB
    tx_attenuation: 0  # dB
    sample_rate: 384000 # Hz
    bandwidth: 200000   # Hz (minimum for AD9363)

  external_pa:
    enabled: true
    gpio_pin: 54       # PA enable pin
    power_watts: 1.0

  gpsdo:
    enabled: false
    reference_freq: 10000000 # 10 MHz

vocoder:
  type: "hardware"     # or "software"
  device: "/dev/ttyUSB0"  # USB-3000

echo_cancellation:
  enabled: true
  filter_length: 512
  step_size: 0.3

timing:
  pll_kp: 0.01
  pll_ki: 0.0001

logging:
  level: "info"       # debug, info, warning, error
  file: "/var/log/mmdvm-sdr-sfr.log"
```

### 8.2 Runtime Control

**Control Interface** (Unix socket or network):
```c
// Control commands
typedef enum {
    CMD_STATUS,       // Get SFR status
    CMD_START,        // Start SFR
    CMD_STOP,         // Stop SFR
    CMD_SET_FREQ,     // Change frequency
    CMD_SET_CC,       // Change color code
    CMD_GET_STATS,    // Get statistics
} sfr_command_t;

// Status structure
typedef struct {
    bool running;
    float frequency_mhz;
    uint8_t color_code;
    uint32_t frames_rx;
    uint32_t frames_tx;
    uint32_t fec_errors;
    float erle_db;          // Echo cancellation performance
    float pll_offset_ppm;   // Timing offset
} sfr_status_t;
```

---

## 9. Testing and Validation

### 9.1 Unit Tests

**Test 1: Timing Accuracy**
```c
void test_timing_accuracy(void) {
    // Measure timeslot boundaries with GPIO toggle
    for (int i = 0; i < 1000; i++) {
        wait_for_timeslot(TIMESLOT_RX);
        gpio_toggle(TEST_PIN);

        // Measure with oscilloscope
        // Expected: 30.000 ms ±1 μs period
    }
}
```

**Test 2: Echo Cancellation ERLE**
```c
void test_echo_cancellation_erle(void) {
    // Inject known echo signal
    float echo_power = 0.0f;
    float residual_power = 0.0f;

    for (int i = 0; i < 8000; i++) { // 1 second at 8 kHz
        float tx_signal = sinf(2.0f * M_PI * 1000.0f * i / 8000.0f);
        float echo = 0.5f * tx_signal; // -6 dB echo
        float rx_signal = 0.01f * sinf(2.0f * M_PI * 500.0f * i / 8000.0f);
        float desired = rx_signal + echo;

        float output = nlms_process(echo_canceller, tx_signal, desired);

        echo_power += echo * echo;
        residual_power += (output - rx_signal) * (output - rx_signal);
    }

    float erle_db = 10.0f * log10f(echo_power / residual_power);
    printf("ERLE: %.1f dB\n", erle_db);

    // Expected: >40 dB after convergence
}
```

### 9.2 Integration Tests

**Test 3: Loopback Test**
```c
void test_loopback(void) {
    // Connect TX output to RX input (via attenuator)
    // Transmit known DMR frame on TS2
    // Verify reception on TS1
    // Check audio quality (cross-correlation)
}
```

**Test 4: Two-Radio Test**
```c
void test_two_radio(void) {
    // Radio A → SFR → Radio B
    // Radio A transmits on TS1
    // SFR receives TS1, retransmits TS2
    // Radio B receives TS2
    // Verify audio quality and latency
}
```

### 9.3 Field Testing

**Range Test Procedure**:
1. Deploy SFR at elevated location
2. Configure for maximum TX power (5W with PA)
3. Mobile station drives away, maintains voice communication
4. Measure maximum reliable range
5. Document signal strength (RSSI) at various distances

**Expected Results**: 10-20 km with 5W PA, clear line-of-sight

---

## 10. Deployment Guide

### 10.1 Hardware Assembly

**Components**:
1. PlutoSDR (AD9363)
2. External PA (1-5W, 400-480 MHz)
3. PA control circuit (GPIO-controlled relay or MOSFET)
4. DVSI USB-3000 codec (or AMBE chip)
5. Antenna (omnidirectional, 400-480 MHz)
6. Power supply (12V, 2-5A depending on PA)
7. (Optional) GPSDO (10 MHz reference)

**Connections**:
```
PlutoSDR TX → Attenuator (10 dB) → PA Input
PlutoSDR RX ← Antenna (direct or via circulator)
PA Output → Antenna
PA Enable ← PlutoSDR GPIO (via level shifter if needed)
USB-3000 ← PlutoSDR USB Host (via USB hub if needed)
```

**PA Control Circuit**:
```
PlutoSDR GPIO (3.3V) → MOSFET Gate (via 1k resistor)
MOSFET Drain → PA Enable (active high)
MOSFET Source → GND
```

### 10.2 Software Installation

**Prerequisites**:
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    libiio-dev libusb-1.0-0-dev \
    libyaml-dev libconfig-dev

# Clone mmdvm-sdr repository
git clone https://github.com/yourusername/mmdvm-sdr.git
cd mmdvm-sdr

# Checkout SFR branch
git checkout feature/dmr-sfr
```

**Build**:
```bash
mkdir build && cd build
cmake .. -DENABLE_SFR=ON -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

**Configuration**:
```bash
# Copy example config
sudo cp /etc/mmdvm-sdr/sfr.yaml.example /etc/mmdvm-sdr/sfr.yaml

# Edit configuration
sudo nano /etc/mmdvm-sdr/sfr.yaml
```

**Run**:
```bash
# Test mode (foreground)
sudo mmdvm-sdr-sfr -c /etc/mmdvm-sdr/sfr.yaml -v

# Production mode (systemd service)
sudo systemctl enable mmdvm-sdr-sfr
sudo systemctl start mmdvm-sdr-sfr
```

### 10.3 Tuning and Optimization

**Step 1: Verify Timing**:
- Connect oscilloscope to GPIO test pin
- Verify 30 ms timeslot period (±1 μs)
- Adjust PLL parameters if needed

**Step 2: Optimize Echo Cancellation**:
- Monitor ERLE in real-time
- Adjust step size for faster convergence
- Increase filter length if ERLE <40 dB

**Step 3: Range Testing**:
- Start with low power (1W)
- Progressively increase distance
- Monitor audio quality and RSSI
- Increase PA power as needed

---

## 11. Future Enhancements

### 11.1 Dual-Timeslot SFR

**Concept**: Support both TS1→TS2 and TS2→TS1 simultaneously

**Benefits**:
- Two independent talk paths
- Doubles capacity

**Challenges**:
- Requires dual vocoder hardware or 2× CPU load
- More complex audio routing
- Increased memory requirements

### 11.2 Trunking Support (DMR Tier III)

**Features**:
- Automatic channel assignment
- Priority and preemption
- Fleet management

**Challenges**:
- Complex protocol implementation
- Requires trunking controller
- Higher CPU and memory overhead

### 11.3 Network Connectivity

**Features**:
- Connect to DMR networks (BrandMeister, TGIF, etc.)
- IP-based DMR (DMR-MARC, c-Bridge)
- Remote monitoring and control

**Implementation**:
- MMDVM-style network protocols
- JSON-based control API
- Web-based dashboard

### 11.4 Advanced Echo Cancellation

**Deep Learning Approach**:
- Train CNN+GRU network for echo cancellation
- Offline training on collected data
- Deploy optimized inference on ARM

**Expected Improvement**: 60-70 dB ERLE

**Challenges**: Computational overhead, requires GPU for training

---

## 12. Conclusion

This implementation specification provides a complete roadmap for DMR Single Frequency Repeater development on PlutoSDR. The phased approach, starting with timing and progressing through echo cancellation and full SFR functionality, manages risk while delivering incremental value.

**Key Success Factors**:
1. Precise timing implementation (software PLL)
2. Effective echo cancellation (NLMS, 40+ dB ERLE)
3. Standards-compliant vocoder (DVSI hardware)
4. Multi-core optimization (distribute processing)
5. Thorough testing and validation

**Expected Outcome**: Functional DMR SFR comparable to commercial handheld SFR products, suitable for amateur radio and experimental deployments.

---

**Document prepared for**: mmdvm-sdr project

**Related documents**:
- DMR_SFR_STANDARDS.md
- COMMERCIAL_IMPLEMENTATIONS.md
- SFR_TECHNICAL_REQUIREMENTS.md
- SFR_FEASIBILITY_STUDY.md

**Implementation Specification Status**: ✅ **COMPLETE**

**Next Step**: Begin Phase 1 implementation (timing and basic TDMA)
