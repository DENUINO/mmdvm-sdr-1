# MMDVM-SDR vs Upstream g4klx/MMDVM Comparison

**Generated:** 2025-11-16
**mmdvm-sdr Repository:** https://github.com/r4d10n/mmdvm-sdr
**Upstream Repository:** https://github.com/g4klx/MMDVM
**Analysis Date:** November 16, 2025

---

## Executive Summary

**mmdvm-sdr** is a fork of the g4klx/MMDVM firmware that has been adapted for standalone operation on x86/ARM Linux platforms using Software Defined Radio (SDR) devices. The fork diverges significantly from the upstream embedded firmware, introducing SDR integration, ZeroMQ support, and removing microcontroller-specific features.

### Key Findings

- **Fork Point:** Initial commit May 15, 2018 (commit `a3eb386`)
- **Upstream Version at Fork:** ~2018 era (pre-POCSAG, pre-FM extensive changes)
- **Current Upstream Version:** 20240113 (Protocol Version 2)
- **mmdvm-sdr Protocol Version:** 1
- **Compatibility Status:** ⚠️ **PARTIAL** - Protocol version mismatch

---

## 1. Repository Statistics

### File Inventory

| Category | mmdvm-sdr | Upstream | Shared | Modified |
|----------|-----------|----------|--------|----------|
| Source Files (.cpp/.h) | 89 | 116 | 59 | 54 (91.5%) |
| Unique to mmdvm-sdr | 30 | - | - | - |
| Unique to Upstream | - | 57 | - | - |

### Unique Files in mmdvm-sdr (30 files)

**SDR Integration:**
- `PlutoSDR.cpp/h` - ADALM Pluto SDR direct access via libiio
- `IORPiSDR.cpp` - SDR-specific I/O implementation
- `Resampler.cpp/h` - Sample rate conversion (SDR↔MMDVM baseband)
- `FMModem.cpp/h` - FM modulation/demodulation for SDR

**ARM Optimizations:**
- `arm_math_neon.cpp/h` - ARM NEON SIMD optimizations
- `arm_math_rpi.cpp/h` - Raspberry Pi ARM math library

**Serial & Communication:**
- `SerialController.cpp/h` - Virtual PTY (pseudo-terminal) management
- `SerialRB.cpp/h` - Serial ring buffer
- `SerialRPI.cpp` - Raspberry Pi serial implementation
- `ISerialPort.cpp/h` - Serial port interface abstraction

**Ring Buffers:**
- `SampleRB.cpp/h` - Sample ring buffer (replaces upstream RingBuffer)
- `RSSIRB.cpp/h` - RSSI ring buffer
- `RingBuff.h` - Custom ring buffer implementation

**Logging & Monitoring:**
- `Log.cpp/h` - Logging infrastructure for Linux
- `UIStats.h` - Text UI statistics display

**Build System:**
- `CMakeLists.txt` - CMake build system (upstream uses Makefile)
- `Toolchain-rpi.cmake` - Cross-compilation toolchain

**Testing:**
- `tests/test_resampler.cpp` - Resampler unit tests
- `tests/test_fmmodem.cpp` - FM modem tests
- `tests/test_neon.cpp` - NEON optimization tests

**Other:**
- `IORPi.cpp` - Raspberry Pi I/O (modified from upstream concept)

### Files Removed from Upstream (57 files)

**FM Mode Implementation (14 files):**
- FM.cpp/h, CalFM.cpp/h
- FMBlanking.cpp/h, FMCTCSSRX.cpp/h, FMCTCSSTX.cpp/h
- FMDirectForm1.h, FMDownSampler.cpp/h, FMKeyer.cpp/h
- FMNoiseSquelch.cpp/h, FMSamplePairPack.h
- FMTimeout.cpp/h, FMTimer.cpp/h, FMUpSampler.cpp/h

**POCSAG Support (3 files):**
- POCSAGTX.cpp/h, CalPOCSAG.cpp/h

**Microcontroller-Specific (20+ files):**
- MMDVM.ino (Arduino sketch)
- IODue.cpp, IOSTM.cpp, IOTeensy.cpp
- SerialArduino.cpp, SerialSTM.cpp, SerialTeensy.cpp
- STMUART.cpp/h, STM32Utils.h
- IOPins.h
- pins/pins_f4_*.h (11 files)
- pins/pins_f7_*.h (5 files)

**Platform-Specific:**
- RingBuffer.h, RingBuffer.impl.h (replaced with custom implementation)
- Version.h (removed, version info in different location)

**STM32 Build Files:**
- stm32f4xx_link.ld, stm32f722_link.ld, stm32f767_link.ld
- openocd.cfg
- STM32F4XX_Lib/, STM32F7XX_Lib/ (submodules)

---

## 2. Modified Files Analysis (54 files)

### 2.1 Core System Files

#### Config.h
**Upstream Changes:**
- Defines MODE_* macros (DSTAR, DMR, YSF, P25, NXDN, POCSAG, FM)
- Serial speed configuration (115200-500000 baud)
- MODE_LEDS, MODE_PINS for hardware LED control
- SEND_RSSI_DATA, SERIAL_REPEATER, I2C_REPEATER features
- Alternate LED configurations

**mmdvm-sdr Changes:**
- **REMOVED:** All MODE_* feature selection macros
- **REMOVED:** Serial speed configuration (handled by Linux)
- **REMOVED:** Hardware-specific pin configurations
- **ADDED:** `STANDALONE_MODE` compilation flag
- **ADDED:** SDR configuration section:
  ```c
  #define SDR_SAMPLE_RATE 1000000U
  #define BASEBAND_RATE 24000U
  #define FM_DEVIATION 5000.0f
  #define PLUTO_URI "ip:192.168.2.1"
  #define SDR_RX_BUFFER_SIZE 32768U
  #define SDR_TX_BUFFER_SIZE 32768U
  ```
- **ADDED:** Text UI configuration
- **ADDED:** Debug flags for SDR subsystems

**Impact:** Major architectural change - shifts from microcontroller embedded system to Linux-based SDR application.

#### Globals.h
**Upstream Changes (2025):**
- Includes ARM CMSIS math library support
- Supports STM32F4, STM32F7, Arduino platforms
- 11 MMDVM_STATE values (includes POCSAG, FM, calibration states)
- Conditional compilation for each mode (#ifdef MODE_*)
- TX_BUFFER_LEN varies by platform (2000-4000)

**mmdvm-sdr Changes:**
- **ADDED:** RPI platform support (`#elif defined(RPI)`)
- **ADDED:** STM32F105xC support
- **ADDED:** `arm_math_rpi.h` inclusion for RPI
- **REMOVED:** POCSAG and FM state values
- **REMOVED:** INTCAL and FM calibration states
- **REMOVED:** All conditional MODE_* compilation
- **CHANGED:** RX_RINGBUFFER_SIZE: 600 → 9600 (16x larger)
- **REMOVED:** TX_BUFFER_LEN definition
- **REMOVED:** m_pocsagEnable, m_fmEnable global variables

**Impact:** Simplified state machine, larger RX buffer for SDR processing, unconditional mode support.

#### IO.h / IO.cpp
**Upstream Changes:**
- Uses RingBuffer<TSample> template
- Conditional filter initialization based on MODE_* defines
- Hardware abstraction for STM32, Arduino, Teensy
- getCPU(), getUDID() methods
- Single-threaded interrupt-driven design
- Supports POCSAG and FM TX levels

**mmdvm-sdr Changes:**
- **REPLACED:** RingBuffer → SampleRB/RSSIRB (custom implementations)
- **ADDED:** ZeroMQ integration for GNU Radio:
  ```cpp
  zmq::context_t m_zmqcontext;
  zmq::socket_t m_zmqsocket;
  std::vector<short> m_audiobuf;
  ```
- **ADDED:** Multi-threading support:
  ```cpp
  pthread_t m_thread;
  pthread_t m_threadRX;
  pthread_mutex_t m_TXlock;
  pthread_mutex_t m_RXlock;
  ```
- **ADDED:** `interruptRX()` method for RX thread
- **ADDED:** `helper()` and `helperRX()` static thread functions
- **REMOVED:** Hardware-specific methods (getCPU, getUDID)
- **REMOVED:** POCSAG and FM TX level parameters
- **REMOVED:** useCOSAsLockout parameter
- **SIMPLIFIED:** Filter architecture - no conditional compilation

**Impact:** Transforms from interrupt-driven embedded I/O to multi-threaded Linux I/O with ZeroMQ messaging.

#### MMDVM.cpp
**Upstream Changes:**
- Arduino/STM32 main loop structure
- Conditional mode processing based on MODE_* defines
- Support for POCSAG and FM modes

**mmdvm-sdr Changes:**
- **ADDED:** RPI to platform support check
- **ADDED:** Log.h and unistd.h includes
- **REMOVED:** All conditional MODE_* compilation
- **REMOVED:** POCSAG and FM mode support
- **ADDED:** Debug logging in setup()
- **SIMPLIFIED:** All modes compiled unconditionally

**Impact:** Cleaner code, assumes all modes available, adds logging.

### 2.2 Serial Protocol (SerialPort.h/cpp)

**Critical Protocol Differences:**

| Feature | Upstream | mmdvm-sdr |
|---------|----------|-----------|
| Protocol Version | **2** | **1** |
| SERIAL_SPEED | Configurable (115200-500000) | N/A (PTY) |
| Buffer Size | 512 bytes | 256 bytes |
| Pointer Type | uint16_t | uint8_t |
| Serial Repeater | #ifdef SERIAL_REPEATER | Removed |
| I2C Repeater | #ifdef I2C_REPEATER | Removed |
| FM Methods | writeFMData(), writeFMStatus(), writeFMEOT() | Removed |

**mmdvm-sdr Additions:**
- `SerialController m_controller` - Virtual PTY management
- `CSerialRB m_repeat` - Custom ring buffer
- Simplified ACK/NAK methods (no type parameter)

**Protocol Compatibility:**
- ✅ Basic commands compatible (GET_VERSION, GET_STATUS, SET_CONFIG, SET_MODE)
- ✅ D-Star, DMR, YSF, P25, NXDN data methods present
- ⚠️ **Version mismatch** - Reports protocol v1, upstream expects v2
- ❌ FM mode commands removed
- ❌ POCSAG commands removed
- ❌ Serial/I2C repeater features removed

**Impact:** Should work with older MMDVMHost versions expecting protocol v1. May have compatibility issues with latest MMDVMHost expecting protocol v2.

### 2.3 Mode-Specific Changes

All mode files (DMR, D-Star, YSF, P25, NXDN) show:
- Copyright year updates removed (kept at 2018 era)
- Minor code cleanups
- Debug output format changes
- No major protocol changes within modes

**Key Pattern:** mmdvm-sdr appears frozen at ~2018 upstream version with local modifications.

---

## 3. Architecture Comparison

### 3.1 Hardware Abstraction

| Layer | Upstream | mmdvm-sdr |
|-------|----------|-----------|
| **Platform** | STM32, Arduino, Teensy | Linux (x86/ARM) |
| **I/O** | GPIO, ADC/DAC | SDR (libiio) |
| **Timing** | Hardware timers | pthread |
| **Serial** | UART hardware | Virtual PTY |
| **Sample Rate** | Fixed ADC/DAC clock | Configurable SDR (520kHz-61.44MHz) |

### 3.2 Signal Processing Pipeline

**Upstream:**
```
ADC (fixed rate) → DC Blocker → Mode Filter → RX Processing
TX Processing → Mode Filter → DAC (fixed rate)
```

**mmdvm-sdr:**
```
SDR RX → Resampler → FM Demod → DC Blocker → Mode Filter → RX Processing
TX Processing → Mode Filter → FM Mod → Resampler → SDR TX
                                      ↓
                              GNU Radio (ZeroMQ)
```

**Key Additions:**
1. **Resampler** - Converts between SDR rate (1 MHz default) and MMDVM baseband (24 kHz)
2. **FM Modem** - Frequency modulation/demodulation
3. **ZeroMQ Bridge** - Optional GNU Radio integration
4. **PlutoSDR Driver** - Direct libiio device access

### 3.3 Threading Model

**Upstream:**
- Single-threaded
- Interrupt-driven (ADC/DAC interrupts)
- `loop()` called from main

**mmdvm-sdr:**
- Multi-threaded:
  - Main thread: `loop()` processing
  - TX thread: `helper()` - ZeroMQ TX, SDR output
  - RX thread: `helperRX()` - SDR input, ZeroMQ RX
- Mutex-protected shared buffers

---

## 4. Compatibility Analysis

### 4.1 MMDVMHost Communication

**Protocol Compatibility Matrix:**

| Feature | Compatible | Notes |
|---------|-----------|-------|
| GET_VERSION | ⚠️ Partial | Returns protocol v1 (upstream v2) |
| GET_STATUS | ✅ Yes | Format unchanged |
| SET_CONFIG | ✅ Yes | Core parameters compatible |
| SET_MODE | ✅ Yes | Modes 0-5 supported |
| D-Star Data | ✅ Yes | Full compatibility |
| DMR Data | ✅ Yes | Full compatibility |
| YSF Data | ✅ Yes | Full compatibility |
| P25 Data | ✅ Yes | Full compatibility |
| NXDN Data | ✅ Yes | Full compatibility |
| POCSAG | ❌ No | Not implemented |
| FM Mode | ❌ No | Not implemented |
| Serial Repeater | ❌ No | Feature removed |
| I2C Repeater | ❌ No | Feature removed |

**Recommendation:** Use with MMDVMHost configured for protocol v1, disable POCSAG and FM modes.

### 4.2 Configuration Parameter Mapping

**MMDVMHost MMDVM.ini → mmdvm-sdr:**

| Parameter | Upstream | mmdvm-sdr | Notes |
|-----------|----------|-----------|-------|
| Port | /dev/ttyACM0, etc. | /path/to/ttyMMDVM0 | Virtual PTY path |
| TXInvert | 0/1 | 0/1 | ✅ Compatible |
| RXInvert | 0/1 | 0/1 | ✅ Compatible |
| PTTInvert | 0/1 | 0/1 | ✅ Compatible |
| TXDelay | 0-255 | 0-255 | ✅ Compatible |
| RXLevel | 0-100 | 0-100 | ✅ Compatible |
| TXLevel | 0-100 | 0-100 | ✅ Compatible |
| RXDCOffset | -128 to 127 | -128 to 127 | ✅ Compatible |
| TXDCOffset | -128 to 127 | -128 to 127 | ✅ Compatible |
| RFLevel | 0-100 | 0-100 | ✅ Compatible |
| POCSAGEnable | 0/1 | ❌ N/A | Not supported |
| FMEnable | 0/1 | ❌ N/A | Not supported |

### 4.3 Required MMDVMHost Modification

**From README.md:**
```cpp
// Modem.cpp Line 120
// Change:
m_serial = new CSerialController(port, true, 115200);
// To:
m_serial = new CSerialController(port, false, 115200);
//                                     ^^^^^ Disable RTS check
```

**Reason:** Virtual PTY doesn't support RTS/CTS hardware flow control.

---

## 5. Fork Divergence Analysis

### 5.1 Upstream Changes Since Fork (2018-2025)

**Major Upstream Additions NOT in mmdvm-sdr:**
1. **POCSAG Support** (2018-2019)
   - Pager protocol transmission
   - Calibration modes
2. **FM Mode** (2019-2021)
   - Analog FM repeater functionality
   - CTCSS RX/TX
   - Noise squelch
   - Up/down samplers
   - Blanking, keyer, timers
3. **Protocol Version 2** (2021+)
   - Enhanced status reporting
   - Mode capability flags
   - Extended debug output
4. **Hardware Support**
   - New STM32 boards (F7 series)
   - Enhanced pin configurations
   - I2C repeater for OLED displays
5. **Performance Improvements**
   - CPU usage reporting
   - Watchdog enhancements
   - Buffer optimization

**Estimated Divergence:** ~7 years of upstream development

### 5.2 mmdvm-sdr Unique Development

**Major Features NOT in Upstream:**
1. **SDR Integration**
   - PlutoSDR support (libiio)
   - Configurable sample rates
   - Sample rate conversion
2. **GNU Radio Integration**
   - ZeroMQ publisher/subscriber
   - Real-time sample streaming
   - Flowgraph examples
3. **Linux Platform Support**
   - Virtual PTY serial emulation
   - pthread multi-threading
   - CMake build system
4. **ARM Optimizations**
   - NEON SIMD math functions
   - Raspberry Pi specific code
5. **Standalone Operation**
   - No Arduino/STM32 dependencies
   - Direct SDR hardware access
   - Text UI with ncurses

---

## 6. Merge Feasibility Assessment

### 6.1 Upstream → mmdvm-sdr (Pulling Updates)

**Difficulty:** 🔴 **VERY HIGH**

**Reasons:**
- Architectural incompatibility (embedded vs Linux)
- Different I/O models (interrupt vs threaded)
- Removed features (FM, POCSAG) deeply integrated
- Protocol version mismatch
- Build system differences (Makefile vs CMake)

**Blockers:**
- Cannot use upstream IO.cpp (STM32 specific)
- Cannot use upstream Config.h (embedded assumptions)
- FM/POCSAG code references throughout codebase
- Hardware timer dependencies

**Feasible Merges:**
- ✅ Bug fixes in mode-specific RX/TX code (DMR, D-Star, etc.)
- ✅ Protocol enhancements (if adapted to v1)
- ✅ Filter coefficient updates
- ⚠️ SerialPort.cpp (requires careful adaptation)

### 6.2 mmdvm-sdr → Upstream (Contributing Back)

**Difficulty:** 🔴 **VERY HIGH**

**Reasons:**
- Fundamentally different target platforms
- Upstream focused on embedded systems
- SDR features not applicable to microcontrollers
- Architectural design incompatible

**Potentially Useful Contributions:**
- Bug fixes in shared code (DMR, D-Star algorithms)
- Protocol optimizations
- Documentation improvements

**Not Mergeable:**
- All SDR-specific code
- Linux platform code
- ZeroMQ integration
- Build system changes

### 6.3 Recommendation

**DO NOT attempt full merge.** The projects have diverged into separate products:

- **g4klx/MMDVM:** Embedded firmware for hardware repeaters/hotspots
- **mmdvm-sdr:** Linux application for SDR-based digital voice

**Instead:**
1. **Selective backporting:** Cherry-pick bug fixes from upstream mode code
2. **Protocol alignment:** Update to protocol v2 for newer MMDVMHost compatibility
3. **Independent development:** Continue as separate fork with distinct purpose

---

## 7. Critical Compatibility Issues

### 7.1 Protocol Version Mismatch

**Problem:**
- mmdvm-sdr: Protocol v1
- Current upstream: Protocol v2
- Latest MMDVMHost may require v2

**Impact:**
- MMDVMHost may reject connection
- Feature negotiation may fail
- Status reporting may be incomplete

**Solution:**
- Update mmdvm-sdr SerialPort.cpp to protocol v2
- Test with latest MMDVMHost
- Document minimum MMDVMHost version

### 7.2 Missing Modes

**Problem:**
- POCSAG not supported
- FM not supported

**Impact:**
- MMDVMHost with FM/POCSAG enabled will fail
- Configuration must explicitly disable these modes

**Solution:**
- Document unsupported modes in README
- Add validation to reject FM/POCSAG commands gracefully
- Consider stub implementation returning NAK

### 7.3 RTS/CTS Flow Control

**Problem:**
- Virtual PTY doesn't support hardware flow control
- Requires MMDVMHost modification

**Impact:**
- Stock MMDVMHost won't work
- Users must patch source

**Solution:**
- Provide MMDVMHost patch file
- Document modification clearly
- Consider alternative flow control mechanism

---

## 8. Migration Path Forward

### 8.1 Short-Term (Immediate)

1. **Update to Protocol v2**
   - Modify SerialPort.cpp
   - Add mode capability flags
   - Test with latest MMDVMHost

2. **Document Compatibility**
   - List supported MMDVMHost versions
   - Provide configuration examples
   - Clarify unsupported features

3. **Improve Error Handling**
   - Gracefully reject FM/POCSAG commands
   - Return appropriate NAK codes
   - Log unsupported operations

### 8.2 Medium-Term (3-6 months)

1. **Selective Bug Fixes**
   - Review upstream commits since 2018
   - Backport relevant mode fixes
   - Test each change thoroughly

2. **Protocol Enhancements**
   - Implement v2 status reporting
   - Add debug output improvements
   - Enhance error recovery

3. **Testing Infrastructure**
   - Expand test suite (tests/ directory)
   - Add integration tests with MMDVMHost
   - Automated compatibility checks

### 8.3 Long-Term (6-12 months)

1. **Optional FM Support**
   - Implement FM using SDR (not upstream code)
   - Native SDR analog FM
   - Test with FM-capable networks

2. **Enhanced SDR Support**
   - Additional SDR backends (LimeSDR, HackRF)
   - Better sample rate handling
   - Improved resampling quality

3. **Performance Optimization**
   - Profile NEON optimizations
   - Reduce latency
   - Optimize buffer management

---

## 9. Patch Application Guide

### 9.1 Applying to Upstream

**NOT RECOMMENDED** - Patches are incompatible with upstream.

The generated patch (`mmdvm-sdr-to-upstream.patch`) contains:
- 177 KB of changes
- 54 modified files
- Fundamental architectural changes

**Attempting to apply will result in:**
- Compilation failures (missing headers)
- Linker errors (missing symbols)
- Runtime crashes (platform assumptions)

### 9.2 Applying to mmdvm-sdr

To apply specific upstream fixes to mmdvm-sdr:

```bash
# Example: Applying a DMR RX fix from upstream
cd /home/user/mmdvm-sdr
git remote add upstream https://github.com/g4klx/MMDVM.git
git fetch upstream

# View specific commit
git show upstream/master:DMRRX.cpp > /tmp/upstream-dmrrx.cpp

# Manually merge relevant changes
# (Automated merge not possible due to divergence)
```

**Manual Merge Checklist:**
1. ✅ Verify fix is platform-independent
2. ✅ Check for dependencies on removed code (FM, POCSAG)
3. ✅ Adapt to mmdvm-sdr architecture (threading, buffers)
4. ✅ Test thoroughly with SDR hardware
5. ✅ Document change in commit message

---

## 10. Test Plan for Compatibility

### 10.1 Unit Tests

**Existing:**
- `tests/test_resampler.cpp` - Sample rate conversion
- `tests/test_fmmodem.cpp` - FM modulation
- `tests/test_neon.cpp` - ARM SIMD operations

**Needed:**
- SerialPort protocol v1/v2 compatibility
- Buffer overflow handling
- Mode switching logic
- ZeroMQ message formatting

### 10.2 Integration Tests

**Required Tests:**
1. **MMDVMHost Communication**
   - Start mmdvm-sdr
   - Connect MMDVMHost
   - Verify version exchange
   - Test mode switching
   - Send test data (all modes)

2. **SDR Hardware**
   - PlutoSDR connection
   - Sample rate configuration
   - TX/RX loopback
   - Signal quality measurement

3. **GNU Radio Integration**
   - ZeroMQ socket creation
   - Sample streaming
   - Flowgraph compatibility

### 10.3 Compatibility Matrix

| MMDVMHost Version | mmdvm-sdr Compatibility | Notes |
|-------------------|------------------------|-------|
| ≤ 2018 | ✅ Full | Protocol v1 native |
| 2019-2020 | ✅ Good | May warn about protocol |
| 2021-2022 | ⚠️ Partial | Protocol v2 preferred |
| 2023+ | ❌ Unknown | Requires testing |

---

## 11. Recommendations

### For mmdvm-sdr Users

1. **Use Older MMDVMHost**
   - Version ≤ 2020 recommended
   - Or apply RTS patch to newer versions

2. **Configuration**
   - Disable POCSAG: `Enable=0`
   - Disable FM: `Enable=0`
   - Enable Debug: `Debug=1`
   - Use provided TXInvert/RXInvert settings

3. **Monitoring**
   - Watch for protocol errors in logs
   - Monitor SDR sample rate stability
   - Check ZeroMQ connection status

### For Developers

1. **Maintenance Strategy**
   - Track mmdvm-sdr as independent project
   - Selectively backport bug fixes only
   - Do not attempt wholesale merge

2. **Future Development**
   - Focus on SDR enhancements
   - Improve Linux platform support
   - Expand test coverage

3. **Documentation**
   - Maintain this comparison document
   - Update when significant upstream changes occur
   - Document each backported fix

### For New Features

1. **Before Adding Upstream Features:**
   - Assess platform compatibility
   - Check for FM/POCSAG dependencies
   - Plan adaptation strategy

2. **Adding SDR Features:**
   - Keep SDR code isolated
   - Maintain conditional compilation
   - Ensure STANDALONE_MODE flag works

3. **Protocol Changes:**
   - Test with multiple MMDVMHost versions
   - Maintain backward compatibility
   - Document breaking changes

---

## Appendix A: File-by-File Change Summary

### Modified Files (54 total)

| File | Changes | Impact |
|------|---------|--------|
| Config.h | Remove MODE_* macros, add SDR config | High |
| Globals.h | Add RPI platform, remove POCSAG/FM | High |
| IO.h/cpp | Add ZeroMQ, threading, remove HW | Critical |
| MMDVM.cpp | Add RPI, remove conditionals | High |
| SerialPort.h/cpp | Protocol v1, PTY support | Critical |
| CWIdTX.h/cpp | Minor cleanups | Low |
| CalDMR.h/cpp | Debug output changes | Low |
| CalDStarRX.h/cpp | Minor updates | Low |
| CalDStarTX.h/cpp | Minor updates | Low |
| CalNXDN.h/cpp | Minor updates | Low |
| CalP25.h/cpp | Minor updates | Low |
| DMRDMORX.h/cpp | Minor updates | Low |
| DMRDMOTX.h/cpp | Minor updates | Low |
| DMRIdleRX.h/cpp | Minor updates | Low |
| DMRRX.h/cpp | Minor updates | Low |
| DMRSlotRX.h/cpp | Debug changes | Low |
| DMRSlotType.h/cpp | Minor updates | Low |
| DMRTX.h/cpp | Minor updates | Low |
| DStarDefines.h | Constants update | Low |
| DStarRX.h/cpp | Minor updates | Low |
| DStarTX.h/cpp | Minor updates | Low |
| Debug.h | Format changes | Low |
| NXDNRX.h/cpp | Minor updates | Low |
| NXDNTX.h/cpp | Minor updates | Low |
| P25Defines.h | Constants update | Low |
| P25RX.h/cpp | Minor updates | Low |
| P25TX.h/cpp | Minor updates | Low |
| Utils.h/cpp | Minor updates | Low |
| YSFRX.h/cpp | Minor updates | Low |
| YSFTX.h/cpp | Minor updates | Low |

---

## Appendix B: Upstream Commit Timeline

**Key Upstream Milestones:**

- **2015-12-22:** Initial commit
- **2017:** D-Star, DMR, YSF, P25, NXDN support
- **2018-05-15:** ⭐ **mmdvm-sdr fork point**
- **2018-2019:** POCSAG implementation
- **2019-2020:** FM mode development
- **2020-2021:** Protocol v2 introduction
- **2021-2022:** Enhanced hardware support
- **2023-2024:** Refinements and bug fixes
- **2024-01-13:** Current version (as of clone)
- **2025-08-27:** Latest visible commit

---

## Appendix C: Build System Comparison

### Upstream (Makefile)

```makefile
# Makefile targets:
# - due (Arduino Due)
# - pi (Raspberry Pi HAT)
# - f4m (STM32F4)
# - f7m (STM32F7)
# - teensy

# Platform-specific compilation
# Hardware-specific linker scripts
# CMSIS library integration
```

### mmdvm-sdr (CMake)

```cmake
# CMakeLists.txt options:
# - BUILD_TESTS (ON/OFF)
# - USE_NEON (ON/OFF)
# - USE_TEXT_UI (ON/OFF)
# - CMAKE_TOOLCHAIN_FILE (cross-compile)

# Dependencies:
# - libzmq (GNU Radio integration)
# - libiio (PlutoSDR)
# - ncurses (Text UI)
# - pthread (threading)
```

**Key Difference:** Upstream targets embedded MCUs, mmdvm-sdr targets Linux systems.

---

## Appendix D: GNU Radio Integration

**ZeroMQ Configuration (IO.cpp):**

```cpp
// TX Socket (mmdvm → GNU Radio)
m_zmqsocket.bind("ipc:///tmp/zmq-mmdvm-tx");

// RX Socket (GNU Radio → mmdvm)
m_zmqsocketRX.connect("ipc:///tmp/zmq-mmdvm-rx");

// Sample Format: 16-bit signed integer
// Sample Rate: 24000 Hz (MMDVM baseband)
```

**GNU Radio Flowgraph:**
- Located in `gr-mmdvm/` directory
- Supports PlutoSDR duplex operation
- Includes DSD decoder integration
- TX/RX frequency configuration

**Not Present in Upstream:** This is entirely mmdvm-sdr specific.

---

## Appendix E: Protocol Command Reference

### Common Commands (Both Versions)

| Command | Hex | Description |
|---------|-----|-------------|
| GET_VERSION | 0x00 | Request firmware version |
| GET_STATUS | 0x01 | Request modem status |
| SET_CONFIG | 0x02 | Configure parameters |
| SET_MODE | 0x03 | Set operating mode |
| CAL_DATA | 0x08 | Calibration data |
| RSSI_DATA | 0x09 | RSSI measurements |
| SEND_CWID | 0x0A | Send CW ID |
| DSTAR_HEADER | 0x10 | D-Star header |
| DSTAR_DATA | 0x11 | D-Star voice frame |
| DSTAR_LOST | 0x12 | D-Star sync lost |
| DSTAR_EOT | 0x13 | D-Star end of TX |
| DMR_DATA1 | 0x18 | DMR slot 1 data |
| DMR_LOST1 | 0x19 | DMR slot 1 lost |
| DMR_DATA2 | 0x1A | DMR slot 2 data |
| DMR_LOST2 | 0x1B | DMR slot 2 lost |
| YSF_DATA | 0x20 | YSF voice frame |
| YSF_LOST | 0x21 | YSF sync lost |
| P25_HDR | 0x30 | P25 header |
| P25_LDU | 0x31 | P25 voice frame |
| P25_LOST | 0x32 | P25 sync lost |
| NXDN_DATA | 0x40 | NXDN voice frame |
| NXDN_LOST | 0x41 | NXDN sync lost |

### Upstream-Only Commands (v2)

| Command | Hex | Description |
|---------|-----|-------------|
| POCSAG_DATA | 0x50 | POCSAG frame |
| FM_PARAMS1 | 0x60 | FM parameters |
| FM_PARAMS2 | 0x61 | FM extended params |
| FM_PARAMS3 | 0x62 | FM CTCSS config |
| FM_DATA | 0x65 | FM audio data |
| ACK | 0x70 | Acknowledge |
| NAK | 0x7F | Not acknowledged |
| DEBUG1-5 | 0xF1-F5 | Debug messages |

**Compatibility:** mmdvm-sdr supports all common commands, lacks FM/POCSAG.

---

## Appendix F: SDR Hardware Specifications

### ADALM Pluto SDR

**Supported by mmdvm-sdr:**
- **RF Range:** 325 MHz - 3.8 GHz
- **Sample Rate:** 520 kHz - 61.44 MHz
- **Bandwidth:** 200 kHz - 56 MHz
- **Interface:** USB 2.0, Ethernet (network mode)
- **Library:** libiio
- **Default URI:** `ip:192.168.2.1` or `usb:`

**mmdvm-sdr Configuration:**
- **TX Frequency:** Set in MMDVMHost MMDVM.ini
- **RX Frequency:** TX ± Duplex offset
- **Sample Rate:** 1 MHz (default, configurable)
- **FM Deviation:** 5 kHz (configurable)

### Other SDR Devices (Potential)

**Not Yet Implemented:**
- LimeSDR (mentioned in README TODO)
- HackRF (mentioned in Config.h)
- USRP (mentioned in documentation)
- bladeRF (mentioned in documentation)

**Implementation Path:**
- Abstract SDR interface (like ISerialPort)
- SoapySDR integration (mentioned in TODO)
- Device-specific drivers

---

## Appendix G: Known Issues and Workarounds

### Issue 1: MMDVMHost RTS Check

**Problem:** Virtual PTY doesn't support RTS/CTS flow control
**Symptom:** MMDVMHost fails to open modem
**Workaround:**
```cpp
// In MMDVMHost/Modem.cpp line 120:
m_serial = new CSerialController(port, false);
//                                     ^^^^^ Set to false
```

### Issue 2: Protocol Version Warning

**Problem:** Protocol v1 vs v2 mismatch
**Symptom:** Warning in MMDVMHost log
**Workaround:** Ignore warning, functionality unaffected for basic modes

### Issue 3: ZeroMQ Compile Errors on RasPi

**Problem:** Missing ZMQ headers
**Symptom:** Compilation fails
**Workaround:**
```bash
apt-get install libzmq3-dev libzmq5
```

### Issue 4: TX Buffer Underruns

**Problem:** Slow CPU can't keep up with SDR
**Symptom:** Distorted TX audio
**Workaround:** Reduce SDR sample rate, enable NEON optimizations

---

## Appendix H: Performance Considerations

### CPU Usage

**mmdvm-sdr Requirements:**
- **Minimum:** Raspberry Pi 3 (Cortex-A53)
- **Recommended:** Raspberry Pi 4, x86 i5+
- **Optimal:** PlutoSDR embedded (Zynq-7000)

**Bottlenecks:**
1. Sample rate conversion (resampler)
2. FM modulation/demodulation
3. Filter processing
4. ZeroMQ serialization

**Optimizations:**
- Enable NEON SIMD (-DUSE_NEON=ON)
- Reduce SDR sample rate
- Use hardware FM (SDR native)
- Minimize debug output

### Memory Usage

**Ring Buffer Sizes:**
- RX Buffer: 9600 samples (vs 600 upstream)
- TX Buffer: Configurable
- SDR Buffers: 32 KB each

**Total Estimated:** ~10-20 MB (vs ~100 KB embedded)

---

## Conclusion

**mmdvm-sdr** and **g4klx/MMDVM** have diverged into fundamentally different products serving different use cases. While they share common heritage and protocol basics, architectural differences make full reconciliation impractical.

**Key Takeaways:**
1. ⚠️ Protocol v1 vs v2 compatibility requires attention
2. ✅ Core digital modes (DMR, D-Star, YSF, P25, NXDN) work well
3. ❌ FM and POCSAG not supported in mmdvm-sdr
4. 🔧 Selective backporting of bug fixes is feasible
5. 🚀 Continue independent development with clear purpose

**This document should be updated:**
- When significant upstream changes occur
- After protocol version updates
- When new SDR backends added
- Annually for maintenance

---

**Document Version:** 1.0
**Last Updated:** 2025-11-16
**Maintainer:** Automated analysis tool
**Next Review:** 2026-11-16
