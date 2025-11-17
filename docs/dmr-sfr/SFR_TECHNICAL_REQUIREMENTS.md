# DMR Single Frequency Repeater - Technical Requirements

**Document Version:** 1.0
**Date:** 2025-11-16
**Project:** mmdvm-sdr DMR SFR Feasibility Study

---

## Executive Summary

This document specifies the detailed technical requirements for implementing a DMR Single Frequency Repeater (SFR). Requirements are derived from ETSI standards, commercial implementations analysis, and engineering constraints. The analysis covers timing, processing, echo cancellation, audio, synchronization, and hardware requirements.

**Critical Finding**: SFR implementation is technically challenging but achievable with proper design, requiring precise timing control, fast TX/RX switching, and robust signal processing.

---

## 1. Timing Requirements

### 1.1 TDMA Frame Timing

| Timing Parameter | Value | Tolerance | Critical? |
|-----------------|-------|-----------|-----------|
| **Timeslot Duration** | 30.000 ms | ±0.001 ms | ✅ Critical |
| **Frame Duration** | 60.000 ms | ±0.002 ms | ✅ Critical |
| **Guard Time** | 2.5 ms | N/A | ✅ Critical |
| **PA Ramp-Up** | 1.25 ms | ±0.1 ms | Important |
| **PA Ramp-Down** | 1.25 ms | ±0.1 ms | Important |
| **Propagation Allowance** | ~1.0 ms | Variable | Important |

#### Timing Accuracy Requirements

**Absolute Timing**:
- Timeslot boundaries must be accurate to within **1 μs**
- Frame alignment accuracy: **<10 μs**
- Slot-to-slot jitter: **<5 μs RMS**

**Reasoning**: TDMA receivers expect signals within narrow time windows. Timing errors cause:
- Slot overlap
- Data corruption
- Synchronization loss
- Reduced range

### 1.2 Processing Latency Budget

**Total Available Time**: 30 ms (one timeslot)

**Budget Breakdown**:

| Processing Stage | Time Budget | Notes |
|-----------------|-------------|-------|
| **RX Slot** | 30.000 ms | Fixed (timeslot duration) |
| **RX Demodulation** | 0-30 ms | Streaming, parallel to RX |
| **DMR Frame Decode** | <5 ms | Must complete before TX slot |
| **Vocoder Decode** | <20 ms | AMBE+2: 20 ms nominal |
| **Audio Processing** | <2 ms | Minimal |
| **Vocoder Encode** | <20 ms | AMBE+2: 20 ms nominal |
| **DMR Frame Encode** | <3 ms | Frame assembly |
| **TX Modulation** | 0-30 ms | Streaming, parallel to TX |
| **TX Slot** | 30.000 ms | Fixed (timeslot duration) |

**Critical Path Analysis**:

```
RX Slot (TS1): 0-30 ms
├─ Demodulation: 0-30 ms (streaming)
├─ Frame sync/decode: 25-30 ms (last 5 ms of slot)
└─ Vocoder decode: 30-50 ms (3 frames × 20 ms, can overlap)

Processing: 30-60 ms
├─ Audio buffering: <2 ms
└─ Echo cancellation: <2 ms (if implemented)

TX Preparation: 50-60 ms
├─ Vocoder encode: 40-60 ms (3 frames × 20 ms)
├─ Frame assembly: 57-60 ms
└─ Modulation prep: 58-60 ms

TX Slot (TS2): 60-90 ms
└─ Transmission: 60-90 ms (streaming)
```

**Total Latency**: 60 ms (one frame = one timeslot delay)

**Maximum Acceptable**: 90 ms (90 ms delay = retransmit on TS2 of next frame)
- Preferred: 60 ms (same frame, TS2)
- Acceptable: 90 ms (next frame, TS2)
- Unacceptable: >90 ms (breaks TDMA synchronization)

### 1.3 TX/RX Switching Time

**Requirement**: Switch between RX and TX within **guard time**

| Switching Parameter | Requirement | Notes |
|--------------------|-------------|-------|
| **RX to TX Switch** | <2.5 ms | Must complete within guard time |
| **TX to RX Switch** | <2.5 ms | Must complete within guard time |
| **Recommended** | <1.0 ms | Leaves margin for propagation |
| **Optimal** | <500 μs | Commercial implementations achieve this |

**Switching Components**:
1. **RF Switching**: Antenna or front-end switching
2. **Synthesizer Settling**: If frequency change required (N/A for SFR)
3. **AGC Settling**: Automatic Gain Control stabilization
4. **Digital Processing**: Mode change in SDR

**For PlutoSDR/AD9363**:
- No RF switching required (simultaneous TX/RX)
- No synthesizer settling (same frequency)
- AGC settling: <100 μs typically
- Digital processing: Software-dependent

**Conclusion**: PlutoSDR hardware capable; software timing is critical.

### 1.4 Synchronization Requirements

#### Frequency Synchronization

| Parameter | Requirement | Rationale |
|-----------|-------------|-----------|
| **Frequency Accuracy** | ±0.5 ppm | Industry standard for DMR |
| **Frequency Stability** | <1 ppm drift/hour | Maintain sync over time |
| **Temperature Stability** | <2 ppm over operating range | Environmental variations |

**PlutoSDR Performance**:
- Reference oscillator: 40 MHz TCXO
- Typical stability: ±1-2 ppm
- **Assessment**: Marginal; may need GPSDO for professional performance

#### Timing Synchronization

**Requirements**:
1. **Frame Alignment**: Lock to received DMO signal
2. **Slot Boundary Precision**: ±1 μs
3. **Long-term Stability**: No drift over hours of operation

**Implementation**:
- PLL-based timing recovery
- Disciplined clock from TDMA frame sync
- Continuous tracking of received signals

---

## 2. Echo Cancellation Requirements

### 2.1 Challenge Definition

**Self-Interference Sources**:
1. **TX to RX Leakage**: Transmitter signal coupling into receiver
2. **Acoustic Echo**: Audio feedback (less relevant for digital)
3. **Digital Echo**: Processing loop feedback

### 2.2 Isolation Requirements

**General Repeater Isolation**:
| System | Required Isolation | Notes |
|--------|-------------------|-------|
| **Traditional Dual-Freq Repeater** | 70-100 dB | Via duplexer |
| **High-Gain RF Repeater** | 100+ dB | Antenna separation |
| **Single Frequency Repeater** | 60-90 dB | Digital cancellation |

**SFR Isolation Methods**:
1. **Temporal Isolation**: 30-60 dB (time slot separation)
2. **Digital Cancellation**: 30-50 dB (adaptive algorithms)
3. **Antenna Isolation**: 20-40 dB (physical separation)
4. **Combined Total**: 80-150 dB (sum of all methods)

### 2.3 Digital Echo Cancellation Algorithms

**Recommended Algorithms**:

#### 2.3.1 NLMS (Normalized Least Mean Square)

**Characteristics**:
- Most widely used for echo cancellation
- Good balance of performance and complexity
- Adaptive to changing conditions
- Lower computational complexity than RLS

**Parameters**:
- **Step size (μ)**: 0.1 to 0.5 (normalized)
- **Filter length**: 256-1024 taps
- **Convergence time**: <100 ms
- **Steady-state ERLE**: 40-50 dB

**Equation**:
```
h(n+1) = h(n) + μ · e(n) · x(n) / (||x(n)||² + δ)

Where:
h(n) = filter coefficients at time n
e(n) = error signal (desired - output)
x(n) = input signal vector
μ = step size
δ = regularization parameter (small constant, e.g., 0.001)
```

**Computational Complexity**: O(N) per sample (N = filter length)

#### 2.3.2 LMS (Least Mean Square)

**Characteristics**:
- Simpler than NLMS
- Fixed step size (can be problematic)
- Lower computational cost
- May be unstable with varying signal levels

**Use Case**: Constant signal level environments

**ERLE Performance**: 35-45 dB

#### 2.3.3 RLS (Recursive Least Squares)

**Characteristics**:
- Faster convergence than LMS/NLMS
- Better steady-state performance
- Higher computational complexity
- More memory required

**Computational Complexity**: O(N²) per sample

**ERLE Performance**: 50-60 dB

**Trade-off**: May exceed processing budget for real-time SFR

#### 2.3.4 Deep Learning Approaches (2024 Research)

**CNN + GRU Networks**:
- State-of-the-art performance (2024)
- Self-interference cancellation: >60 dB
- Requires training data
- High computational requirements

**Feasibility for mmdvm-sdr**:
- ❌ Too computationally intensive for real-time on PlutoSDR
- ✅ Possible future optimization for ARM processors

### 2.4 Echo Return Loss Enhancement (ERLE)

**ERLE Definition**: Measure of echo cancellation effectiveness

**Required ERLE**: 40-50 dB for SFR operation

**Achieving Required ERLE**:
| Method | ERLE Contribution | Implementation |
|--------|------------------|----------------|
| **Temporal (Timeslot)** | 30-40 dB | TDMA structure (inherent) |
| **NLMS Adaptive Filter** | 40-50 dB | Digital signal processing |
| **Antenna Separation** | 20-30 dB | Physical installation |
| **Combined** | 90-120 dB | Sum of all methods |

**Practical Requirement**:
- **Minimum**: 40 dB digital cancellation
- **Target**: 50 dB digital cancellation
- **Combined with timeslot isolation**: 70-90 dB total

### 2.5 Convergence Time

**Requirement**: Echo canceller must converge quickly

| Phase | Time Requirement | Notes |
|-------|------------------|-------|
| **Initial Convergence** | <500 ms | First few frames |
| **Tracking** | Continuous | Adapt to changes |
| **Re-convergence** | <200 ms | After signal interruption |

**Impact on SFR**:
- First few transmissions may have echo
- System improves over first 10-15 frames
- User-perceptible delay: minimal if <500 ms convergence

### 2.6 Processing Requirements

**For NLMS with 512-tap filter at 8 kHz sampling**:

**Operations per Sample**:
- Multiply-accumulate (MAC): 512
- Vector operations: 3-4
- Normalization: 1

**Total Operations**: ~520 MACs per sample

**At 8 kHz sampling rate**:
- Operations per second: 520 × 8,000 = 4.16 MMAC/s (Million MACs)

**Processor Requirements**:
- **ARM Cortex-A9** (PlutoSDR): 1,000+ MMAC/s capable
- **Utilization**: <1% for echo cancellation alone

**Conclusion**: ✅ Computationally feasible on PlutoSDR

---

## 3. Audio Processing Requirements

### 3.1 Vocoder Requirements

**DMR Standard Vocoder**: AMBE+2

| Parameter | Specification |
|-----------|--------------|
| **Type** | AMBE+2 (DVSI proprietary) |
| **Bit Rate** | 2,450 bit/s (voice) |
| **Total Rate** | 3,600 bit/s (with FEC) |
| **Frame Length** | 20 ms |
| **Sampling Rate** | 8 kHz |
| **Sample Format** | S16LE (signed 16-bit little-endian) |
| **Frames per Burst** | 3 frames (60 ms) |

#### 3.1.1 AMBE+2 Licensing

**Commercial Options**:
1. **DVSI Hardware Codecs**:
   - AMBE-2020 chip
   - AMBE-3003 chip
   - USB-3000 dongle
   - Cost: $100-300 per unit

2. **Software Licenses**:
   - AMBE+2 SDK (proprietary, expensive)
   - Contact DVSI for pricing

#### 3.1.2 Open-Source Alternatives

**Codec2** (VK5DGR):
- **License**: Open-source (LGPL)
- **Bit Rate**: 1,200-3,200 bit/s
- **Quality**: Comparable to AMBE+2 at lower bit rates
- **DMR Compatibility**: ❌ **Not compatible** (breaks standard)

**mbelib** (Open-source AMBE decoder):
- Decode-only (no encoding)
- Used in DSD+, SDR-Trunk
- Reverse-engineered
- Legal gray area

**MD380 Vocoder** (GitHub: nostar/md380_vocoder):
- Extracted from MD380 firmware
- Software AMBE+2 2450×1150
- Encoding and decoding
- Licensing unclear

**Recommendation for mmdvm-sdr**:
1. **Commercial Deployment**: Use DVSI hardware codec
2. **Amateur/Experimental**: Investigate md380_vocoder or Codec2 (non-standard)
3. **Standards-Compliant**: Must use AMBE+2

### 3.2 Audio Processing Pipeline

**SFR Audio Path**:

```
RX Path:
DMR Signal → Demodulate → Extract Voice Frames → AMBE+2 Decode → PCM Audio (8 kHz)
                                                                            ↓
                                                                   Audio Processing
                                                                            ↓
TX Path:                                                                    ↓
DMR Signal ← Modulate ← Assemble Frames ← AMBE+2 Encode ← PCM Audio (8 kHz)
```

**Processing Stages**:

1. **Voice Frame Extraction**:
   - Extract 3 × 72-bit vocoder frames from 30 ms burst
   - Handle FEC decoding
   - Detect and flag errors

2. **AMBE+2 Decode**:
   - Input: 72-bit encoded frames
   - Output: 160 samples (20 ms at 8 kHz)
   - Latency: 20 ms per frame

3. **Audio Processing** (Optional):
   - Noise reduction: <2 ms
   - AGC: <1 ms
   - Filtering: <1 ms
   - **Total budget**: <5 ms

4. **AMBE+2 Encode**:
   - Input: 160 samples (20 ms at 8 kHz)
   - Output: 72-bit encoded frames
   - Latency: 20 ms per frame

5. **Frame Assembly**:
   - Combine 3 vocoder frames into DMR burst
   - Add FEC
   - Generate sync patterns
   - Latency: <3 ms

### 3.3 Noise Reduction

**Requirement**: Advanced noise reduction (up to 25 dB suppression)

**Algorithms**:
1. **Spectral Subtraction**
2. **Wiener Filtering**
3. **Voice Activity Detection (VAD)**

**Implementation**:
- Optional feature
- Improves audio quality in noisy environments
- Processing time: <2 ms per 20 ms frame
- May improve perceived range

### 3.4 Audio Buffering

**Requirements**:
- **Jitter Buffer**: 60-120 ms
- **De-jitter**: Smooth out timing variations
- **Packet Loss Concealment**: Interpolate missing frames

**Implementation**:
- Ring buffer: 240-480 ms capacity
- Watermark-based flow control
- Underrun/overrun protection

---

## 4. Delay Compensation Requirements

### 4.1 Sources of Delay

| Delay Source | Typical Value | Notes |
|-------------|---------------|-------|
| **RF Propagation** | 3.33 μs/km | Speed of light |
| **ADC/DAC Latency** | 100-500 μs | Hardware dependent |
| **Digital Filtering** | 500-2000 μs | FIR filter group delay |
| **Vocoder Decode** | 20 ms | Per frame |
| **Audio Processing** | 2-5 ms | Optional processing |
| **Vocoder Encode** | 20 ms | Per frame |
| **Frame Assembly** | 2-3 ms | DMR burst generation |
| **Total** | ~50-60 ms | Fits within timeslot budget |

### 4.2 Compensation Strategies

#### 4.2.1 Fixed Delay Compensation

**Method**: Add fixed delay to align TX slot

**Implementation**:
```
RX_Timeslot = 1 (0-30 ms)
Processing_Delay = 60 ms
TX_Timeslot = 2 (60-90 ms)

Total_Delay = RX_Timeslot + Processing_Delay + TX_Timeslot
            = 0-30 ms + 60 ms + 60-90 ms
            = 120-180 ms total latency (acceptable for DMR)
```

#### 4.2.2 Adaptive Delay Compensation

**Method**: Measure actual processing time and adjust

**Benefits**:
- Minimize latency
- Adapt to varying CPU load
- Optimize for different processing modes

**Implementation Complexity**: Medium

### 4.3 Latency Budget Summary

**Absolute Maximum**: 90 ms (next frame, same timeslot)
**Target**: 60 ms (same frame, next timeslot)
**Achievable**: 50-70 ms with optimization

**User Perception**:
- <100 ms: Generally imperceptible
- 100-200 ms: Noticeable but acceptable
- >200 ms: Annoying

**SFR Total Latency**: 120-180 ms end-to-end (acceptable)

---

## 5. Full-Duplex Operation Challenges

### 5.1 Traditional Full-Duplex vs. SFR

**Traditional Full-Duplex**:
- Different frequencies for TX and RX
- Duplexer provides isolation (80-100 dB)
- TX and RX simultaneous and continuous

**SFR Approach**:
- **Same frequency** for TX and RX
- **Time-division**: TX and RX in different timeslots
- **Not truly simultaneous** (temporal separation)

**Key Difference**: SFR leverages TDMA for isolation, not true full-duplex.

### 5.2 Self-Interference Challenge

**Problem**: TX signal is 90-120 dB stronger than desired RX signal

**Example**:
- TX Power: +20 dBm (100 mW)
- RX Sensitivity: -100 dBm
- **Required Isolation**: 120 dB

**SFR Mitigation**:
1. **Timeslot Separation**: 30-40 dB (TX off during RX)
2. **Digital Cancellation**: 40-50 dB (NLMS algorithm)
3. **Antenna Separation**: 20-30 dB (physical distance)
4. **Combined**: 90-120 dB (sufficient)

### 5.3 TX/RX Leakage in SDR

**SDR-Specific Issues**:

#### 5.3.1 LO Leakage (PlutoSDR/AD9363)

**Problem**:
> "Due to the design of the AD9364/AD9363, if both the TX and RX Local Oscillators (LO) are set to the same frequency or very close to each other, the TX LO may leak into the RX path."

**Impact on SFR**:
- TX LO leakage into RX
- Desensitization of receiver
- Requires mitigation

**Mitigation**:
1. **Time-division operation** (SFR inherent advantage)
   - TX LO only active during TX timeslot
   - RX LO only active during RX timeslot
   - No simultaneous TX/RX on same frequency

2. **RX Gain Management**:
   - Reduce RX gain during TX slot
   - Increase RX gain during RX slot
   - Prevent receiver saturation

3. **Digital Filtering**:
   - Reject out-of-slot signals
   - Timeslot gating in software

#### 5.3.2 AD9363 Isolation Specifications

**Internal TX-to-RX Isolation**:
- **Specified**: >50 dB (AD9361, similar for AD9363)
- **PCB-dependent**: Actual isolation depends on layout
- **Not published**: Official AD9363 datasheet lacks specific value

**External Isolation**:
- **Antenna separation**: 30-40 dB (separate TX/RX antennas)
- **Filtering**: 10-20 dB (band-pass filters)

**Total Available**: 90-110 dB (sufficient for SFR)

### 5.4 Timing-Based Isolation

**SFR Advantage**: Temporal separation provides inherent isolation

**Method**:
1. **RX Timeslot (TS1: 0-30 ms)**:
   - TX fully off (PA disabled)
   - RX active
   - No TX leakage

2. **Guard Time (30-32.5 ms)**:
   - TX ramp-up
   - RX disabled or gated
   - Switch antennas (if using separate)

3. **TX Timeslot (TS2: 32.5-60 ms)**:
   - TX active
   - RX fully off (or heavily attenuated)
   - No interference to RX

**Isolation from Timing**: Effectively infinite (TX off = no signal)

**Practical Consideration**: Switching transients must be managed.

---

## 6. SDR-Specific Considerations

### 6.1 PlutoSDR/AD9363 Capabilities

#### 6.1.1 Full-Duplex Capability

**AD9363 Architecture**:
- 1T1R (1 Transmit, 1 Receive chain)
- Independent TX and RX paths
- Can operate simultaneously at different frequencies (FDD)

**Same-Frequency Limitation**:
- LO leakage issue when TX LO = RX LO
- **Not recommended** for true same-frequency full-duplex
- **Acceptable for SFR**: Time-division mitigates issue

#### 6.1.2 TX Power Output

| Parameter | Specification |
|-----------|--------------|
| **Maximum Output Power** | +7 dBm (5 mW) |
| **Typical Output** | 0 to +7 dBm |
| **External PA** | Required for >10 mW |

**For SFR**:
- +7 dBm sufficient for testing
- External PA needed for practical range (500 mW - 5 W)
- PA control (enable/disable) required for timeslot switching

#### 6.1.3 Frequency Range

**AD9363**:
- **Tuning Range**: 325 MHz - 3.8 GHz
- **DMR Bands Supported**:
  - ✅ VHF: 136-174 MHz (requires external mixer/upconverter)
  - ✅ UHF: 400-480 MHz (native)
  - ✅ 900 MHz: 896-941 MHz (native)

**Note**: VHF DMR requires external frequency conversion or PlutoSDR+ (extended range).

#### 6.1.4 Bandwidth

**AD9363 Bandwidth**:
- 200 kHz to 20 MHz programmable
- DMR requirement: 12.5 kHz channel
- **Configuration**: 200 kHz minimum (16× DMR channel)
- **Impact**: Wider filtering, but acceptable

#### 6.1.5 Sample Rate

**AD9363**:
- Up to 61.44 MSPS (full duplex)
- DMR symbol rate: 4,800 symbols/s
- **Recommended**: 48 kSPS minimum (10× symbol rate)
- **Typical**: 192-384 kSPS (40-80× symbol rate)

### 6.2 Processing Platform

**PlutoSDR Platform**:
- **FPGA**: Xilinx Zynq 7010 (28 nm)
- **ARM CPU**: Dual-core Cortex-A9 @ 667 MHz
- **RAM**: 512 MB DDR3
- **Storage**: Linux filesystem on flash

**Processing Capacity**:
- **FPGA**: Sufficient for digital filtering, decimation, interpolation
- **ARM**: Sufficient for DMR protocol stack, vocoder, echo cancellation
- **Combined**: Adequate for SFR implementation

### 6.3 Latency Sources in SDR

| Component | Latency | Mitigation |
|-----------|---------|------------|
| **ADC** | ~10 μs | Fixed, minimal |
| **Digital Downconversion** | ~100 μs | Optimize filter length |
| **USB Transfer (to host)** | 1-5 ms | Use on-device processing |
| **Host Processing** | 10-50 ms | Optimize algorithms |
| **USB Transfer (from host)** | 1-5 ms | Use on-device processing |
| **Digital Upconversion** | ~100 μs | Optimize filter length |
| **DAC** | ~10 μs | Fixed, minimal |

**Hosted SDR (PlutoSDR ↔ PC)**:
- Total latency: 20-100 ms
- Acceptable for SFR but tight

**Standalone (mmdvm-sdr)**:
- No USB transfer latency
- Total latency: 10-30 ms
- **Preferred for SFR**

### 6.4 Timing Accuracy

**SDR Clock Sources**:
- **PlutoSDR**: 40 MHz TCXO (±1-2 ppm)
- **Disciplined Option**: External 10 MHz reference or GPSDO

**DMR Timing Requirements**:
- Timeslot accuracy: ±1 μs
- Achievable with software PLL locked to received frames
- Long-term stability: May need GPSDO for extended operation

---

## 7. Processing Overhead Requirements

### 7.1 Computational Load Estimate

**For SFR on PlutoSDR ARM (Dual Cortex-A9 @ 667 MHz)**:

| Processing Task | CPU Load | Notes |
|----------------|----------|-------|
| **DMR Demodulation** | 15-20% | Symbol timing, 4-FSK demod |
| **Frame Decode/Encode** | 10-15% | DMR protocol, FEC |
| **AMBE+2 Vocoder** | 20-30% | Software or hardware codec |
| **Echo Cancellation** | 5-10% | NLMS 512-tap filter |
| **Audio Processing** | 2-5% | Optional (noise reduction) |
| **System Overhead** | 5-10% | OS, I/O, scheduling |
| **Total** | 57-90% | Single core |

**Multi-Core Optimization**:
- Core 1: RX processing (demod, decode, vocoder decode)
- Core 2: TX processing (vocoder encode, encode, modulation)
- **Result**: 30-50% load per core (comfortable margin)

### 7.2 Memory Requirements

**RAM Usage**:
- **DMR Protocol Stack**: 10-20 MB
- **Audio Buffers**: 5-10 MB
- **Echo Cancellation**: 2-4 MB (filter coefficients, history)
- **Vocoder**: 5-10 MB (if software)
- **Total**: 25-50 MB

**PlutoSDR Available**: 512 MB (sufficient)

### 7.3 Real-Time Constraints

**Critical Real-Time Requirements**:
1. **Timeslot boundaries**: Must be met within μs precision
2. **Symbol timing**: Must be maintained without jitter
3. **Audio frame processing**: Deadline-driven (20 ms frames)

**Linux RT Kernel**: Recommended for predictable latency

**Priority Scheduling**:
- Highest: Timeslot timer
- High: RX/TX processing
- Medium: Protocol stack
- Low: Non-critical tasks

---

## 8. Performance Predictions

### 8.1 Expected Range

**Factors**:
1. **TX Power**: +7 dBm (PlutoSDR) to +37 dBm (5W with PA)
2. **Antenna Gain**: 0 dBi (omnidirectional) to +10 dBi (directional)
3. **RX Sensitivity**: -100 dBm (typical DMR)
4. **Environment**: Urban vs. rural, line-of-sight

**Range Estimates**:

| Configuration | Range (Urban) | Range (Rural/LOS) |
|--------------|---------------|-------------------|
| **PlutoSDR native (+7 dBm)** | 0.5-1 km | 2-3 km |
| **With 1W PA (+30 dBm)** | 3-5 km | 10-15 km |
| **With 5W PA (+37 dBm)** | 5-10 km | 20-30 km |

**Timing Limitation**: 30-50 km maximum (TDMA constraint)

### 8.2 Audio Quality

**Factors**:
1. **Vocoder Quality**: AMBE+2 (good)
2. **Processing Latency**: 60-90 ms (acceptable)
3. **Echo Cancellation**: 40-50 dB (sufficient)

**Predicted Quality**: Comparable to commercial SFR products

**Potential Issues**:
- Initial frames may have echo (convergence time)
- Weak signals may have vocoder artifacts
- Noise reduction effectiveness varies

### 8.3 Reliability

**Expected Reliability**:
- **Timing**: High (software PLL is stable)
- **Echo Cancellation**: Medium-High (depends on implementation quality)
- **Vocoder**: High (if using DVSI hardware) / Medium (if software)

**Failure Modes**:
1. **Timing drift**: Requires clock discipline
2. **Echo cancellation failure**: Requires tuning
3. **Vocoder errors**: Requires error handling
4. **Processing overruns**: Requires optimization

---

## 9. Hardware Requirements Summary

### 9.1 Minimum Requirements

| Component | Specification |
|-----------|--------------|
| **SDR Platform** | PlutoSDR or equivalent (AD9363/AD9364) |
| **Frequency Range** | 400-480 MHz (UHF) native |
| **TX Power** | +7 dBm minimum (5 mW) |
| **Processing** | Dual-core ARM Cortex-A9 @ 667 MHz |
| **RAM** | 512 MB |
| **Clock Stability** | ±2 ppm (±1 ppm preferred) |
| **Antenna** | Single omnidirectional or TX/RX pair |

### 9.2 Recommended Enhancements

| Enhancement | Benefit | Cost |
|-------------|---------|------|
| **External PA (1-5W)** | Increase range 10-30 km | $50-200 |
| **GPSDO** | Improve frequency stability to 0.01 ppm | $100-300 |
| **Separate TX/RX Antennas** | Improve isolation 20-30 dB | $50-150 |
| **Hardware AMBE+2 Codec** | Reduce CPU load, ensure quality | $100-300 |
| **Duplexer** (optional) | Additional isolation (not needed) | $200-500 |

### 9.3 Optional Components

- **Band-pass Filters**: Reduce out-of-band interference
- **LNA (Low-Noise Amplifier)**: Improve RX sensitivity
- **Cooling**: Heat dissipation for extended TX duty cycle

---

## 10. Software Requirements

### 10.1 Core Software Components

1. **DMR Protocol Stack**:
   - TDMA frame generation and parsing
   - Slot timing and synchronization
   - Color code and embedded signaling

2. **Modulator/Demodulator**:
   - 4-FSK modulation
   - Symbol timing recovery
   - Carrier frequency offset compensation

3. **Vocoder**:
   - AMBE+2 encode/decode
   - Frame formatting
   - Error handling

4. **Echo Cancellation**:
   - NLMS adaptive filter
   - Convergence monitoring
   - Parameter tuning

5. **Audio Processing**:
   - Buffering
   - Noise reduction (optional)
   - AGC (optional)

### 10.2 Real-Time Operating System

**Recommended**: Linux with PREEMPT_RT patches

**Benefits**:
- Predictable latency
- Priority-based scheduling
- High-resolution timers

**Alternative**: Bare-metal implementation (higher complexity)

### 10.3 Existing Code Bases

**Leverage Existing Projects**:
1. **MMDVM**: DMR protocol stack (partial)
2. **md380_vocoder**: Software AMBE+2
3. **GNU Radio**: DSP blocks (filters, modulators)
4. **Codec2**: Open-source vocoder (non-standard alternative)

**Integration Strategy**:
- Reuse proven DMR frame handling from MMDVM
- Integrate echo cancellation (new development)
- Add SFR-specific timing logic

---

## 11. Testing and Validation Requirements

### 11.1 Unit Testing

**Components to Test**:
1. **Timing Accuracy**: Verify timeslot boundaries ±1 μs
2. **Echo Cancellation**: Measure ERLE >40 dB
3. **Vocoder**: Validate encoding/decoding quality
4. **Modulation**: Check 4-FSK deviation and spectrum

### 11.2 Integration Testing

**Test Scenarios**:
1. **Loopback**: TX → RX within same unit
2. **Two-Radio**: Radio A → SFR → Radio B
3. **Range Test**: Measure effective range vs. TX power

### 11.3 Interoperability Testing

**Test with Commercial DMR Radios**:
- Motorola MOTOTRBO
- Hytera PD-series
- Kenwood NX-series
- TYT/Anytone DMR radios

**Validation Criteria**:
- Successful call establishment
- Clear audio (no echo, artifacts)
- Proper timeslot operation

### 11.4 Performance Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| **Range** | 5-10 km (with 1W PA) | Field testing |
| **Audio Quality** | MOS >3.5 | Subjective testing |
| **Echo Cancellation** | ERLE >40 dB | Signal analyzer |
| **Timing Accuracy** | <1 μs jitter | Oscilloscope |
| **CPU Load** | <50% per core | System monitoring |

---

## 12. Challenges and Mitigation Strategies

### 12.1 Major Challenges

| Challenge | Difficulty | Mitigation |
|-----------|-----------|------------|
| **Precise Timing** | High | Software PLL, RT kernel, hardware timers |
| **Echo Cancellation** | Medium-High | NLMS algorithm, tuning, separate antennas |
| **Vocoder Licensing** | Medium | DVSI hardware or md380_vocoder |
| **Processing Latency** | Medium | Optimize algorithms, multi-core |
| **Frequency Stability** | Low-Medium | TCXO (acceptable), GPSDO (preferred) |

### 12.2 Risk Assessment

**High Risk**:
- Timing synchronization: Critical for TDMA

**Medium Risk**:
- Echo cancellation: Affects audio quality
- Vocoder implementation: Licensing and quality

**Low Risk**:
- Processing power: PlutoSDR has sufficient capacity
- Modulation/demodulation: Well-understood 4-FSK

---

## 13. Conclusions

### 13.1 Technical Feasibility

**Conclusion**: ✅ **DMR SFR is technically feasible on PlutoSDR**

**Justification**:
1. ✅ Timing requirements are achievable with software PLL and RT kernel
2. ✅ Processing overhead fits within PlutoSDR ARM CPU capacity
3. ✅ Echo cancellation is computationally feasible (NLMS algorithm)
4. ✅ PlutoSDR hardware supports required frequency, bandwidth, and sample rate
5. ⚠️ Vocoder requires hardware or software solution (licensing consideration)

### 13.2 Performance Expectations

**Realistic Performance**:
- **Range**: 5-10 km with 1-5W PA (comparable to commercial handheld SFR)
- **Audio Quality**: Good (AMBE+2 vocoder, 40+ dB echo cancellation)
- **Latency**: 60-90 ms (acceptable for voice)
- **Reliability**: High (with proper implementation)

### 13.3 Critical Success Factors

1. **Precise Timing**: Software PLL locked to DMR frames
2. **Effective Echo Cancellation**: NLMS with 40+ dB ERLE
3. **Efficient Processing**: Multi-core optimization, <50% CPU load
4. **Vocoder Solution**: DVSI hardware (preferred) or md380_vocoder
5. **External PA**: 1-5W for practical range

### 13.4 Recommended Approach

**Phase 1: Core Functionality**:
1. Implement precise TDMA timing
2. Basic DMR modulation/demodulation
3. Timeslot-based TX/RX switching

**Phase 2: Audio Path**:
1. Integrate AMBE+2 vocoder (hardware or software)
2. Audio buffering and frame handling
3. Basic audio quality validation

**Phase 3: Echo Cancellation**:
1. Implement NLMS adaptive filter
2. Tune parameters for 40+ dB ERLE
3. Test with loopback and real radios

**Phase 4: Optimization**:
1. Multi-core processing distribution
2. Latency reduction
3. Range testing with external PA

---

## 14. References

### 14.1 Standards and Specifications

- ETSI TS 102 361-1 V2.6.1: DMR Air Interface
- ETSI TS 102 361-2 V2.5.1: Voice and Services
- DVSI AMBE+2 Vocoder Specifications

### 14.2 Technical Papers

- "An overview on optimized NLMS algorithms for acoustic echo cancellation" (EURASIP 2015)
- "Digital Self-Interference Cancellation for Full-Duplex Systems Based on CNN and GRU" (MDPI 2024)
- "Implementation of In-Band Full-Duplex Using Software Defined Radio with Adaptive Filter-Based Self-Interference Cancellation" (MDPI 2023)

### 14.3 Manufacturer Documentation

- Analog Devices AD9363 Datasheet
- ADALM-PLUTO Detailed Specifications
- Motorola MOTOTRBO Technical Specifications
- Hytera DMR Radio SFR Application Notes

---

**Document prepared for**: mmdvm-sdr project
**Related documents**:
- DMR_SFR_STANDARDS.md
- COMMERCIAL_IMPLEMENTATIONS.md
- SFR_FEASIBILITY_STUDY.md (next)
- SFR_IMPLEMENTATION_SPEC.md
