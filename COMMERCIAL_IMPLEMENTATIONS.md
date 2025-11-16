# Commercial DMR Single Frequency Repeater Implementations

**Document Version:** 1.0
**Date:** 2025-11-16
**Project:** mmdvm-sdr DMR SFR Feasibility Study

---

## Executive Summary

This document analyzes commercial DMR Single Frequency Repeater (SFR) implementations from major manufacturers. Commercial SFR products demonstrate that the technology is mature and field-proven, though implementations vary in approach and capabilities. Key findings:

- **Motorola MOTOTRBO**: Firmware-based SFR (R2.7.0+), no duplexer required
- **Hytera PD98X**: Full-duplex hardware with interference cancellation
- **Kenwood**: No native SFR support; relies on third-party compatibility
- **Tait**: Traditional dual-frequency repeaters only
- **Other vendors**: Multiple Chinese manufacturers offer SFR solutions

---

## 1. Motorola Solutions - MOTOTRBO Platform

### 1.1 Product Overview

Motorola's MOTOTRBO is the most widely deployed DMR system globally. SFR functionality was added through firmware updates.

| Specification | Details |
|--------------|---------|
| **SFR Support** | Firmware-based |
| **Minimum Firmware** | R2.7.0 |
| **Compatible Products** | All current-generation MOTOTRBO radios |
| **Operating Mode** | DMO (Direct Mode Operation) extension |

### 1.2 How MOTOTRBO SFR Works

**Operational Principle**:
1. Repeater input on **Time Slot 1**
2. Repeater output on **Time Slot 2**
3. Single frequency for both RX and TX
4. No traditional duplexer required

**Key Innovation**:
> "This enables the range of a direct link between conventional DMR devices in DMO mode to be extended without using a standard base station or repeater or changing frequency."

### 1.3 MOTOTRBO SFR Products

#### SLR Series Repeaters

**SLR5700** - Compact Repeater:
- Supports DMR Tier II and Tier III
- SFR capability with appropriate firmware
- VHF: 136-174 MHz
- UHF: 403-470 MHz
- Power output: 1-40W (software adjustable)

**SLR1000** - Enhanced SFR Model:
- Internal **high-speed TX/RX antenna switch**
- Feature license required for SFR
- Optimized for single-frequency operation
- Eliminates need for external duplexer

**XPR 8400** - Base Station/Repeater:
- Multi-mode platform
- DMR, analog, and mixed mode
- SFR capable with R2.7.0+ firmware

### 1.4 Technical Specifications

| Parameter | Specification |
|-----------|--------------|
| TX/RX Switching | High-speed antenna switch |
| Duplexer | Not required |
| Frequency Agility | Yes (software programmable) |
| Setup Complexity | Low (no tuned duplexer) |
| Licensing | Feature license required |

### 1.5 Motorola SFR Advantages

**Frequency Resource Conservation**:
- Saves licensed frequency pairs
- Operates on single simplex channel
- Reduces spectrum costs

**Simplified Installation**:
- No duplexer tuning required
- Reduced equipment costs
- Easier site selection

**Compatibility**:
- Works with all ETSI-compliant DMR Tier II radios
- Not proprietary to Motorola radios
- Standard DMO mode operation

### 1.6 Motorola Documentation

**Primary Sources**:
- MOTOTRBO Repeater Specification Sheet
- SLR5700 Product Specifications
- R1 Single Frequency Repeater Specification

**Note**: Detailed technical specifications are proprietary; full implementation details not publicly available.

---

## 2. Hytera Communications

### 2.1 Product Overview

Hytera is a major DMR manufacturer with comprehensive SFR solutions, particularly in their portable radio lineup.

| Specification | Details |
|--------------|---------|
| **Primary SFR Product** | PD98X series portable radios |
| **Technology** | Interference cancellation hardware |
| **Implementation** | Full-duplex capable hardware |
| **Licensing** | Feature license required (~$100) |

### 2.2 PD98X Series - Flagship SFR Portable

#### Key Features

**Single Frequency Repeater Mode**:
- Increases coverage in DMO mode
- Based on interference cancellation technology
- Uses one slot to receive, other to transmit
- Same frequency for RX and TX

**Technical Implementation**:
> "The PD98X uses one slot to receive a signal and the other to transmit it in the same frequency in DMO mode to extend the communication distance."

#### Hardware Requirements

**Critical Distinction**:
- Requires **full-duplex hardware variant**
- Standard PD98X does not include full-duplex capability
- Full-duplex model designation: **PD98X-FD**

**Licensing**:
- SFR feature license: ~$100
- Can be purchased with radio or added later
- License enables SFR firmware functionality

### 2.3 PD78X Series

**Limited Information**:
Research did not reveal specific SFR capabilities for PD78X series. Official Hytera PD78X product pages do not mention SFR functionality, suggesting it may be limited to PD98X series.

### 2.4 Hytera MD-782i Mobile Radio

**MD782i-FD** (Full-Duplex):
- Mobile radio with SFR capability
- Full-duplex hardware variant required
- Feature license activates SFR mode
- Successfully tested with third-party radios (Kenwood, etc.)

**Compatibility Testing**:
- Hytera PD982 SFR mode tested with Kenwood D340U
- Works with Kenwood NX-3320 (reported by users)
- Demonstrates ETSI DMR standard compliance

### 2.5 Interference Cancellation Technology

Hytera's approach to SFR relies on **interference cancellation**:

**Method**:
1. Full-duplex hardware receives and transmits simultaneously
2. Digital signal processing cancels self-interference
3. Adaptive algorithms track amplitude and phase variations
4. Enables operation on single frequency

**Performance**:
- Sufficient cancellation for DMO range extension
- Specific cancellation depth not publicly disclosed
- Field reports indicate reliable operation

### 2.6 Hytera Application Notes

**DMR Radio SFR Application Notes (R2.0)**:
- Technical document available: DMR-Radio_SFR_Application-Notes_R2.0.pdf
- Provides setup and configuration guidance
- Available from Hytera dealers and distributors

### 2.7 Hytera PD985 SFR

**Specialized SFR Handheld**:
- Marketed as "Handheld On-Channel Repeater"
- Includes full-duplex hardware standard
- Optimized for SFR operation
- Professional-grade implementation

---

## 3. Kenwood Communications

### 3.1 Native SFR Support

**Key Finding**: Kenwood does **not manufacture native SFR products**.

From user forums and technical discussions:
> "Kenwood does not have native single frequency repeater capability, which is a shame as it would be a useful feature in the NXR-1700."

### 3.2 Kenwood DMR Repeater Products

#### NXR-1700 Series

**Standard Dual-Frequency Repeater**:
- VHF: 136-174 MHz
- UHF: 400-520 MHz
- DMR Tier II Conventional Operation
- NXDN, DMR digital CAI, and FM analog
- Traditional repeater (requires frequency pair)

#### NXR-1800 Series

**Specifications**:
- Multi-mode: DMR/NXDN/Analog
- VHF/UHF models available
- Conventional repeater architecture
- No SFR capability

### 3.3 Compatibility with Third-Party SFR

**Positive Compatibility**:

Kenwood NX-series radios **work with third-party SFR solutions** because:
- Kenwood is ETSI-compliant for DMR Tier II
- Standard DMO mode supported
- Any basic DMR conventional repeater in DMR mode will work

**Tested Combinations**:
- Kenwood NX-series ↔ Hytera MD-782i-FD SFR: **Works**
- Kenwood NX-series ↔ Hytera PD982 SFR: **Works**
- Kenwood NX-series ↔ Motorola SLR1000 SFR: **Should work** (ETSI compliant)

### 3.4 Programming Considerations

**Important Configuration Notes**:

When using Kenwood radios with third-party SFR:
- **DCDM must be unchecked** (Dual Capacity Direct Mode)
- Use only 1 memory channel
- Configure for standard DMO operation
- Program correct timeslot (TS1 for transmit typically)

**DCDM Issue**:
> "Kenwood DMR talking to Hytera DMR on simplex required DCDM unchecked."

### 3.5 Market Position

Kenwood's strategy appears to be:
1. Focus on traditional dual-frequency repeater products
2. Rely on ETSI standard compliance for interoperability
3. Allow users to deploy third-party SFR solutions
4. Maintain compatibility through standards adherence

---

## 4. Tait Communications

### 4.1 Product Overview

Tait is a professional-grade DMR equipment manufacturer focusing on mission-critical communications.

### 4.2 DMR Repeater Products

#### TB9300 Base Station

**Specifications**:
- Multi-mode platform
- Analog Conventional, MPT, DMR Conventional, DMR Trunking
- DMR Tier 2 & Tier 3 standards compliant
- Ultra-narrowband 6.25kHz equivalent (2×TDMA in 12.5kHz)

**Frequency Bands**:
- VHF: 174-193 MHz (50W)
- UHF: 400-470 MHz (50W)
- 220 MHz: 216-225 MHz (100W)
- 900 MHz: 896-902 MHz, 927-941 MHz (100W)

**Frequency Accuracy**: 0.5 ppm

#### TB7300 Series

**Features**:
- TB7310: DMR Tier 3 & Tier 2, analog repeater support
- Dual mode, simplex, and DFSI capabilities
- Professional-grade reliability
- Mission-critical focus

### 4.3 SFR Support

**Research Finding**: No evidence of Tait single frequency repeater products.

**Tait's Focus**:
- Traditional dual-frequency repeater architecture
- Professional and mission-critical markets
- High reliability and performance
- Standard repeater operation

**Simplex Capability**:
- TB7300 series lists "simplex" as a capability
- This likely refers to base station operation (not repeater)
- Different from SFR functionality

### 4.4 Technical Excellence

**Tait's Strength**:
- Excellent frequency stability (0.5 ppm)
- Robust construction
- Extended temperature ranges
- Long mean time between failures (MTBF)

**Market Position**:
- Public safety
- Utilities
- Transportation
- Mining and resources

---

## 5. Alinco Corporation

### 5.1 Product Overview

Alinco manufactures amateur radio equipment, including DMR-capable transceivers.

### 5.2 DR-Series DMR Products

#### DR-MD500T (Dual Band)

**Specifications**:
- VHF: 136-174 MHz
- UHF: 400-480 MHz
- Two-Slot TDMA (Tier I/Tier II)
- Output power: VHF 55W/25W/10W/1W; UHF 40W/25W/10W/1W
- AMBE+2 (DVSI) digital vocoder
- Frequency stability: ±2.0 ppm

**Channel Capacity**:
- 4,000 channels
- 250 zones (250 channels/zone)
- 10,000 TalkGroups
- 300,000 contact lists

#### DR-MD520T/E (Tri-Band)

**Specifications**:
- VHF: 144 MHz (55W)
- 1.25M: 220 MHz (5W)
- UHF: 440 MHz (40W)
- Digital and analog mixed mode
- 4,000 channels
- 250 zones
- 500,000 digital contact list

### 5.3 Cross-Band Repeat (X-Band)

**Important Distinction**:

Alinco DR-series radios feature **X-band repeat**:
- Analog uplink/digital downlink (or vice versa)
- Cross-band repeater functionality
- **Different frequencies** for RX and TX

**NOT Single Frequency Repeater**:
- X-band repeat uses two different frequencies
- Functions as a traditional cross-band repeater
- Not the same as DMR SFR operation

### 5.4 Amateur Radio Market

Alinco's focus:
- Amateur (ham) radio market
- Cost-effective DMR solutions
- Multi-band operation
- Feature-rich at competitive prices

**SFR Capability**: No evidence of single-frequency repeater functionality in Alinco DR-series products.

---

## 6. Other Manufacturers

### 6.1 Belfone Technology

**BF-SFR600** - Single Frequency Repeater:
- Dedicated DMR single frequency repeater
- Uses both time slots simultaneously
- TX on TS1, RX on TS2 (or vice versa)
- No duplexer required
- Professional DMR solution

**Technical Approach**:
- Purpose-built SFR hardware
- Full digital implementation
- Commercial/professional market

### 6.2 Rexon Technology

**RPT-810** - DMR Digital Direct Mode Repeater:
- Single frequency operation
- TX = RX same frequency
- DMR direct mode compatible
- Designed for range extension

**Features**:
- Compact form factor
- Easy deployment
- No frequency coordination complexity
- Cost-effective solution

### 6.3 FDP (Connect Systems)

**FDP SFR** - Single Frequency Repeater DMR UHF Mobile:

**Key Features**:
- Simultaneously receives and transmits at same time on same frequency
- Utilizes DMR's two time slots concurrently
- Receives on one timeslot while transmitting on the other
- No duplexer required
- Frequency agility benefit

**Marketing Points**:
> "Ease of set-up – no tuned duplexer. A duplexer is not required, so you are not bound by the bandwidth of the duplexer. This gives you frequency agility."

### 6.4 Chinese Manufacturers

Multiple Chinese manufacturers offer DMR SFR solutions:
- Lower cost than tier-1 brands
- Varying quality and support levels
- Often OEM/rebadged products
- Popular in amateur and commercial markets

---

## 7. Comparative Analysis

### 7.1 Technology Approaches

| Manufacturer | Approach | Hardware | Key Technology |
|-------------|----------|----------|----------------|
| **Motorola** | Firmware + Fast Switch | High-speed antenna switch | Software-enabled, SLR1000 optimized |
| **Hytera** | Interference Cancellation | Full-duplex hardware required | Digital signal processing |
| **Kenwood** | Third-party compatibility | Standard DMR Tier II | N/A - relies on others |
| **Tait** | Traditional repeaters only | Dual-frequency | N/A |
| **Belfone/Rexon** | Purpose-built SFR | Dedicated hardware | Full digital implementation |

### 7.2 Market Segments

#### Professional/Commercial
- **Motorola MOTOTRBO**: Market leader, proven reliability
- **Hytera**: Strong alternative, competitive pricing
- **Tait**: Mission-critical focus (no SFR)
- **Belfone**: Cost-effective professional solution

#### Amateur Radio
- **Alinco**: No SFR, but DMR-capable
- **Kenwood**: Compatible via third-party
- **FDP/Connect Systems**: Targeted at amateur market

### 7.3 Implementation Complexity

**Easiest to Deploy**:
1. Purpose-built SFR units (Belfone, Rexon, FDP)
2. Motorola SLR1000 (designed for SFR)
3. Hytera PD98X/MD782i (requires full-duplex variant)

**Most Complex**:
1. Traditional repeaters (require duplexer)
2. Cross-band solutions (frequency coordination)

### 7.4 Cost Considerations

**Feature Licensing**:
- Motorola: Feature license required (price not public)
- Hytera: ~$100 for SFR license
- Belfone/Rexon: Built into product price

**Hardware Requirements**:
- Motorola: May need SLR1000 variant for optimal performance
- Hytera: Must purchase full-duplex model (PD98X-FD, MD782i-FD)
- Purpose-built: All hardware included

---

## 8. Technical Specifications Summary

### 8.1 TX/RX Switching Methods

#### High-Speed Antenna Switch (Motorola)
- **Switching time**: <1 ms (estimated)
- **Method**: Electronic RF switch
- **Isolation**: Temporal (different time slots)
- **Advantage**: Simple, reliable

#### Interference Cancellation (Hytera)
- **Method**: Digital signal processing
- **Hardware**: Full-duplex radio with DSP
- **Cancellation**: Adaptive algorithms
- **Advantage**: Simultaneous TX/RX capability

### 8.2 Frequency Stability

| Manufacturer | Frequency Accuracy | Standard |
|-------------|-------------------|----------|
| Motorola | 0.5 ppm | Industry standard |
| Hytera | 0.5 ppm | ETSI compliant |
| Kenwood | 0.5 ppm | ETSI compliant |
| Tait | 0.5 ppm | High stability |
| Alinco | ±2.0 ppm | Amateur grade |

### 8.3 Power Output

**Typical SFR Products**:
- Handheld SFR (Hytera PD98X): 1-5W
- Mobile SFR (Hytera MD782i): 25-45W
- Base SFR (Motorola SLR1000): 1-40W (adjustable)
- Purpose-built (Belfone BF-SFR600): Varies by model

---

## 9. Field Performance Reports

### 9.1 Reported Range

**Metropolitan Environment**:
- 7-10 km typical
- High-density urban areas
- Multiple obstacles

**Clear Line-of-Sight**:
- 10+ km portable-to-SFR-to-portable
- Hillside to hillside: optimal
- Subject to terrain and antenna height

**Limitations**:
- 30-50 km practical maximum
- Well below 150 km theoretical DMR limit
- TDMA timing constraints apply

### 9.2 Reliability

**Commercial Products**:
- Motorola MOTOTRBO: Proven reliability, large installed base
- Hytera: Good performance reports, growing adoption
- Purpose-built units: Variable quality

**Common Issues Reported**:
- Configuration complexity (user error)
- Compatibility problems with DCDM settings
- Range less than expected (antenna/location)

### 9.3 User Feedback

**Positive Reports**:
- "Extends DMO range effectively"
- "No duplexer tuning needed"
- "Frequency agility is beneficial"
- "Easy to relocate/redeploy"

**Negative Reports**:
- "Range limited compared to traditional repeater"
- "Programming can be tricky"
- "Not all radios compatible without reconfiguration"

---

## 10. Licensing and Regulatory

### 10.1 Feature Licensing

**Manufacturer Licensing**:
- Motorola: Feature license from Motorola required
- Hytera: ~$100 SFR feature license
- Purpose-built: Included in purchase

**Business Model**:
- Unlock SFR functionality in capable hardware
- Generates additional revenue for manufacturers
- Enables product differentiation

### 10.2 Spectrum Licensing

**Regulatory Considerations**:
- SFR operates on single frequency
- May require simplex license (jurisdiction-dependent)
- Some jurisdictions allow on amateur bands
- Commercial use requires appropriate license class

**Frequency Coordination**:
- Simpler than dual-frequency repeaters
- Less spectrum required
- May enable deployment where pairs unavailable

---

## 11. Procurement and Availability

### 11.1 Commercial Products

**Motorola MOTOTRBO**:
- Available through authorized dealers
- Professional installation recommended
- Full support and documentation
- Higher price point

**Hytera Products**:
- Wide dealer network
- Competitive pricing
- Good availability
- Growing market share

**Purpose-Built SFR Units**:
- Available from specialized dealers
- Online availability (Belfone, FDP)
- Variable support levels

### 11.2 Amateur Radio Market

**Availability**:
- Hytera products available to hams (PD98X, MD782i)
- FDP SFR targeted at amateur market
- Alinco through ham radio dealers
- Used commercial equipment market

---

## 12. Reverse Engineering Considerations

### 12.1 Public Information Available

**What's Known**:
1. Basic operating principle (TS1 RX, TS2 TX)
2. DMO mode compatibility required
3. Fast TX/RX switching needed
4. Interference cancellation beneficial

**What's Proprietary**:
1. Specific switching algorithms
2. Interference cancellation implementations
3. Timing compensation methods
4. Firmware implementation details

### 12.2 Patent Landscape

**Potential Patents**:
- Antenna switching methods
- Interference cancellation algorithms
- Timing synchronization techniques
- Digital echo cancellation

**Research Recommended**:
- USPTO and EPO patent searches
- Motorola and Hytera patent portfolios
- Prior art in broadcasting (on-channel repeaters)

---

## 13. Key Takeaways for mmdvm-sdr

### 13.1 Commercial Proof of Concept

**SFR is field-proven technology**:
- Multiple manufacturers
- Various implementation approaches
- Deployed in real-world applications
- Mature product offerings

### 13.2 Technical Feasibility Confirmed

**Critical Technologies Demonstrated**:
1. ✅ Fast TX/RX switching is achievable
2. ✅ Single frequency operation works within DMR standard
3. ✅ Range extension of DMO mode is practical
4. ✅ No duplexer required

### 13.3 Implementation Approaches

**Two Primary Methods**:

1. **High-Speed Switching** (Motorola approach)
   - Temporal isolation
   - Fast antenna switching
   - Simpler implementation
   - Suitable for SDR

2. **Interference Cancellation** (Hytera approach)
   - Digital signal processing
   - Full-duplex operation
   - More complex
   - Requires specialized hardware/algorithms

### 13.4 Range Expectations

**Realistic Performance**:
- 7-10 km metropolitan
- 10-30 km clear line-of-sight
- 30-50 km maximum practical

**Limitation**:
- Well below traditional repeater range
- TDMA timing constraints
- Processing delays reduce effective range

### 13.5 Standards Compliance

**All commercial products comply with ETSI TS 102 361**:
- Standard DMR air interface
- TDMA frame structure
- DMO mode operation
- Interoperability proven

---

## 14. Recommendations for mmdvm-sdr

### 14.1 Approach Selection

**Recommended**: High-speed switching method (Motorola-style)
- Simpler implementation
- Better suited to SDR hardware
- Temporal isolation is cleaner
- Interference cancellation not required initially

### 14.2 Compatibility Target

**Design for compatibility with**:
- Motorola MOTOTRBO radios in DMO mode
- Hytera radios in DMO mode
- Any ETSI DMR Tier II compliant radio
- Standard timeslot assignment (RX TS1, TX TS2)

### 14.3 Performance Goals

**Realistic Targets**:
- 5-10 km range initially
- Extend to 20-30 km with optimization
- Match or exceed handheld SFR performance
- Prioritize reliability over maximum range

### 14.4 Further Research

**Investigate**:
1. Motorola SLR1000 switching timing (if specs available)
2. Hytera interference cancellation algorithms (academic papers)
3. TX/RX switching latency for PlutoSDR
4. Guard time utilization for processing budget

---

## 15. References

### 15.1 Manufacturer Websites

- **Motorola Solutions**: https://www.motorolasolutions.com/
- **Hytera Communications**: https://www.hytera.com/
- **Kenwood Communications**: https://comms.kenwood.com/
- **Tait Communications**: https://www.taitcommunications.com/
- **Alinco Corporation**: https://www.alinco.com/

### 15.2 Product Documentation

- Motorola MOTOTRBO Repeater Specification Sheet
- Hytera DMR Radio SFR Application Notes R2.0
- Kenwood NXR-1700/1800 Specifications
- Tait TB9300/TB7300 Specification Sheets

### 15.3 User Communities

- RadioReference.com DMR Forums
- MMDVM.com Community
- Amateur Radio Stack Exchange
- DMR-MARC Network

---

**Document prepared for**: mmdvm-sdr project
**Related documents**:
- DMR_SFR_STANDARDS.md
- SFR_TECHNICAL_REQUIREMENTS.md (next)
- SFR_FEASIBILITY_STUDY.md
- SFR_IMPLEMENTATION_SPEC.md
