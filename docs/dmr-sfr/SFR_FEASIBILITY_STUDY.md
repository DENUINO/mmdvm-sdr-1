# DMR Single Frequency Repeater - PlutoSDR Feasibility Study

**Document Version:** 1.0
**Date:** 2025-11-16
**Project:** mmdvm-sdr DMR SFR Feasibility Study

---

## Executive Summary

### Feasibility Conclusion: ✅ **FEASIBLE with Caveats**

DMR Single Frequency Repeater (SFR) implementation on PlutoSDR is **technically feasible** but presents significant challenges. The PlutoSDR hardware has the necessary capabilities, but success depends on:

1. **Software timing precision** (achievable with RT kernel and careful design)
2. **Echo cancellation implementation** (computationally feasible with NLMS)
3. **Vocoder solution** (hardware codec recommended, software possible)
4. **External power amplifier** (1-5W for practical range)

**Confidence Level**: 75% - High confidence in technical feasibility, moderate confidence in achieving commercial-grade performance on first attempt.

**Recommended**: Proceed with phased implementation approach, starting with basic TDMA timing and progressing to full SFR functionality.

---

## 1. Hardware Capability Analysis

### 1.1 PlutoSDR/AD9363 Platform Assessment

#### 1.1.1 AD9363 RF Transceiver

| Capability | Requirement | AD9363 Specification | Assessment |
|-----------|-------------|---------------------|------------|
| **Frequency Range** | 400-480 MHz (UHF DMR) | 325 MHz - 3.8 GHz | ✅ **Exceeds** |
| **Bandwidth** | 12.5 kHz (DMR channel) | 200 kHz - 20 MHz | ✅ **Exceeds** (use 200 kHz) |
| **Sample Rate** | ≥48 kSPS (10× symbol rate) | Up to 61.44 MSPS | ✅ **Exceeds** |
| **TX Power** | ≥0 dBm (1 mW) | +7 dBm (5 mW) max | ✅ **Adequate** (external PA recommended) |
| **RX Sensitivity** | -100 dBm or better | -95 dBm typical | ⚠️ **Marginal** (adequate for testing) |
| **Full-Duplex** | Time-division capable | 1T1R FDD capable | ✅ **Yes** (with caveats) |

**Overall Hardware Assessment**: ✅ **PlutoSDR hardware is capable**

**Caveats**:
1. **TX Power**: +7 dBm (5 mW) is sufficient for testing but requires external PA (1-5W) for practical range
2. **VHF Coverage**: 136-174 MHz requires external mixer/upconverter or PlutoSDR+ variant
3. **RX Sensitivity**: Slightly worse than dedicated DMR radios but acceptable

#### 1.1.2 Full-Duplex Capability

**Question**: Can PlutoSDR operate full-duplex on the same frequency?

**Analysis**:

**Traditional Full-Duplex Limitation**:
> "Due to the design of the AD9364/AD9363, if both the TX and RX Local Oscillators (LO) are set to the same frequency or very close to each other, the TX LO may leak into the RX path."

**Impact on SFR**: ⚠️ **LO leakage is a concern BUT mitigated by SFR design**

**Mitigation in SFR**:
- SFR uses **time-division**, not simultaneous same-frequency operation
- TX is OFF during RX timeslot → No LO leakage during RX
- RX is OFF (or gated) during TX timeslot → No sensitivity issue
- Switching occurs during 2.5 ms guard time

**Conclusion**: ✅ **PlutoSDR's limitation with same-frequency full-duplex does NOT apply to SFR** because SFR uses time-division multiplexing, not true simultaneous TX/RX.

**Verification Method**:
```
RX Timeslot (0-30 ms):    TX LO = OFF,   RX LO = ON  → No leakage
Guard Time (30-32.5 ms):  Switching
TX Timeslot (32.5-60 ms): TX LO = ON,    RX LO = OFF → No interference
```

#### 1.1.3 TX-to-RX Isolation

**Internal Isolation** (AD9363 chip):
- Specification: >50 dB (based on AD9361 family)
- PCB-dependent: Actual may vary

**External Isolation** (system-level):
- Timeslot separation: **Effectively infinite** (TX off during RX)
- Optional antenna separation: +20-30 dB
- Optional filtering: +10-20 dB

**Total Isolation**: 50 dB (chip) + ∞ (timeslot) + 30 dB (antenna) = **>100 dB (sufficient)**

**Assessment**: ✅ **Isolation is adequate**

### 1.2 Processing Platform Analysis

#### 1.2.1 Zynq 7010 FPGA

| Specification | Value | SFR Requirement | Assessment |
|--------------|-------|-----------------|------------|
| **Logic Cells** | 28,000 | Moderate (filters, decimation) | ✅ Sufficient |
| **DSP Slices** | 80 | FIR filters, DDC/DUC | ✅ Sufficient |
| **Block RAM** | 2.1 Mb | Buffers, FIFOs | ✅ Sufficient |
| **Max Frequency** | 450 MHz | Digital processing | ✅ Sufficient |

**FPGA Utilization Estimate for SFR**:
- Digital downconversion/upconversion: 20-30%
- Decimation/interpolation filters: 20-30%
- Sample buffering: 10-20%
- **Total**: 50-80% (acceptable)

**Conclusion**: ✅ **FPGA has sufficient resources**

#### 1.2.2 ARM Cortex-A9 Processors

| Specification | Value | SFR Requirement | Assessment |
|--------------|-------|-----------------|------------|
| **Cores** | Dual-core | Multi-threaded processing | ✅ Excellent |
| **Clock Speed** | 667 MHz | DMR + echo cancel + vocoder | ✅ Sufficient |
| **Architecture** | ARMv7-A with NEON | SIMD for DSP | ✅ Beneficial |
| **L1 Cache** | 32 KB I + 32 KB D per core | Fast processing | ✅ Adequate |
| **L2 Cache** | 512 KB shared | Reduce memory latency | ✅ Good |

**CPU Load Estimate** (single core):
- DMR demodulation: 15-20%
- Frame decode/encode: 10-15%
- AMBE+2 vocoder (software): 20-30%
- Echo cancellation (NLMS): 5-10%
- Audio processing: 2-5%
- System overhead: 5-10%
- **Total**: 57-90% (tight on single core)

**Multi-Core Distribution**:
- **Core 0**: RX path (demod, decode, vocoder decode) → 35-50%
- **Core 1**: TX path (vocoder encode, encode, modulation) → 30-45%
- **Result**: ✅ **Comfortable margin** (40-50% per core)

**NEON SIMD Benefits**:
- Accelerate FIR filtering
- Speed up NLMS echo cancellation (vector operations)
- Optimize vocoder (if software AMBE+2)
- **Estimated speedup**: 2-4× for DSP operations

**Conclusion**: ✅ **ARM processors are sufficient**, especially with multi-core optimization and NEON SIMD

#### 1.2.3 Memory

| Resource | Available | Requirement | Assessment |
|----------|-----------|-------------|------------|
| **DDR3 RAM** | 512 MB | 25-50 MB (SFR) | ✅ Ample (10× headroom) |
| **Flash Storage** | 128 MB (typical) | <10 MB (code) | ✅ Sufficient |

**Memory Bandwidth**: 6.4 GB/s (sufficient for real-time DMR processing)

**Conclusion**: ✅ **Memory is not a bottleneck**

### 1.3 Clock Stability Analysis

#### 1.3.1 PlutoSDR Reference Oscillator

| Specification | PlutoSDR | DMR Requirement | Assessment |
|--------------|----------|-----------------|------------|
| **Reference Freq** | 40 MHz TCXO | Any stable reference | ✅ Good |
| **Frequency Stability** | ±1 to ±2 ppm | ≤0.5 ppm (preferred) | ⚠️ **Marginal** |
| **Temperature Stability** | ±2 ppm | ≤2 ppm | ✅ Meets minimum |

**Impact of ±2 ppm at 450 MHz UHF**:
- Frequency error: ±900 Hz
- DMR tolerance: Typically ±1 kHz
- **Conclusion**: ⚠️ **Acceptable but not ideal**

**Commercial DMR Standard**: 0.5 ppm

**Mitigation Options**:
1. **Software AFC** (Automatic Frequency Control):
   - Lock to received signal
   - Compensate for oscillator drift
   - **Effectiveness**: Corrects TX to match RX (good for SFR)

2. **External GPSDO**:
   - 10 MHz reference input (PlutoSDR supports)
   - Stability: 0.001 ppm or better
   - **Cost**: $100-300
   - **Benefit**: Professional-grade stability

**Conclusion**: ⚠️ **TCXO is marginal but workable**; GPSDO recommended for professional deployment

### 1.4 Hardware Summary

| Hardware Component | Status | Adequacy for SFR |
|-------------------|--------|-----------------|
| **AD9363 RF Transceiver** | ✅ Capable | Good with external PA |
| **Zynq 7010 FPGA** | ✅ Sufficient | 50-80% utilization |
| **ARM Cortex-A9 Dual-Core** | ✅ Sufficient | 40-50% load per core |
| **512 MB RAM** | ✅ Ample | 10× headroom |
| **40 MHz TCXO** | ⚠️ Marginal | Acceptable, GPSDO preferred |
| **Overall Hardware** | ✅ **Feasible** | Adequate for SFR implementation |

**Hardware Enhancements Recommended**:
1. **External PA** (1-5W): Required for practical range
2. **GPSDO** (0.01 ppm): Recommended for professional performance
3. **Separate TX/RX Antennas**: Improves isolation

**Hardware Enhancements Optional**:
- LNA (improve RX sensitivity)
- Band-pass filters (reduce interference)
- Heat sinks (extended TX duty cycle)

---

## 2. Software Feasibility Analysis

### 2.1 Timing Precision

#### 2.1.1 Requirement

**DMR TDMA Timing**:
- Timeslot duration: 30.000 ms ±0.001 ms
- Slot boundary precision: ±1 μs
- Frame sync accuracy: <10 μs

#### 2.1.2 Linux Timing Capabilities

**Standard Linux Kernel**:
- Timer resolution: 1-10 ms (HZ=100-1000)
- Jitter: 1-10 ms
- **Assessment**: ❌ **Insufficient** for μs-level timing

**Linux with PREEMPT_RT Patches**:
- Timer resolution: <10 μs
- Jitter: <100 μs
- **Assessment**: ⚠️ **Marginal** (may achieve ms-level, not μs-level)

**Hardware Timers + Software PLL**:
- Use PlutoSDR hardware timers (μs resolution)
- Software PLL locks to received DMR frame sync
- Maintain timeslot boundaries relative to received signal
- **Assessment**: ✅ **Achievable** (this is the key technique)

#### 2.1.3 Implementation Strategy

**Recommended Approach**:
1. **Lock to Received Signal**:
   - Detect DMR sync pattern in RX slot
   - Measure timeslot boundaries
   - Discipline TX timing to maintain offset

2. **Hardware Timer**:
   - Use Zynq ARM timer (32-bit, μs resolution)
   - Generate interrupts at timeslot boundaries
   - Trigger TX/RX switching

3. **Software PLL**:
   - Track timing drift
   - Adjust timer to maintain lock
   - Compensate for processing delays

**Feasibility**: ✅ **High** - This technique is proven in MMDVM and similar systems

**Verification**: Oscilloscope measurement of timeslot boundaries (should show <1 μs jitter)

### 2.2 Echo Cancellation

#### 2.2.1 NLMS Algorithm Feasibility

**Computational Requirement**:
- Filter length: 512 taps
- Sampling rate: 8 kHz
- Operations: 512 MACs per sample
- Total: 4.16 MMAC/s

**ARM Cortex-A9 Capability**:
- Peak performance: ~1,334 MMAC/s per core (with NEON)
- NLMS requirement: 4.16 MMAC/s
- **Utilization**: 0.3% per core

**Conclusion**: ✅ **Trivial computational load** - Echo cancellation is easily achievable

#### 2.2.2 ERLE Performance Prediction

**NLMS Algorithm**:
- Typical ERLE: 40-50 dB
- Convergence time: <500 ms
- Tracking capability: Good

**Expected Performance in SFR**:
- Digital cancellation: 40-50 dB
- Timeslot isolation: 30-40 dB (TX off during RX)
- Antenna separation: 20-30 dB (if used)
- **Total**: 90-120 dB (sufficient)

**Feasibility**: ✅ **High** - NLMS is well-understood and computationally lightweight

#### 2.2.3 Implementation Considerations

**Challenges**:
1. **Initial Convergence**: First 10-20 frames may have echo
2. **Non-linear Distortion**: PA non-linearity may reduce ERLE
3. **Parameter Tuning**: Step size, filter length require optimization

**Mitigation**:
- Pre-training: Initialize filter coefficients from prior session
- Adaptive step size: Faster convergence, lower steady-state error
- Non-linear echo cancellation: For PA distortion (advanced feature)

**Conclusion**: ✅ **Feasible** with standard techniques

### 2.3 Vocoder Solution

#### 2.3.1 AMBE+2 Requirements

**DMR Standard**: AMBE+2 vocoder (2,450 bit/s + FEC)

**Options**:

| Option | Approach | Pros | Cons | Feasibility |
|--------|----------|------|------|------------|
| **DVSI Hardware Codec** | USB-3000, AMBE chip | Standards-compliant, low CPU | Cost ($100-300), proprietary | ✅ High |
| **MD380 Vocoder (Software)** | Extracted from MD380 firmware | Free, no hardware | Licensing unclear, CPU load | ⚠️ Medium |
| **mbelib** | Open-source decoder only | Free, decode-only | No encode, incomplete | ❌ Insufficient |
| **Codec2** | Open-source alternative | Free, low bit rate | **Not DMR-compatible** | ❌ Non-standard |

**Recommended**: **DVSI Hardware Codec**
- Ensures standards compliance
- Low CPU overhead
- Proven quality
- Acceptable cost for professional deployment

**Alternative**: **MD380 Vocoder**
- For amateur/experimental use
- CPU load: 20-30% (acceptable)
- Legal gray area (reverse-engineered)

#### 2.3.2 Computational Feasibility (Software AMBE+2)

**If using md380_vocoder**:
- CPU load: 20-30% single core
- Multi-core: Distribute encode/decode to separate cores
- **Conclusion**: ✅ **Computationally feasible** on PlutoSDR ARM

**Concerns**:
- Licensing (unclear legal status)
- Quality (may not match DVSI hardware)
- Support (community-driven)

### 2.4 DMR Protocol Stack

#### 2.4.1 Existing Code Base

**MMDVM Project**:
- Mature DMR protocol implementation
- Supports Tier II DMR
- Handles frame sync, slot types, embedded signaling
- **License**: GPL v2 (open-source)

**Feasibility**: ✅ **High** - Reuse existing MMDVM code

**Required Modifications**:
1. **SFR-Specific Timing**:
   - RX on TS1, TX on TS2
   - Guard time management
   - Timeslot switching

2. **DMO Mode**:
   - Ensure DMO-compatible operation
   - Color code handling
   - Simplex channel configuration

**Effort**: Moderate (2-4 weeks for experienced developer)

#### 2.4.2 Integration Challenges

**Challenges**:
1. **Timing Coordination**: Synchronize MMDVM stack with hardware timers
2. **Buffer Management**: Align audio frames with TDMA slots
3. **Error Handling**: Manage lost frames, timing errors

**Mitigation**:
- Design clear interfaces between components
- Use ring buffers with overflow/underflow protection
- Implement robust state machines

**Feasibility**: ✅ **Achievable** with careful design

### 2.5 Software Summary

| Software Component | Feasibility | Confidence | Critical Path? |
|-------------------|-------------|-----------|---------------|
| **Timing Precision** | ✅ High | 85% | ✅ Yes |
| **Echo Cancellation** | ✅ High | 90% | Moderate |
| **Vocoder (Hardware)** | ✅ High | 95% | Moderate |
| **Vocoder (Software)** | ⚠️ Medium | 70% | Moderate |
| **DMR Protocol Stack** | ✅ High | 90% | ✅ Yes |
| **Overall Software** | ✅ **Feasible** | 80% | - |

**Critical Success Factors**:
1. **Precise timing implementation** (software PLL locked to received frames)
2. **Robust echo cancellation** (NLMS with proper tuning)
3. **Vocoder solution** (DVSI hardware recommended)

---

## 3. Performance Predictions

### 3.1 Range Estimates

#### 3.1.1 Link Budget Analysis

**Assumptions**:
- Frequency: 450 MHz (UHF)
- Environment: Urban (moderate obstacles)
- Antennas: Omnidirectional (0 dBi)

**Link Budget Components**:

| Parameter | Value | Notes |
|-----------|-------|-------|
| **TX Power (PlutoSDR)** | +7 dBm | Native output |
| **TX Antenna Gain** | 0 dBi | Omnidirectional |
| **EIRP** | +7 dBm | Effective radiated power |
| | | |
| **Free Space Path Loss** | See table | Frequency + distance dependent |
| **Urban Obstacles** | -10 to -30 dB | Buildings, terrain |
| **Fade Margin** | -10 dB | Safety margin |
| | | |
| **RX Antenna Gain** | 0 dBi | Omnidirectional |
| **RX Sensitivity** | -100 dBm | Typical DMR radio |

**Path Loss Table (450 MHz, Free Space)**:

| Distance | Free Space Loss | Total Loss (Urban) | Received Power | Margin |
|----------|----------------|-------------------|----------------|--------|
| **1 km** | -81 dB | -101 dB | -94 dBm | +6 dB ✅ |
| **2 km** | -87 dB | -107 dB | -100 dBm | 0 dB ⚠️ |
| **5 km** | -95 dB | -115 dB | -108 dBm | -8 dB ❌ |

**With 1W (+30 dBm) External PA**:

| Distance | Free Space Loss | Total Loss (Urban) | Received Power | Margin |
|----------|----------------|-------------------|----------------|--------|
| **1 km** | -81 dB | -101 dB | -71 dBm | +29 dB ✅ |
| **3 km** | -91 dB | -111 dB | -81 dBm | +19 dB ✅ |
| **5 km** | -95 dB | -115 dB | -85 dBm | +15 dB ✅ |
| **10 km** | -101 dB | -121 dB | -91 dBm | +9 dB ✅ |
| **20 km** | -107 dB | -127 dB | -97 dBm | +3 dB ⚠️ |
| **30 km** | -111 dB | -131 dB | -101 dBm | -1 dB ❌ |

**With 5W (+37 dBm) External PA**:

| Distance | Free Space Loss | Total Loss (Urban) | Received Power | Margin |
|----------|----------------|-------------------|----------------|--------|
| **10 km** | -101 dB | -121 dB | -84 dBm | +16 dB ✅ |
| **20 km** | -107 dB | -127 dB | -90 dBm | +10 dB ✅ |
| **30 km** | -111 dB | -131 dB | -94 dBm | +6 dB ✅ |
| **40 km** | -113 dB | -133 dB | -96 dBm | +4 dB ⚠️ |
| **50 km** | -115 dB | -135 dB | -98 dBm | +2 dB ⚠️ |

**TDMA Timing Limit**: 30-50 km (processing delays reduce theoretical 150 km limit)

#### 3.1.2 Predicted Range

**PlutoSDR Native (+7 dBm)**:
- **Urban**: 0.5-1 km
- **Suburban**: 1-2 km
- **Rural/LOS**: 2-3 km
- **Assessment**: ⚠️ **Marginal** - Suitable for testing only

**With 1W PA (+30 dBm)**:
- **Urban**: 3-5 km
- **Suburban**: 5-10 km
- **Rural/LOS**: 10-15 km
- **Assessment**: ✅ **Good** - Comparable to handheld SFR

**With 5W PA (+37 dBm)**:
- **Urban**: 5-10 km
- **Suburban**: 10-20 km
- **Rural/LOS**: 20-30 km
- **Assessment**: ✅ **Excellent** - Comparable to mobile SFR

**Conclusion**: External PA (1-5W) is **essential** for practical SFR deployment.

### 3.2 Audio Quality Predictions

#### 3.2.1 Factors Affecting Quality

| Factor | Impact | Expected Performance |
|--------|--------|---------------------|
| **Vocoder Quality** | High | Good (if AMBE+2) |
| **Echo Cancellation** | Medium | 40-50 dB ERLE (good) |
| **Timing Jitter** | Low | <1 μs (negligible) |
| **Processing Artifacts** | Low | Minimal (headroom) |

#### 3.2.2 Predicted MOS Score

**MOS (Mean Opinion Score)**: Subjective audio quality (1-5 scale)

**Predicted MOS**:
- **Ideal conditions**: 4.0-4.2 (good)
- **Typical conditions**: 3.5-4.0 (acceptable to good)
- **Weak signal**: 2.5-3.5 (acceptable)

**Comparison**:
- Commercial SFR: 3.5-4.0 (comparable)
- Traditional repeater: 4.0-4.5 (slightly better)
- Analog FM: 3.0-4.0 (similar)

**Conclusion**: ✅ **Audio quality should be comparable to commercial SFR**

### 3.3 Latency Predictions

**End-to-End Latency Breakdown**:

| Stage | Latency | Notes |
|-------|---------|-------|
| **RX Timeslot** | 30 ms | Fixed (TDMA structure) |
| **Demodulation** | 5 ms | Parallel to RX |
| **Frame Decode** | 5 ms | DMR protocol |
| **Vocoder Decode** | 60 ms | 3 × 20 ms frames |
| **Audio Processing** | 5 ms | Optional processing |
| **Vocoder Encode** | 60 ms | 3 × 20 ms frames |
| **Frame Encode** | 3 ms | DMR protocol |
| **TX Timeslot** | 30 ms | Fixed (TDMA structure) |
| **Total** | ~150-180 ms | End-to-end |

**Comparison**:
- **Traditional repeater**: 50-100 ms
- **Commercial SFR**: 120-200 ms
- **Predicted mmdvm-sdr SFR**: 150-180 ms

**User Impact**:
- <200 ms: Generally acceptable for voice
- Voice-operated switching (VOX) may feel slightly delayed
- Push-to-talk (PTT) operation: No perceptible impact

**Conclusion**: ✅ **Latency is acceptable** for DMR voice communications

### 3.4 Reliability Predictions

**Expected Reliability**:

| Component | Predicted Reliability | Failure Mode |
|-----------|----------------------|--------------|
| **Timing Sync** | 95-98% | Occasional frame slip |
| **Echo Cancellation** | 90-95% | Residual echo in weak conditions |
| **Vocoder** | 98-99% (HW) / 90-95% (SW) | Frame errors, artifacts |
| **Modulation/Demod** | 95-98% | Weak signal performance |
| **Overall System** | 85-90% | Combined factors |

**Failure Scenarios**:
1. **Timing drift**: Requires periodic resync
2. **Echo cancellation convergence**: First 10 frames may have echo
3. **Vocoder errors**: Weak signals may cause artifacts
4. **Processing overruns**: Under heavy CPU load

**Mitigation**:
- Continuous timing tracking
- Fast echo canceller re-convergence
- FEC in DMR provides error resilience
- Real-time OS with priority scheduling

**Conclusion**: ✅ **Reliability should be acceptable** for amateur/experimental use, with optimization needed for commercial-grade

---

## 4. Comparison with Commercial Products

### 4.1 Feature Comparison

| Feature | Commercial SFR | PlutoSDR SFR (Predicted) | Assessment |
|---------|---------------|-------------------------|------------|
| **Range (1W)** | 5-10 km | 3-10 km | ⚠️ Comparable |
| **Range (5W)** | 20-30 km | 20-30 km | ✅ Comparable |
| **Audio Quality** | Good (MOS 3.5-4.0) | Good (MOS 3.5-4.0) | ✅ Comparable |
| **Latency** | 120-200 ms | 150-180 ms | ✅ Comparable |
| **Frequency Stability** | 0.5 ppm | 1-2 ppm (TCXO) | ⚠️ Slightly worse |
| **TX Power** | 1-40W | 0.005W (requires ext PA) | ❌ Requires PA |
| **Form Factor** | Compact, integrated | Development board + PA | ❌ Larger |
| **Cost** | $500-2000 | $150 (PlutoSDR) + $100-300 (PA + codec) | ✅ Lower cost |
| **Standards Compliance** | ✅ Full | ✅ Full (with AMBE+2) | ✅ Equal |

### 4.2 Strengths vs. Commercial Products

**Advantages of mmdvm-sdr SFR**:
1. **Lower cost**: $250-450 total vs. $500-2000 commercial
2. **Open-source**: Modifiable, customizable
3. **SDR flexibility**: Can support multiple modes (DMR, D-Star, YSF, etc.)
4. **Educational value**: Understanding of DMR internals

**Disadvantages vs. Commercial Products**:
1. **Requires assembly**: Not plug-and-play
2. **Form factor**: Larger, less integrated
3. **Frequency stability**: Marginal without GPSDO
4. **Support**: Community vs. commercial vendor

### 4.3 Use Case Suitability

**Recommended Use Cases**:
- ✅ Amateur radio experimentation
- ✅ Educational projects
- ✅ Low-cost temporary repeater
- ✅ Multi-mode hotspot/repeater
- ⚠️ Small-scale commercial (with external PA and GPSDO)
- ❌ Mission-critical applications

**Not Recommended For**:
- ❌ Public safety (use commercial)
- ❌ Large-scale commercial networks (use commercial)
- ❌ Unattended operation without monitoring (reliability concerns)

---

## 5. Risk Assessment

### 5.1 Technical Risks

| Risk | Probability | Impact | Mitigation | Residual Risk |
|------|------------|--------|------------|--------------|
| **Timing synchronization failure** | Medium | High | Software PLL, RT kernel, testing | Low |
| **Insufficient echo cancellation** | Low | Medium | NLMS algorithm, tuning, separate antennas | Low |
| **Vocoder licensing issues** | Medium | High | Use DVSI hardware | Low |
| **Processing overruns** | Low | Medium | Multi-core optimization, profiling | Low |
| **Frequency instability** | Medium | Medium | GPSDO, AFC | Low-Medium |
| **Hardware damage (PA)** | Low | Medium | Proper PA control, testing | Low |

### 5.2 Implementation Risks

| Risk | Probability | Impact | Mitigation | Residual Risk |
|------|------------|--------|------------|--------------|
| **Development time exceeds estimate** | High | Medium | Phased approach, incremental development | Medium |
| **Integration challenges** | Medium | Medium | Modular design, clear interfaces | Low-Medium |
| **Debugging difficulty** | Medium | Low | Good logging, test equipment | Low |
| **Performance below expectations** | Medium | Medium | Conservative design, optimization phase | Low-Medium |

### 5.3 Overall Risk Level

**Overall Project Risk**: ⚠️ **Medium**

**Justification**:
- Technical feasibility is high (hardware and software capable)
- Implementation complexity is moderate (reuse existing code)
- Performance may be slightly below commercial products initially
- Risk can be managed through phased development

**Recommendation**: ✅ **Proceed with project**, using phased approach to manage risks

---

## 6. Alternative Approaches

### 6.1 Dual PlutoSDR Approach

**Concept**: Use two PlutoSDR units (one for RX, one for TX)

**Advantages**:
- Eliminates TX/RX switching complexity
- Avoids LO leakage issue completely
- Separate optimization for TX and RX
- Better isolation (separate hardware)

**Disadvantages**:
- Higher cost (2× PlutoSDR)
- Increased complexity (synchronization between units)
- Larger form factor
- Still requires external PA for TX

**Assessment**: ⚠️ **Possible but unnecessary** - Single PlutoSDR is sufficient for SFR

### 6.2 Different SDR Platform

**Alternatives**:

| Platform | Pros | Cons | Assessment |
|----------|------|------|------------|
| **LimeSDR** | Higher TX power (10 dBm), 2×2 MIMO | More expensive, larger | ⚠️ Possible |
| **USRP B210** | High performance, 2×2 MIMO | Very expensive ($1000+) | ❌ Overkill |
| **HackRF** | Low cost | Low TX power, half-duplex only | ❌ Insufficient |
| **RTL-SDR** | Very low cost | RX only | ❌ Insufficient |

**Conclusion**: PlutoSDR offers best price/performance for SFR application

### 6.3 Hybrid Approach (SDR + Hardware Codec)

**Recommended Configuration**:
- PlutoSDR: RF transceiver, DMR modem
- DVSI AMBE+2 codec: Vocoder (USB or chip)
- External PA: 1-5W RF amplifier
- Optional GPSDO: Frequency stability

**Benefits**:
- Standards-compliant vocoder (DVSI)
- Lower CPU load (hardware codec)
- Professional audio quality
- Modular design

**Cost**: $150 (PlutoSDR) + $200 (codec) + $100 (PA) + $200 (GPSDO) = **$650**

**Assessment**: ✅ **Recommended approach** for best performance

---

## 7. Development Effort Estimate

### 7.1 Phased Development Plan

**Phase 1: Basic TDMA Operation** (4-6 weeks)
- Implement precise timeslot timing
- DMR frame sync and demodulation
- TX/RX timeslot switching
- **Deliverable**: Loopback test (RX TS1, TX TS2 on same unit)

**Phase 2: Audio Path Integration** (3-4 weeks)
- Integrate AMBE+2 vocoder (hardware or software)
- Audio buffering and frame handling
- Basic end-to-end audio path
- **Deliverable**: Audio throughput test (RX → vocoder → TX)

**Phase 3: Echo Cancellation** (2-3 weeks)
- Implement NLMS adaptive filter
- Parameter tuning and optimization
- Integration with audio path
- **Deliverable**: Echo cancellation performance test (ERLE >40 dB)

**Phase 4: DMR Protocol Stack** (3-4 weeks)
- Integrate MMDVM DMR code
- Implement DMO mode
- Color code and signaling
- **Deliverable**: Interoperability test with commercial radios

**Phase 5: Optimization and Testing** (4-6 weeks)
- Multi-core optimization
- Latency reduction
- Range testing with external PA
- **Deliverable**: Field-tested SFR

**Total Estimated Effort**: 16-23 weeks (4-6 months)

**Assumptions**:
- Single experienced developer
- Part-time effort (20 hours/week)
- Existing MMDVM code reuse

**Full-Time Effort**: 8-12 weeks (2-3 months)

### 7.2 Skill Requirements

**Required Skills**:
1. **Embedded Linux development** (high proficiency)
2. **Digital signal processing** (intermediate proficiency)
3. **Real-time programming** (intermediate proficiency)
4. **RF and radio fundamentals** (intermediate proficiency)
5. **DMR protocol knowledge** (basic proficiency, can learn)

**Nice to Have**:
- FPGA programming (Verilog/VHDL)
- NEON SIMD optimization
- GNURadio experience
- Amateur radio license (for testing)

### 7.3 Tools and Equipment

**Essential**:
- PlutoSDR ($150)
- Linux development PC
- DVSI AMBE codec ($100-300) or md380_vocoder
- DMR radio (for testing) ($100-300 used)

**Recommended**:
- Oscilloscope (timing verification)
- Spectrum analyzer (signal quality)
- External PA 1-5W ($100-200)
- GPSDO ($100-300)

**Optional**:
- Logic analyzer (debugging)
- Second PlutoSDR (comparison testing)
- Additional DMR radios

**Total Equipment Cost**: $450-1,300

---

## 8. Success Criteria

### 8.1 Minimum Viable Product (MVP)

**MVP Definition**: SFR that successfully retransmits DMR signals

**MVP Success Criteria**:
1. ✅ Receive DMR signal on TS1
2. ✅ Decode and demodulate DMR
3. ✅ Re-encode and modulate for TS2
4. ✅ Transmit on TS2 (same frequency)
5. ✅ Timing accuracy <10 μs
6. ✅ Audio quality acceptable (MOS >3.0)

**MVP Range**: 1-3 km (with 1W PA)

### 8.2 Target Product

**Target Definition**: SFR comparable to low-end commercial products

**Target Success Criteria**:
1. ✅ Range 5-10 km (with 1-5W PA)
2. ✅ Audio quality MOS >3.5
3. ✅ Echo cancellation ERLE >40 dB
4. ✅ Interoperable with commercial DMR radios (Motorola, Hytera, etc.)
5. ✅ Timing accuracy <1 μs
6. ✅ Reliability >90% (continuous operation)
7. ✅ Latency <200 ms

### 8.3 Stretch Goals

**Advanced Features** (beyond initial scope):
1. Dual-timeslot SFR (RX TS1→TX TS2, RX TS2→TX TS1)
2. Trunking support (DMR Tier III)
3. Multi-frequency capability
4. Remote monitoring and control
5. Advanced echo cancellation (non-linear, deep learning)

---

## 9. Conclusions

### 9.1 Overall Feasibility Assessment

**Conclusion**: ✅ **DMR Single Frequency Repeater implementation on PlutoSDR is FEASIBLE**

**Confidence Level**: 75%

**Justification**:
1. ✅ **Hardware is capable**: PlutoSDR has sufficient RF performance, processing power, and memory
2. ✅ **Software is achievable**: Timing, echo cancellation, and vocoder are computationally feasible
3. ✅ **Proven techniques**: Commercial products demonstrate viability
4. ⚠️ **Challenges exist**: Timing precision, vocoder licensing, performance optimization required
5. ✅ **Risks are manageable**: Phased development approach mitigates implementation risks

### 9.2 Recommended Path Forward

**Recommendation**: ✅ **Proceed with phased implementation**

**Phase 1 Priority**: Timing and basic TDMA operation
- This is the highest-risk component
- Demonstrates core feasibility
- Builds foundation for all other features

**Quick Wins**:
1. Implement TDMA frame generation
2. Test TX/RX switching timing
3. Verify timeslot alignment
4. **Decision Point**: If timing is achievable, proceed to Phase 2

### 9.3 Key Success Factors

**Critical for Success**:
1. **Precise timing implementation** → Software PLL locked to received DMR frames
2. **External PA** → 1-5W for practical range
3. **Vocoder solution** → DVSI hardware codec (recommended)
4. **Real-time OS** → Linux with PREEMPT_RT or bare-metal
5. **Multi-core optimization** → Distribute processing across both ARM cores

**Nice to Have**:
- GPSDO for frequency stability
- Separate TX/RX antennas for isolation
- Advanced echo cancellation algorithms

### 9.4 Expected Outcomes

**Best Case** (90% of goals achieved):
- Range: 10-15 km (with 5W PA)
- Audio quality: MOS 3.8-4.2
- Reliability: 95%+
- Comparable to commercial SFR

**Likely Case** (75% of goals achieved):
- Range: 5-10 km (with 1-5W PA)
- Audio quality: MOS 3.5-4.0
- Reliability: 85-90%
- Usable for amateur/experimental deployment

**Worst Case** (50% of goals achieved):
- Range: 1-3 km
- Audio quality: MOS 3.0-3.5
- Reliability: 70-80%
- Useful for testing/development only

**Most Probable Outcome**: **Likely Case** → Functional SFR suitable for amateur radio use

### 9.5 Final Recommendation

**Recommendation to Project Team**:

✅ **APPROVE** DMR Single Frequency Repeater development for mmdvm-sdr project

**Rationale**:
1. Technical feasibility is high
2. Cost is reasonable ($450-650 total)
3. Educational value is significant
4. Potential to contribute to open-source amateur radio community
5. Phased approach manages risk effectively

**Conditions**:
1. Use DVSI hardware codec for standards compliance
2. Budget for external PA (1-5W)
3. Plan for 4-6 months development time
4. Target amateur/experimental use case (not commercial)

**Next Step**: Begin Phase 1 implementation (timing and basic TDMA)

---

## 10. References

### 10.1 Standards and Specifications

- ETSI TS 102 361-1 V2.6.1: DMR Air Interface Protocol
- DVSI AMBE+2 Vocoder Specifications
- Analog Devices AD9363 Datasheet

### 10.2 Commercial Products

- Motorola MOTOTRBO SLR Series
- Hytera PD98X Specifications
- Belfone BF-SFR600

### 10.3 Research Papers

- "An overview on optimized NLMS algorithms for acoustic echo cancellation" (EURASIP 2015)
- "Implementation of In-Band Full-Duplex Using SDR with Adaptive Filter-Based Self-Interference Cancellation" (MDPI 2023)

### 10.4 Open Source Projects

- MMDVM: https://github.com/g4klx/MMDVM
- md380_vocoder: https://github.com/nostar/md380_vocoder
- Codec2: http://www.rowetel.com/codec2.html

---

**Document prepared for**: mmdvm-sdr project
**Related documents**:
- DMR_SFR_STANDARDS.md
- COMMERCIAL_IMPLEMENTATIONS.md
- SFR_TECHNICAL_REQUIREMENTS.md
- SFR_IMPLEMENTATION_SPEC.md (next)

**Feasibility Study Status**: ✅ **COMPLETE**

**Recommendation**: **PROCEED** with implementation
