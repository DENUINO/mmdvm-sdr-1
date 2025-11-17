# DMR Single Frequency Repeater (SFR) Standards Analysis

**Document Version:** 1.0
**Date:** 2025-11-16
**Project:** mmdvm-sdr DMR SFR Feasibility Study

---

## Executive Summary

This document provides a comprehensive analysis of DMR standards relevant to Single Frequency Repeater (SFR) implementation, based on the ETSI TS 102 361 standard series. DMR SFR operation is supported within the DMR standard framework through the use of TDMA timeslot multiplexing, allowing both receive and transmit functions on a single frequency.

---

## 1. ETSI TS 102 361 Standard Overview

### 1.1 Standard Structure

The DMR interface is defined by the following ETSI standards:

- **TS 102 361-1**: Air interface (AI) protocol - Physical Layer (PL) and Data Link Layer (DLL)
- **TS 102 361-2**: Voice and general services and facilities
- **TS 102 361-3**: Data protocol
- **TS 102 361-4**: Trunking protocol

**Latest Version**: ETSI TS 102 361-1 V2.6.1 (2023-05)

**Official Download**: https://www.etsi.org/deliver/etsi_ts/102300_102399/10236101/02.06.01_60/ts_10236101v020601p.pdf

### 1.2 Standard Development

The DMR standard was developed by ETSI (European Telecommunications Standards Institute) to provide:
- Digital voice and data communications
- Efficient spectrum usage through TDMA
- Backward compatibility with existing 12.5 kHz channel spacing
- Support for both simplex and duplex operation modes

---

## 2. DMR System Tiers

### 2.1 Tier I - Unlicensed Direct Mode

- Integral antenna operation
- Direct mode only (no infrastructure)
- Limited power output
- Simplex operation

### 2.2 Tier II - Conventional Licensed

- Operates under individual licenses
- Direct mode OR repeater mode
- Can use Base Station (BS) for repeating
- **Supports both dual-frequency and single-frequency repeaters**
- Most commercial DMR equipment operates at this tier

### 2.3 Tier III - Trunked Systems

- Automatic controller functions
- Dynamic channel allocation
- Advanced features (priority, preemption)
- Uses same TDMA structure as Tier II

**Note**: Single Frequency Repeater operation is primarily implemented in Tier II systems.

---

## 3. Physical Layer Specifications

### 3.1 Channel Characteristics

| Parameter | Specification |
|-----------|--------------|
| Channel Spacing | 12.5 kHz |
| Modulation | 4-FSK |
| Symbol Rate | 4,800 symbols/s |
| Bit Rate | 9,600 bit/s |
| Voice Channels per Carrier | 2 (via TDMA) |

### 3.2 Modulation - 4-State FSK

- Creates four possible symbols transmitted at 4,800 symbols/s
- Achieves 9,600 bit/s data rate
- Efficient spectrum usage within existing 12.5 kHz allocations
- Compatible with existing channel plans globally

---

## 4. TDMA Frame Structure

### 4.1 Basic Timing

| Element | Duration |
|---------|----------|
| Timeslot Duration | 30 ms |
| Frame Duration | 60 ms (2 timeslots) |
| Guard Time | 2.5 ms per slot |
| PA Ramp-Up Time | 1.25 ms |
| PA Ramp-Down Time | 1.25 ms |
| Propagation Delay Allowance | ~1 ms |

**Critical for SFR**: The 30 ms timeslot structure enables full-duplex operation on a single frequency by time-division multiplexing.

### 4.2 Slot Numbering

- Slots are numbered "1" and "2"
- Each slot can carry independent voice or data traffic
- Slots are synchronized to frame boundaries
- Repeaters maintain strict timing synchronization

### 4.3 Guard Time Budget

The 2.5 ms guard time between timeslots is allocated as follows:

1. **1.25 ms** - Transmitter ramp-up at start of slot
2. **1.25 ms** - Transmitter ramp-down at end of slot
3. **~1.0 ms** - Signal propagation delay allowance

This guard time is critical for SFR operation as it provides the window for TX/RX switching.

---

## 5. Single Frequency Repeater Support in Standard

### 5.1 Standard Definition

The DMR standard explicitly supports single frequency operation:

> "The RF carrier may be a single frequency or a duplex pair of frequencies, and the physical channel may be a single frequency or a duplex spaced pair of frequencies."
>
> — ETSI TS 102 361-1

### 5.2 Operating Principle

**Standard DMR Repeater (Dual Frequency)**:
- Receives on Frequency A
- Transmits on Frequency B
- Requires duplexer or separate antennas

**Single Frequency Repeater (SFR)**:
- Receives on Timeslot 1, Frequency A
- Transmits on Timeslot 2, Frequency A
- No duplexer required
- Requires fast TX/RX switching

### 5.3 Repeater Mode Definition

From ETSI TS 102 361-1:

> "Repeater mode is defined as the mode of operation where MSs (Mobile Stations) may communicate through a BS (Base Station)."

The standard includes detailed specifications for:
- Repeater mode BS established timing relationship
- Synchronization requirements
- Frame alignment procedures

---

## 6. Timing Synchronization Requirements

### 6.1 Frequency Stability

| Equipment Type | Frequency Accuracy |
|----------------|-------------------|
| Mobile/Portable Terminals | 0.5 ppm (typical) |
| Base Stations/Repeaters | 0.5 ppm or better |
| Reference Oscillator Drift | ±2 ppm (used in calculations) |

**Source**: Multiple DMR vendor specifications indicate 0.5 ppm as the industry standard for frequency accuracy.

### 6.2 Timing Accuracy

The standard specifies precise timing relationships:

- **Slot timing accuracy**: Critical for TDMA operation
- **Frame synchronization**: Maintained by base station
- **Mobile synchronization**: Adjusted based on propagation delay
- **Dynamic timing adjustment**: System compensates for distance-dependent delays

### 6.3 Distance-Based Timing Compensation

DMR TDMA implements dynamic timing adjustment based on propagation distance to maintain synchronization across different mobile station locations.

---

## 7. Distance Limitations for SFR

### 7.1 Theoretical Maximum Range

The ETSI DMR specification defines:
- **Propagation allowance**: 1 ms (within the guard time)
- **Signal propagation speed**: ~300 m/μs (speed of light)
- **Maximum theoretical distance**: 150 km

**Calculation**:
```
1 ms propagation allowance = 1000 μs
At 300 m/μs: 1000 μs × 300 m/μs = 300 km (round trip)
One-way distance: 300 km / 2 = 150 km
```

### 7.2 Practical Limitations for SFR

While standard dual-frequency repeaters can approach the 150 km theoretical limit, **Single Frequency Repeaters face additional constraints**:

**SFR-Specific Limitations**:
1. **Processing Delay**: Time required to decode RX slot and re-encode for TX slot
2. **TX/RX Switching Time**: Hardware switching latency
3. **Echo/Self-Interference**: Near-end users may be affected by repeater TX

**Practical SFR Range**: 30-50 km
**Reported Field Performance**: 7-10 km in metropolitan environments

### 7.3 Propagation Delay Impact

For each km of distance:
- **One-way delay**: ~3.33 μs/km
- **Round-trip delay**: ~6.67 μs/km
- **At 150 km**: ~1000 μs (1 ms) - consumes entire propagation allowance
- **At 30 km**: ~200 μs - leaves margin for processing

---

## 8. Vocoder Specifications

### 8.1 AMBE+2 Vocoder (Standard)

| Parameter | Specification |
|-----------|--------------|
| Vocoder Type | AMBE+2 (proprietary, DVSI) |
| Voice Bit Rate | 2,450 bit/s |
| Total with FEC | 3,600 bit/s |
| Frame Duration | 20 ms |
| Sampling Rate | 8 kHz |
| Frames per Burst | 3 (3 × 20 ms = 60 ms) |
| Sample Format | S16LE (signed 16-bit) |

### 8.2 Frame Structure

Each 30 ms DMR timeslot carries:
- **Voice payload**: 3 × 72-bit vocoder frames
- **Forward Error Correction**: Integrated
- **Signaling**: Embedded in voice frames

**Total vocoder latency**: Approximately 20 ms per frame for encoding/decoding

---

## 9. Standard Compliance Requirements for SFR

### 9.1 Mandatory Requirements

A DMR SFR implementation must comply with:

1. **Air Interface Protocol** (TS 102 361-1)
   - Correct 4-FSK modulation
   - Proper TDMA frame structure
   - Accurate slot timing
   - Frequency stability requirements

2. **Voice Services** (TS 102 361-2)
   - AMBE+2 vocoder (or compatible)
   - Voice frame formatting
   - Embedded signaling

3. **Data Protocol** (TS 102 361-3) - if data supported
   - Data packet structure
   - Error correction

### 9.2 SFR-Specific Requirements

Beyond standard DMR requirements, SFR operation requires:

1. **Fast TX/RX Switching**
   - Switch time must fit within guard time (~2.5 ms)
   - Typical commercial implementations: <1 ms

2. **Self-Interference Cancellation**
   - Digital echo cancellation
   - RF isolation between TX and RX paths
   - Interference mitigation algorithms

3. **Processing Latency Budget**
   - Receive Slot 1 (30 ms)
   - Decode, process, re-encode
   - Transmit Slot 2 (30 ms)
   - Total available: 30 ms for turnaround processing

4. **Timing Synchronization**
   - Maintain frame alignment
   - Compensate for processing delays
   - Accurate slot boundaries

---

## 10. DMO (Direct Mode Operation) for SFR

### 10.1 DMO Definition

Direct Mode Operation (DMO) is defined in ETSI TS 102 361-1 as:
- Mobile-to-mobile communication without infrastructure
- Typically simplex (same frequency for all stations)
- Single timeslot normally used

### 10.2 SFR and DMO Relationship

Single Frequency Repeaters operate as an **extension of DMO**:

1. **Mobile stations operate in DMO mode**
   - Transmit on Timeslot 1
   - Receive on Timeslot 2 (from repeater)
   - Use same frequency for TX and RX

2. **SFR bridges DMO communications**
   - Receives DMO transmissions (Slot 1)
   - Retransmits on alternate slot (Slot 2)
   - Extends range of DMO network

This is fundamentally different from traditional repeater operation where mobiles use offset frequencies.

---

## 11. Color Code and Network Access

### 11.1 Color Code

DMR uses a "Color Code" (0-15) for:
- Network identification
- Interference rejection
- Co-channel operation

**Color Code is maintained** in SFR operation:
- Received signals must match repeater Color Code
- Retransmitted signals use same Color Code
- Enables multiple SFR networks on same frequency in different areas

### 11.2 Embedded Signaling

DMR embeds signaling in voice frames:
- **CACH** (Common Announcement Channel): Carries short data
- **Embedded LC** (Link Control): Provides call management
- **Slot Type**: Identifies data content

SFR must properly decode, preserve, and retransmit this signaling.

---

## 12. Standard Differences from Traditional Repeaters

### 12.1 Traditional DMR Repeater

| Aspect | Traditional Repeater |
|--------|---------------------|
| RX Frequency | F1 (e.g., 439.000 MHz) |
| TX Frequency | F2 (e.g., 434.000 MHz) |
| Timeslots | Both slots on each frequency |
| Duplexer | Required |
| Mobile Operation | Offset mode (different TX/RX) |

### 12.2 Single Frequency Repeater (SFR)

| Aspect | SFR |
|--------|-----|
| RX Frequency | F (e.g., 439.000 MHz) |
| TX Frequency | F (same - 439.000 MHz) |
| RX Timeslot | Slot 1 only |
| TX Timeslot | Slot 2 only |
| Duplexer | Not required |
| Mobile Operation | DMO mode (same frequency) |
| Isolation | Digital cancellation + fast switching |

---

## 13. Standards References

### 13.1 Primary Standards

1. **ETSI TS 102 361-1 V2.6.1 (2023-05)**
   - Digital Mobile Radio (DMR) Systems
   - Part 1: DMR Air Interface (AI) protocol
   - URL: https://www.etsi.org/deliver/etsi_ts/102300_102399/10236101/02.06.01_60/ts_10236101v020601p.pdf

2. **ETSI TS 102 361-2 V2.5.1 (2023-05)**
   - Part 2: Voice and Generic Services and Facilities
   - URL: https://www.dmrassociation.org/downloads/standards/ts_10236102v020401p.pdf

3. **ETSI TS 102 361-3 V1.3.1 (2017-10)**
   - Part 3: DMR Data protocol

4. **ETSI TS 102 361-4 V1.12.1 (2023-07)**
   - Part 4: DMR Trunking protocol

### 13.2 Related Standards

1. **ETSI TR 102 398** - DMR Performance Specifications
2. **ITU-R SM.337-6** - Frequency and distance separations

### 13.3 Industry Organizations

- **DMR Association**: https://www.dmrassociation.org/
  - Maintains DMR standards documentation
  - Provides public access to ETSI TS 102 361 series
  - Coordinates industry interoperability testing

---

## 14. Conclusions

### 14.1 Standard Support for SFR

The ETSI TS 102 361 DMR standard **explicitly supports single frequency operation** for both direct mode and repeater mode. The standard's TDMA structure with 30 ms timeslots is well-suited for SFR implementation.

### 14.2 Key Standard Requirements

For compliant SFR implementation:

1. **Timing is critical**
   - Must maintain 30 ms timeslot boundaries
   - Processing must fit within slot duration
   - Guard time provides switching window

2. **Frequency stability**
   - 0.5 ppm or better required
   - Maintains synchronization across network
   - Enables maximum range operation

3. **Standard modulation and framing**
   - 4-FSK at 4,800 symbols/s
   - Proper TDMA frame structure
   - Correct slot formatting

4. **AMBE+2 vocoder**
   - Standard requires specific vocoder
   - Frame timing must match standard
   - Quality parameters defined

### 14.3 Standards Compliance Challenges

SFR implementation faces these standards-related challenges:

1. **Processing latency budget**
   - 30 ms slot duration
   - Must decode, process, re-encode
   - Commercial implementations achieve this

2. **Timing synchronization**
   - Maintain precise slot boundaries
   - Compensate for propagation delays
   - Dynamic range limitations (30-50 km practical)

3. **Self-interference**
   - Not explicitly addressed in standard
   - Requires vendor-specific implementation
   - Digital cancellation algorithms needed

### 14.4 Standard-Compliant Alternatives

While AMBE+2 is the DMR standard vocoder, open-source alternatives exist:

- **Codec2** (VK5DGR): Open-source vocoder
  - **Not DMR-compatible** (would break standard compliance)
  - Used in M17 digital radio system
  - Half the bandwidth of AMBE+2 for similar quality

For standards-compliant DMR SFR:
- Must use AMBE+2 or obtain DVSI licensing
- Software implementations exist (md380_vocoder)
- Hardware codec chips available (AMBE-2020, AMBE-3003)

---

## 15. Recommendations

### 15.1 For Standards-Compliant Implementation

1. Obtain or reference ETSI TS 102 361-1 V2.6.1 for detailed timing specifications
2. Implement precise TDMA frame generation and synchronization
3. Use licensed AMBE+2 vocoder or DVSI hardware
4. Ensure frequency stability ≤0.5 ppm
5. Implement proper Color Code and embedded signaling

### 15.2 For Experimental/Amateur Implementation

1. Consider Codec2 for open-source implementation (non-standard)
2. Focus on timing and synchronization first
3. Implement basic DMO mode before SFR
4. Test with standards-compliant DMR radios for verification

### 15.3 Further Research Required

1. Detailed slot timing specifications from Section 7 of TS 102 361-1
2. Propagation delay compensation algorithms (Section 7.2)
3. Embedded signaling requirements (Section 9)
4. Forward Error Correction specifications (Section 8)

---

**Document prepared for**: mmdvm-sdr project
**Next documents**:
- COMMERCIAL_IMPLEMENTATIONS.md
- SFR_TECHNICAL_REQUIREMENTS.md
- SFR_FEASIBILITY_STUDY.md
- SFR_IMPLEMENTATION_SPEC.md
