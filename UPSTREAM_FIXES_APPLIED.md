# MMDVM-SDR Upstream Bug Fixes Applied

**Document Version:** 1.0
**Date:** 2025-11-17
**Purpose:** Track bug fixes cherry-picked from g4klx/MMDVM upstream repository

---

## Overview

This document tracks critical bug fixes cherry-picked from the upstream g4klx/MMDVM repository and adapted for mmdvm-sdr. All fixes listed here have been applied, tested for compilation, and verified as non-hardware-specific improvements that benefit SDR operation.

---

## Applied Fixes

### 1. D-Star Fast Data Support and 64-bit Pattern Matching

**Upstream Commits:**
- **9440b43** - "Simplify the D-Star fast data support" (2020-12-14)
- **657e712** - "Detect full 48 bits of last data frame in a transmission" (2020-11-28)

**Author:** Jonathan Naylor G4KLX, Tim Stewart

**Issue Fixed:**
D-Star receiver was using 32-bit pattern buffers which:
1. Required maintaining separate buffers for different sync patterns
2. Could only match 32 bits of the end-of-transmission sync, causing false detections when certain bit patterns appeared in DV Fast Data (especially images)

**Root Cause:**
The D-Star specification requires matching the full 48-bit end-of-transmission pattern (see ARRL D-STAR.pdf section 2.1.2, item 6). Using only 32 bits led to premature end-of-transmission detection.

**Changes Made:**

#### File: `DStarRX.h`
- Changed `uint32_t m_patternBuffer` to `uint64_t m_patternBuffer`
- Updated copyright to include 2020

#### File: `DStarRX.cpp`
- Updated copyright to include 2020
- Changed all sync pattern constants from `uint32_t` to `uint64_t`:
  - `FRAME_SYNC_DATA`: `0x00557650U` → `0x0000000000557650U`
  - `FRAME_SYNC_MASK`: `0x00FFFFFFU` → `0x0000000000FFFFFFU`
  - `DATA_SYNC_DATA`: `0x00AAB468U` → `0x0000000000AAB468U`
  - `DATA_SYNC_MASK`: `0x00FFFFFFU` → `0x0000000000FFFFFFU`
  - `END_SYNC_DATA`: `0xAAAA135EU` → `0x0000AAAAAAAA135EU` (now 48 bits)
  - `END_SYNC_MASK`: `0xFFFFFFFFU` → `0x0000FFFFFFFFFFFFU` (now 48 bits)
- Replaced all 5 instances of `countBits32()` with `countBits64()`
- Updated variable types in late sync detection loop from `uint32_t` to `uint64_t`

**Impact:**
- ✅ Prevents false end-of-transmission detections during DV Fast Data
- ✅ Simplifies code by removing need for separate 32-bit and 64-bit buffers
- ✅ Matches full 48-bit D-Star end sync as per specification
- ✅ Improves reliability when transmitting images and other data via D-Star

**Testing Status:**
- ✅ Code compiles successfully
- ✅ All countBits32 calls replaced (5 total)
- ⚠️ Runtime testing required with actual D-Star signals

---

## Rejected/Deferred Fixes

### P25 PDU Header Support (Commit 3bae422)

**Status:** Deferred

**Reason:**
This fix adds support for P25 PDU (Packet Data Unit) headers, requiring:
1. Addition of P25_DUID constants (not present in mmdvm-sdr)
2. NID (Network Identifier) decoding to extract DUID
3. Major restructuring of P25RX::processHdr() with switch/case statements

The current mmdvm-sdr P25 implementation is simpler and doesn't do DUID-based frame type detection. Implementing this would require:
- Significant architectural changes
- Extensive testing across all P25 frame types
- Risk of breaking existing LDU1/LDU2 voice frame handling

**Assessment:** Complex refactoring not suitable for simple cherry-pick. PDU support is primarily for P25 trunking data which may not be commonly used in SDR hotspot applications.

### P25 DUID Processing Enhancement (Commit 260dca9)

**Status:** Rejected

**Reason:**
Major refactoring (683 additions, 607 deletions across 3 files) that completely restructures P25 receiver architecture. Requires the PDU support mentioned above plus extensive additional changes.

### DMR/YSF Filter Separation (Commit 5800b33)

**Status:** Not Applicable

**Reason:**
Upstream change splits DMR and YSF RRC filters to prevent state interference. However, mmdvm-sdr uses a completely different IO architecture (SDR-based processing vs. hardware interrupts). The relevant filter structures (`m_rrc02Filter`, `m_rrc02State`) do not exist in mmdvm-sdr's IO.cpp/IO.h.

---

## Verification Checklist

- [x] D-Star sync patterns changed to uint64_t
- [x] D-Star countBits32 calls changed to countBits64 (5 total)
- [x] END_SYNC uses full 48-bit pattern
- [x] Code compiles without D-Star-related errors
- [x] Documentation updated
- [ ] D-Star voice transmission tested (runtime)
- [ ] D-Star DV Fast Data tested (runtime)
- [ ] D-Star end-of-transmission detection verified (runtime)

---

## Notes on Already-Applied Fixes

During the analysis, several fixes were found to already be present in mmdvm-sdr:

### DMR DMO RX Sync Tolerance (Commit 2e9155e - 2018-06-03)
**Status:** ✅ Already Applied

Found in `DMRDMORX.cpp`:
```cpp
const uint8_t MAX_SYNC_SYMBOLS_ERRS = 2U;  // Was 4U upstream
const uint8_t MAX_SYNC_BYTES_ERRS   = 3U;  // Was 6U upstream
const uint8_t MAX_SYNC_LOST_FRAMES  = 13U; // Was 26U upstream
```

This fix tightens DMR DMO (Direct Mode Operation) sync detection tolerances, reducing false synchronization on noise.

### YSF Signal Loss Detection (Commit f324f96 - 2024-11-21)
**Status:** ✅ Already Applied

Found in `YSFRX.cpp`:
```cpp
const unsigned int MAX_SYNC_FRAMES = 4U + 1U;
```

This sets the number of frames before YSF declares signal lost. Note: upstream value changed from `1U + 1U` (commit 823b91c, 2020-08-03) to `4U + 1U` (commit f324f96, 2024-11-21). The current value allows more tolerance before declaring signal lost, preventing premature disconnections.

---

## Upstream Monitoring

The following upstream repository areas should be monitored for future bug fixes:

### High-Priority Areas

1. **DMR Slot Synchronization**
   - Files: `DMRSlotRX.cpp`, `DMRRX.cpp`, `DMRDMORX.cpp`
   - Focus: Timing-critical code, affects reliability
   - Last checked: 2025-11-17

2. **D-Star GMSK Demodulation**
   - Files: `DStarRX.cpp`, `DStarTX.cpp`
   - Focus: Signal processing improvements
   - Last checked: 2025-11-17

3. **P25 Phase 1 Decoding**
   - Files: `P25RX.cpp`, `P25TX.cpp`
   - Focus: Algorithm enhancements
   - Last checked: 2025-11-17

4. **YSF DN Mode**
   - Files: `YSFRX.cpp`, `YSFTX.cpp`
   - Focus: Protocol edge cases
   - Last checked: 2025-11-17

5. **NXDN**
   - Files: `NXDNRX.cpp`, `NXDNTX.cpp`
   - Focus: Scrambler and protocol handling
   - Last checked: 2025-11-17

### Upstream Repository

- **URL:** https://github.com/g4klx/MMDVM
- **Branch:** master
- **Remote name:** upstream

### Checking for Updates

```bash
cd /home/user/mmdvm-sdr
git fetch upstream
git log upstream/master --since="1 month ago" --oneline -- DMRRX.cpp DStarRX.cpp YSFRX.cpp P25RX.cpp NXDNRX.cpp
```

---

## Backporting Guidelines

When considering upstream fixes for backporting:

### ✅ Good Candidates
- Bug fixes in mode RX/TX logic (platform-independent)
- Filter coefficient corrections
- Protocol enhancements (if compatible with protocol v1)
- Sync pattern improvements
- Timing constant adjustments
- Error correction improvements

### ❌ Bad Candidates
- New hardware platform support (STM32-specific, Arduino)
- FM mode changes (not implemented in mmdvm-sdr)
- POCSAG changes (not implemented in mmdvm-sdr)
- STM32-specific optimizations
- Arduino library dependencies
- IO.cpp hardware interrupt changes (mmdvm-sdr uses SDR/threading model)
- Changes requiring protocol v2 (mmdvm-sdr uses v1)

### Adaptation Requirements

Before applying any upstream change:
- [ ] Does it reference FM/POCSAG code? → Skip or adapt
- [ ] Does it use hardware-specific APIs? → Adapt to Linux/pthread model
- [ ] Does it change protocol version? → Review compatibility
- [ ] Does it modify IO.cpp interrupt code? → Adapt to threaded SDR model
- [ ] Does it add new `#ifdef MODE_*`? → Evaluate necessity
- [ ] Is it purely algorithmic? → Usually safe to apply

---

## Build Status

**Last Build Test:** 2025-11-17
**Status:** ✅ D-Star changes compile successfully

**Note:** Pre-existing build issues in the repository (NEON library linking) are unrelated to the applied D-Star fixes. The D-Star RX/TX modules compile without errors.

---

## References

1. **Upstream Repository:** https://github.com/g4klx/MMDVM
2. **D-Star Specification:** http://www.arrl.org/files/file/D-STAR.pdf
3. **MMDVM-SDR Migration Notes:** See `docs/upstream/MIGRATION_NOTES.md`
4. **MMDVM-SDR Comparison:** See `docs/upstream/UPSTREAM_COMPARISON.md`

---

## Contributing

When applying new upstream fixes:

1. Document the fix in this file
2. List upstream commit hash and author
3. Describe the issue being fixed
4. Document all changes made
5. Note any adaptations required for mmdvm-sdr
6. Update verification checklist
7. Test compilation
8. Commit with message: `Backport: <description> from upstream <commit>`

---

**Document Status:** Active
**Next Review:** When significant upstream changes occur or quarterly (whichever is sooner)
**Maintained By:** mmdvm-sdr development team
