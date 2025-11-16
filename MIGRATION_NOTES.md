# MMDVM-SDR Migration and Integration Notes

**Version:** 1.0
**Date:** 2025-11-16
**Target:** mmdvm-sdr developers and users

---

## Table of Contents

1. [Introduction](#introduction)
2. [Understanding the Patch](#understanding-the-patch)
3. [Applying Changes to Upstream](#applying-changes-to-upstream)
4. [Backporting Upstream Fixes](#backporting-upstream-fixes)
5. [MMDVMHost Integration](#mmdvmhost-integration)
6. [Protocol Version Upgrade Path](#protocol-version-upgrade-path)
7. [Testing Procedures](#testing-procedures)
8. [Troubleshooting](#troubleshooting)

---

## 1. Introduction

This document provides practical guidance for:
- **Users:** Integrating mmdvm-sdr with MMDVMHost
- **Developers:** Maintaining mmdvm-sdr and selectively applying upstream fixes
- **Contributors:** Understanding the architecture for bug fixes and enhancements

### ⚠️ Critical Warning

**DO NOT attempt to apply the full `mmdvm-sdr-to-upstream.patch` to the upstream g4klx/MMDVM repository.**

This patch represents a complete architectural transformation and is provided for:
- Documentation purposes
- Understanding the full scope of changes
- Reference when selectively backporting fixes

Attempting a full patch application will result in build failures and broken functionality.

---

## 2. Understanding the Patch

### 2.1 Patch Contents

The `mmdvm-sdr-to-upstream.patch` file (177 KB) contains:
- 54 modified files
- ~4,000+ lines of changes
- Architectural transformations
- Platform-specific code

### 2.2 Patch Structure

```
mmdvm-sdr-to-upstream.patch
├── Core System Changes (Config.h, Globals.h, IO.h/cpp, MMDVM.cpp)
├── Serial Protocol (SerialPort.h/cpp)
├── Mode Implementations (DMR, D-Star, YSF, P25, NXDN)
└── Calibration Files (Cal*.cpp/h)
```

### 2.3 Change Categories

| Category | Files | Applicability |
|----------|-------|---------------|
| **Critical Changes** | Config.h, Globals.h, IO.h/cpp | mmdvm-sdr only |
| **Protocol Changes** | SerialPort.h/cpp | Needs adaptation |
| **Mode Logic** | DMR*.cpp, DStar*.cpp, etc. | Potentially portable |
| **Minor Changes** | Utils.cpp, Debug.h | Low impact |

---

## 3. Applying Changes to Upstream

### ⛔ IMPORTANT: Full Patch NOT Supported

**This is NOT a valid operation:**
```bash
# ❌ DO NOT DO THIS
cd /path/to/upstream-MMDVM
patch -p1 < mmdvm-sdr-to-upstream.patch  # WILL FAIL
```

**Why it fails:**
1. **Missing files:** Patch references mmdvm-sdr-specific files
   - `PlutoSDR.h/cpp` - Doesn't exist in upstream
   - `arm_math_rpi.h` - Not in upstream
   - `SampleRB.h` - Custom implementation

2. **Removed files:** Patch modifies removed upstream files
   - `FM.h/cpp` - Removed from mmdvm-sdr
   - `POCSAG*.h/cpp` - Not implemented

3. **Platform conflicts:** Linux vs embedded assumptions
   - `pthread` vs hardware interrupts
   - Virtual PTY vs UART
   - ZeroMQ vs hardware I/O

### 3.2 What CAN Be Extracted

**Useful cherry-picks:**
1. **Bug fixes in mode algorithms** (if platform-independent)
2. **Filter coefficient corrections**
3. **Protocol logic improvements**
4. **Debug output enhancements**

**Example: Extracting a DMR fix**
```bash
# View the DMR changes from the patch
grep -A 50 "^--- /tmp/upstream-mmdvm/DMRRX.cpp" mmdvm-sdr-to-upstream.patch

# Extract just the algorithmic changes (skip platform-specific)
# Apply manually after review
```

---

## 4. Backporting Upstream Fixes

This is the **recommended workflow** for keeping mmdvm-sdr updated with upstream improvements.

### 4.1 Setup

```bash
# Add upstream remote (one-time setup)
cd /home/user/mmdvm-sdr
git remote add upstream https://github.com/g4klx/MMDVM.git
git fetch upstream

# Check upstream for new commits
git log upstream/master --since="2024-01-01" --oneline
```

### 4.2 Identifying Backport Candidates

**Good candidates:**
- ✅ Bug fixes in mode RX/TX logic
- ✅ Filter coefficient updates
- ✅ Protocol enhancements (if v1 compatible)
- ✅ Calibration improvements

**Bad candidates:**
- ❌ New hardware platform support
- ❌ FM mode changes
- ❌ POCSAG changes
- ❌ STM32-specific optimizations
- ❌ Arduino library dependencies

### 4.3 Manual Backport Process

**Step-by-step example:**

```bash
# 1. Identify upstream commit
git log upstream/master --grep="DMR" --since="2024-01-01" --oneline
# Example output: abc1234 Fix DMR slot timing issue

# 2. View the commit
git show abc1234

# 3. Extract changed files
git show abc1234 DMRRX.cpp > /tmp/upstream-dmrrx-fix.diff

# 4. Analyze the change
cat /tmp/upstream-dmrrx-fix.diff
# Identify: Is it platform-specific? Does it depend on removed code?

# 5. Apply manually
# Open mmdvm-sdr DMRRX.cpp and apply the logic change
# Adapt to mmdvm-sdr architecture if needed

# 6. Test
cd build
cmake .. && make
./mmdvm  # Run and test DMR mode

# 7. Commit
git add DMRRX.cpp
git commit -m "Backport: Fix DMR slot timing issue from upstream abc1234"
```

### 4.4 Adaptation Checklist

Before applying any upstream change:

- [ ] Does it reference FM/POCSAG code? → Skip or adapt
- [ ] Does it use hardware-specific APIs? → Adapt to Linux/pthread
- [ ] Does it change protocol version? → Review compatibility
- [ ] Does it modify RingBuffer template? → Adapt to SampleRB/RSSIRB
- [ ] Does it add new #ifdef MODE_*? → Remove conditionals
- [ ] Does it touch IO.cpp interrupt code? → Adapt to threaded model

### 4.5 High-Value Backport Targets

**Recommended areas to monitor:**

1. **DMR Slot Synchronization**
   - Files: `DMRSlotRX.cpp`, `DMRRX.cpp`
   - Reason: Timing-critical, affects reliability

2. **D-Star GMSK Demodulation**
   - Files: `DStarRX.cpp`
   - Reason: Signal processing improvements

3. **P25 Phase 1 Decoding**
   - Files: `P25RX.cpp`
   - Reason: Algorithm enhancements

4. **YSF DN Mode**
   - Files: `YSFRX.cpp`
   - Reason: Protocol edge cases

5. **Filter Coefficients**
   - Files: `IO.cpp` (filter arrays)
   - Reason: Direct improvement to signal quality

---

## 5. MMDVMHost Integration

### 5.1 Required MMDVMHost Modification

**File:** `MMDVMHost/Modem.cpp`
**Line:** ~120

**Original:**
```cpp
m_serial = new CSerialController(port, true, 115200);
//                                     ^^^^ RTS check enabled
```

**Modified:**
```cpp
m_serial = new CSerialController(port, false, 115200);
//                                     ^^^^^ RTS check disabled
```

**Creating a patch:**
```bash
cd /path/to/MMDVMHost
git diff Modem.cpp > mmdvmhost-rts-disable.patch
```

**Patch file example:**
```diff
--- a/Modem.cpp
+++ b/Modem.cpp
@@ -117,7 +117,7 @@ bool CModem::open()
     return false;
   }

-  m_serial = new CSerialController(port, true, 115200);
+  m_serial = new CSerialController(port, false, 115200);

   bool ret = m_serial->open();
   if (!ret)
```

### 5.2 MMDVM.ini Configuration

**Example configuration for mmdvm-sdr:**

```ini
[General]
Callsign=MYCALL
Id=1234567
Timeout=180
Duplex=1  # Set to 0 for simplex
Display=Null
Daemon=0

[Modem]
Port=/home/user/mmdvm-sdr/build/ttyMMDVM0  # Path to PTY
TXInvert=1         # Adjust based on SDR requirements
RXInvert=1         # Adjust based on SDR requirements
PTTInvert=0
TXDelay=0
RXOffset=0
TXOffset=0
DMRDelay=0
RXLevel=100
TXLevel=100
RXDCOffset=0
TXDCOffset=0
RFLevel=100
RSSIMappingFile=RSSI.dat
UseCOSAsLockout=0
Trace=0
Debug=1            # Enable for troubleshooting

[D-Star]
Enable=1
Module=C
SelfOnly=0
BlackList=

[DMR]
Enable=1
Beacons=0
BeaconInterval=60
BeaconDuration=3
ColorCode=1
SelfOnly=0
TXHang=4
# ... additional DMR settings

[System Fusion]
Enable=1
SelfOnly=0
# ... additional YSF settings

[P25]
Enable=1
SelfOnly=0
# ... additional P25 settings

[NXDN]
Enable=1
SelfOnly=0
# ... additional NXDN settings

[POCSAG]
Enable=0           # ⚠️ NOT SUPPORTED - Must be disabled

[FM]
Enable=0           # ⚠️ NOT SUPPORTED - Must be disabled
```

### 5.3 Connection Testing

**Step 1: Start mmdvm-sdr**
```bash
cd /home/user/mmdvm-sdr/build
./mmdvm
# Look for: "PTY device created: /home/user/mmdvm-sdr/build/ttyMMDVM0"
```

**Step 2: Verify PTY**
```bash
ls -l /home/user/mmdvm-sdr/build/ttyMMDVM0
# Should show: lrwx------ ... ttyMMDVM0 -> /dev/pts/X
```

**Step 3: Start MMDVMHost**
```bash
cd /path/to/MMDVMHost
./MMDVMHost MMDVM.ini
```

**Expected output:**
```
MMDVM protocol version: 1
Description: ...
```

**Success indicators:**
- No "Cannot open modem" error
- Version exchange completes
- Mode LEDs visible (if configured)
- Network connection succeeds

### 5.4 Troubleshooting MMDVMHost Connection

**Problem:** MMDVMHost says "Cannot open modem"
**Solution:**
1. Check PTY path in MMDVM.ini matches mmdvm-sdr output
2. Verify RTS check is disabled in Modem.cpp
3. Check file permissions on PTY

**Problem:** "Protocol version mismatch" warning
**Solution:**
- Expected behavior (v1 vs v2)
- Does not affect basic operation
- Can be ignored for DMR/D-Star/YSF/P25/NXDN

**Problem:** Mode not activating
**Solution:**
1. Check Enable=1 in MMDVM.ini for that mode
2. Verify mmdvm-sdr compiled with mode support
3. Check Debug=1 output for errors
4. Monitor SDR connection status

---

## 6. Protocol Version Upgrade Path

### 6.1 Current State

- **mmdvm-sdr:** Protocol Version 1
- **Upstream MMDVM:** Protocol Version 2
- **Impact:** Potential compatibility issues with newer MMDVMHost

### 6.2 Upgrading to Protocol v2

**Benefits:**
- Better MMDVMHost compatibility
- Enhanced status reporting
- Future-proofing

**Risks:**
- Breaking changes for older MMDVMHost
- Testing required across all modes

### 6.3 Implementation Steps

**Step 1: Update SerialPort.cpp**

```cpp
// Change protocol version
const uint8_t PROTOCOL_VERSION = 2U;  // Was: 1U
```

**Step 2: Update getVersion() response**

```cpp
void CSerialPort::getVersion()
{
  uint8_t reply[200U];

  reply[0U] = MMDVM_FRAME_START;
  reply[1U] = 0U;
  reply[2U] = MMDVM_GET_VERSION;
  reply[3U] = PROTOCOL_VERSION;  // Returns 2

  // Add capability flags (protocol v2 requirement)
  uint8_t capabilities = 0x00U;
  capabilities |= 0x01U;  // D-Star
  capabilities |= 0x02U;  // DMR
  capabilities |= 0x04U;  // YSF
  capabilities |= 0x08U;  // P25
  capabilities |= 0x10U;  // NXDN
  // NOT: 0x20U (POCSAG)
  // NOT: 0x40U (FM)

  reply[4U] = capabilities;
  reply[5U] = 0x00U;  // Extended capabilities (future use)

  // ... rest of version response
}
```

**Step 3: Update getStatus() format**

```cpp
void CSerialPort::getStatus()
{
  uint8_t reply[20U];

  reply[0U]  = MMDVM_FRAME_START;
  reply[1U]  = 0U;
  reply[2U]  = MMDVM_GET_STATUS;

  // Protocol v2 additions:
  reply[3U]  = m_modemState;      // Current state
  reply[4U]  = m_tx ? 0x01U : 0x00U;   // TX flag

  bool adcOverflow, dacOverflow;
  io.getOverflow(adcOverflow, dacOverflow);
  reply[5U] = adcOverflow ? 0x01U : 0x00U;
  reply[6U] = dacOverflow ? 0x01U : 0x00U;

  // ... additional status fields
}
```

**Step 4: Test compatibility**

```bash
# Test matrix
# 1. Old MMDVMHost (expects v1) with new mmdvm-sdr (v2)
# 2. New MMDVMHost (expects v2) with new mmdvm-sdr (v2)
# 3. All modes: DMR, D-Star, YSF, P25, NXDN
```

### 6.4 Backward Compatibility Strategy

**Option A: Conditional protocol version**
```cpp
#define PROTOCOL_VERSION_V1 1U
#define PROTOCOL_VERSION_V2 2U

// Build flag: -DUSE_PROTOCOL_V2
#ifdef USE_PROTOCOL_V2
const uint8_t PROTOCOL_VERSION = PROTOCOL_VERSION_V2;
#else
const uint8_t PROTOCOL_VERSION = PROTOCOL_VERSION_V1;
#endif
```

**Option B: Auto-negotiation**
- Detect MMDVMHost version from initial handshake
- Respond with appropriate protocol version
- Complex implementation, not recommended initially

**Recommendation:** Start with Option A for testing, then switch to v2 as default.

---

## 7. Testing Procedures

### 7.1 Pre-Integration Testing

**Unit Tests:**
```bash
cd /home/user/mmdvm-sdr/build
./test_resampler  # Sample rate conversion
./test_fmmodem    # FM modulation
./test_neon       # ARM optimizations
```

**Expected output:**
```
[test_resampler] PASS: Upsampling 1:10
[test_resampler] PASS: Downsampling 10:1
[test_fmmodem] PASS: Modulation
[test_fmmodem] PASS: Demodulation
[test_neon] PASS: FIR filter
All tests passed!
```

### 7.2 SDR Hardware Testing

**PlutoSDR Connection:**
```bash
# Test libiio device access
iio_info -n 192.168.2.1
# Should show: AD9361 device

# Test mmdvm-sdr SDR initialization
./mmdvm
# Look for: "PlutoSDR initialized successfully"
```

**Sample Rate Verification:**
```bash
# In mmdvm-sdr output, check:
# "SDR RX sample rate: 1000000 Hz"
# "SDR TX sample rate: 1000000 Hz"
# "Resampler ratio: 41.667"
```

### 7.3 Mode-by-Mode Testing

**D-Star Test:**
1. Configure MMDVMHost with D-Star enabled
2. Transmit from radio or test signal
3. Verify RX decoding in MMDVMHost log
4. Verify network transmission
5. Test TX from network to radio

**DMR Test:**
1. Configure DMR colorcode, timeslots
2. Test Slot 1 RX
3. Test Slot 2 RX
4. Test duplex TX (if enabled)
5. Verify talkgroup routing

**Repeat for:** YSF, P25, NXDN

### 7.4 Integration Test Checklist

- [ ] mmdvm-sdr starts without errors
- [ ] PTY device created successfully
- [ ] MMDVMHost connects to PTY
- [ ] Protocol version exchange succeeds
- [ ] SET_CONFIG command accepted
- [ ] Mode switching works (SET_MODE)
- [ ] D-Star RX functional
- [ ] D-Star TX functional
- [ ] DMR Slot 1 RX functional
- [ ] DMR Slot 2 RX functional
- [ ] DMR TX functional (simplex or duplex)
- [ ] YSF RX functional
- [ ] YSF TX functional
- [ ] P25 RX functional
- [ ] P25 TX functional
- [ ] NXDN RX functional
- [ ] NXDN TX functional
- [ ] Network connectivity stable
- [ ] SDR TX/RX without underruns/overruns
- [ ] No memory leaks (run valgrind)
- [ ] CPU usage acceptable (<50% on target platform)

### 7.5 Performance Testing

**CPU Load Test:**
```bash
# Run mmdvm-sdr and monitor
top -p $(pgrep mmdvm)

# Target: <30% CPU on Raspberry Pi 4
# Target: <10% CPU on x86 i5
```

**Memory Test:**
```bash
# Check for memory leaks
valgrind --leak-check=full ./mmdvm

# Run for 1 hour, should be stable
```

**Latency Test:**
```bash
# Measure TX trigger to RF output
# Measure RX detection to network forwarding
# Target: <100ms end-to-end
```

---

## 8. Troubleshooting

### 8.1 Build Issues

**Problem:** CMake can't find libzmq
```
CMake Error: Could not find ZMQ
```
**Solution:**
```bash
sudo apt-get update
sudo apt-get install libzmq3-dev
```

**Problem:** libiio not found
```
CMake Error: Could not find IIO
```
**Solution:**
```bash
sudo apt-get install libiio-dev libiio0
```

**Problem:** NEON compilation fails
```
error: 'arm_neon.h' file not found
```
**Solution:**
```bash
# Disable NEON if not on ARM
cmake .. -DUSE_NEON=OFF
```

### 8.2 Runtime Issues

**Problem:** Segmentation fault on startup
**Diagnosis:**
```bash
gdb ./mmdvm
(gdb) run
(gdb) bt  # Backtrace when crash occurs
```
**Common causes:**
- NULL pointer in SDR initialization
- Thread synchronization issue
- Buffer overflow

**Problem:** ZeroMQ bind error
```
Error: Address already in use (ipc:///tmp/zmq-mmdvm-tx)
```
**Solution:**
```bash
rm /tmp/zmq-mmdvm-*
./mmdvm
```

**Problem:** PlutoSDR not detected
```
Error: Could not connect to PlutoSDR
```
**Solution:**
```bash
# Check network connection
ping 192.168.2.1

# Check USB connection
iio_info -u usb:

# Try local context
./mmdvm  # Will auto-detect
```

### 8.3 MMDVMHost Issues

**Problem:** Modem keeps disconnecting
**Diagnosis:**
- Check mmdvm-sdr logs for crashes
- Check PTY stability
- Monitor for buffer overruns

**Solution:**
```bash
# Increase buffer sizes in Config.h
#define SDR_RX_BUFFER_SIZE 65536U  // Was 32768U
#define SDR_TX_BUFFER_SIZE 65536U
```

**Problem:** No RX decoding
**Diagnosis:**
- Check SDR frequency configuration
- Verify signal present (spectrum analyzer)
- Check RXInvert setting

**Solution:**
```bash
# Try toggling RXInvert
RXInvert=0  # or 1

# Check deviation
# mmdvm-sdr expects 5 kHz, adjust in Config.h
```

**Problem:** Distorted TX audio
**Diagnosis:**
- Buffer underruns (CPU too slow)
- Incorrect deviation
- Sample rate mismatch

**Solution:**
```bash
# Reduce SDR sample rate
#define SDR_SAMPLE_RATE 500000U  # Was 1000000U

# Enable NEON optimizations
cmake .. -DUSE_NEON=ON

# Check TXLevel
TXLevel=50  # Reduce if distorted
```

### 8.4 Debugging Techniques

**Enable verbose logging:**
```cpp
// In Log.h, set:
#define LOG_LEVEL LOG_DEBUG

// Rebuild and run
```

**Monitor ZeroMQ traffic:**
```bash
# Install zmq tools
pip install zmq

# Monitor TX stream
python3 << EOF
import zmq
context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("ipc:///tmp/zmq-mmdvm-tx")
socket.setsockopt(zmq.SUBSCRIBE, b'')
while True:
    msg = socket.recv()
    print(f"RX: {len(msg)} bytes")
EOF
```

**Capture SDR samples:**
```bash
# Use PlutoSDR directly
iio_readdev -n 192.168.2.1 -b 1024 cf-ad9361-lpc > /tmp/samples.bin

# Analyze in GNU Radio or MATLAB
```

### 8.5 Common Error Messages

| Error | Meaning | Solution |
|-------|---------|----------|
| "PTY creation failed" | Virtual PTY error | Check /dev/pts permissions |
| "SDR initialization failed" | PlutoSDR not accessible | Check network/USB, verify URI |
| "Resampler init failed" | Invalid sample rate | Check SDR_SAMPLE_RATE vs BASEBAND_RATE |
| "ZMQ bind error" | Socket already in use | Remove /tmp/zmq-mmdvm-* files |
| "Buffer overflow" | CPU too slow | Reduce sample rate, enable NEON |
| "Protocol version mismatch" | MMDVMHost expects v2 | Expected, can be ignored |

---

## 9. Development Guidelines

### 9.1 Code Style

**Maintain consistency with upstream:**
- Use existing coding style in each file
- Keep copyright headers updated
- Add comments for mmdvm-sdr-specific changes

**Example:**
```cpp
// Original MMDVM code
void CIO::process()
{
  // ... upstream logic

#if defined(RPI)
  // mmdvm-sdr addition for Linux platform
  pthread_mutex_lock(&m_RXlock);
  // ... SDR-specific code
  pthread_mutex_unlock(&m_RXlock);
#endif
}
```

### 9.2 Branching Strategy

**Recommended branches:**
- `master` - Stable releases
- `develop` - Integration branch
- `feature/xxx` - New features
- `bugfix/xxx` - Bug fixes
- `upstream/xxx` - Backported upstream changes

**Example workflow:**
```bash
# Create backport branch
git checkout -b upstream/dmr-timing-fix

# Apply upstream fix
# ... manual adaptation ...

# Test thoroughly
# ... run tests ...

# Merge to develop
git checkout develop
git merge upstream/dmr-timing-fix

# After testing, merge to master
git checkout master
git merge develop
```

### 9.3 Testing Requirements

**Before committing:**
1. All unit tests pass
2. Integration tests pass
3. No compiler warnings
4. No memory leaks (valgrind)
5. Code reviewed

**Before merging to master:**
1. Full test suite passes
2. Tested on target hardware (PlutoSDR + RaspberryPi)
3. Tested with MMDVMHost
4. Documentation updated

### 9.4 Documentation Updates

**When to update docs:**
- New features added
- API changes
- Configuration changes
- Build process changes
- Troubleshooting tips added

**Files to update:**
- `README.md` - User-facing changes
- `UPSTREAM_COMPARISON.md` - If upstream divergence changes
- `MIGRATION_NOTES.md` - New integration procedures
- `IMPLEMENTATION_SUMMARY.md` - Architecture changes

---

## 10. Quick Reference

### 10.1 Essential Commands

```bash
# Build mmdvm-sdr
cd /home/user/mmdvm-sdr
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Run mmdvm-sdr
./mmdvm

# Run MMDVMHost
cd /path/to/MMDVMHost
./MMDVMHost MMDVM.ini

# Check upstream for updates
git fetch upstream
git log upstream/master --since="1 month ago" --oneline

# Run tests
./test_resampler && ./test_fmmodem && ./test_neon
```

### 10.2 Key Files Reference

| File | Purpose | When to Modify |
|------|---------|----------------|
| `Config.h` | Build configuration | Adding SDR backends, changing defaults |
| `IO.cpp` | Main I/O processing | Signal processing changes |
| `SerialPort.cpp` | Protocol implementation | Protocol updates, command changes |
| `PlutoSDR.cpp` | SDR hardware driver | Device-specific optimizations |
| `Resampler.cpp` | Sample rate conversion | Performance tuning |
| `CMakeLists.txt` | Build system | Dependencies, compile options |

### 10.3 Important Paths

```
/home/user/mmdvm-sdr/
├── build/
│   ├── mmdvm              # Main executable
│   ├── ttyMMDVM0          # PTY symlink (runtime created)
│   ├── test_*             # Unit tests
│   └── CMakeCache.txt     # Build configuration
├── Config.h               # Compile-time configuration
├── MMDVM.ini              # Runtime configuration (MMDVMHost)
├── UPSTREAM_COMPARISON.md # This comparison document
├── MIGRATION_NOTES.md     # This document
└── mmdvm-sdr-to-upstream.patch  # Full diff patch
```

### 10.4 Useful Links

- **Upstream MMDVM:** https://github.com/g4klx/MMDVM
- **MMDVMHost:** https://github.com/g4klx/MMDVMHost
- **mmdvm-sdr:** https://github.com/r4d10n/mmdvm-sdr
- **PlutoSDR:** https://wiki.analog.com/university/tools/pluto
- **libiio:** https://github.com/analogdevicesinc/libiio

---

## 11. Conclusion

This migration guide provides the foundation for:
1. ✅ Understanding mmdvm-sdr vs upstream differences
2. ✅ Integrating with MMDVMHost
3. ✅ Selectively backporting upstream improvements
4. ✅ Maintaining and extending mmdvm-sdr

**Key Takeaways:**
- **Never** apply the full patch to upstream
- **Always** test backported changes thoroughly
- **Maintain** protocol compatibility with MMDVMHost
- **Focus** on SDR-specific enhancements

**Next Steps:**
1. Review UPSTREAM_COMPARISON.md for detailed analysis
2. Test current mmdvm-sdr with your MMDVMHost setup
3. Consider protocol v2 upgrade for future compatibility
4. Monitor upstream for valuable bug fixes

---

**Document Version:** 1.0
**Last Updated:** 2025-11-16
**For Questions:** Refer to project issues on GitHub
**Next Review:** When significant upstream changes occur
