# MMDVM-SDR Build Options

**Version:** 1.0
**Last Updated:** 2025-11-17
**CMake Minimum Version:** 3.10

This document describes all available build options for MMDVM-SDR. These options allow you to customize the build for different hardware platforms, enable/disable features, and configure the modem for specific use cases.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Core Build Options](#core-build-options)
3. [Protocol and Transport Options](#protocol-and-transport-options)
4. [Mode Support Options](#mode-support-options)
5. [Build Examples](#build-examples)
6. [Compiler Options](#compiler-options)
7. [Dependencies](#dependencies)
8. [Troubleshooting](#troubleshooting)

---

## Quick Start

### Default Build (Recommended)
```bash
mkdir build && cd build
cmake ..
make -j4
```

**Default Configuration:**
- Standalone SDR mode (no GNU Radio)
- NEON optimizations enabled
- Text UI enabled
- PlutoSDR target enabled
- Unit tests enabled
- PTY transport (not UDP)
- Protocol version 2
- FM mode enabled
- POCSAG mode disabled

### Custom Build Example
```bash
mkdir build && cd build
cmake .. \
  -DSTANDALONE_MODE=ON \
  -DUSE_NEON=ON \
  -DUSE_UDP_MODEM=ON \
  -DPROTOCOL_VERSION=2 \
  -DENABLE_POCSAG=ON \
  -DBUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release
make -j4
```

---

## Core Build Options

### STANDALONE_MODE
**Type:** `BOOL`
**Default:** `ON`
**CMake Flag:** `-DSTANDALONE_MODE=ON|OFF`

Enables standalone SDR mode with direct hardware control (no GNU Radio required).

**When ON:**
- Uses `IORPiSDR.cpp` for I/O layer
- Enables FM modem and resampler
- Requires PlutoSDR hardware (or compatible SDR)
- Includes `FMModem.cpp`, `Resampler.cpp`, `PlutoSDR.cpp`

**When OFF:**
- Uses `IORPi.cpp` for GNU Radio ZMQ interface
- Requires GNU Radio flowgraph running separately
- Uses ZeroMQ for IPC
- Requires `libzmq3-dev`

**Example:**
```bash
# Standalone mode (direct SDR control)
cmake .. -DSTANDALONE_MODE=ON

# ZMQ mode (GNU Radio integration)
cmake .. -DSTANDALONE_MODE=OFF
```

**Dependencies:**
- ON: Requires `libiio-dev` (for PlutoSDR)
- OFF: Requires `libzmq3-dev`

---

### USE_NEON
**Type:** `BOOL`
**Default:** `ON`
**CMake Flag:** `-DUSE_NEON=ON|OFF`

Enables ARM NEON SIMD optimizations for DSP functions.

**Optimized Functions:**
- FIR filtering (8-way SIMD)
- IIR biquad cascades
- Correlation (sync detection)
- Vector operations (add, subtract, scale)

**Performance Impact:**
- FIR filters: 2.5-3x speedup
- Correlation: 4-6x speedup
- Overall CPU reduction: ~25%

**Platform Support:**
- ARM Cortex-A (ARMv7-A and later)
- Raspberry Pi 2/3/4
- ADALM Pluto embedded CPU
- Zynq-7000 ARM cores

**When ON:**
- Uses `arm_math_neon.cpp` with SIMD intrinsics
- Adds compiler flags: `-mfpu=neon -mfloat-abi=hard`
- Defines: `USE_NEON`

**When OFF:**
- Uses `arm_math_rpi.cpp` with scalar operations
- No SIMD optimizations
- Slower but works on all platforms

**Example:**
```bash
# Enable NEON (ARM platforms)
cmake .. -DUSE_NEON=ON

# Disable NEON (x86, older ARM)
cmake .. -DUSE_NEON=OFF
```

**Auto-Detection:**
The build system checks `CMAKE_SYSTEM_PROCESSOR` to ensure NEON is only enabled on compatible ARM platforms.

---

### USE_TEXT_UI
**Type:** `BOOL`
**Default:** `ON`
**CMake Flag:** `-DUSE_TEXT_UI=ON|OFF`

Enables ncurses-based text UI for real-time monitoring.

**Features:**
- Statistics display (TX/RX counters, modes)
- Real-time status updates
- Performance metrics

**When ON:**
- Includes `UIStats.h` (statistics collection)
- Links against `libncurses`
- Defines: `TEXT_UI`

**When OFF:**
- No UI support
- Minimal console output
- Lower memory footprint

**Example:**
```bash
# With text UI
cmake .. -DUSE_TEXT_UI=ON

# Headless mode (no UI)
cmake .. -DUSE_TEXT_UI=OFF
```

**Dependencies:**
- Requires: `libncurses5-dev` or `libncurses-dev`
- Install: `sudo apt-get install libncurses5-dev`

---

### PLUTO_SDR
**Type:** `BOOL`
**Default:** `ON`
**CMake Flag:** `-DPLUTO_SDR=ON|OFF`

Enables ADALM PlutoSDR hardware support.

**When ON:**
- Includes `PlutoSDR.cpp/h` driver
- Links against `libiio`
- Defines: `PLUTO_SDR`, `HAVE_LIBIIO`

**When OFF:**
- PlutoSDR support disabled
- Can still use other SDR hardware (if drivers added)

**Hardware Requirements:**
- ADALM PlutoSDR (USB or network-attached)
- Firmware rev 0.31 or later
- libiio 0.21 or later

**Example:**
```bash
# PlutoSDR target
cmake .. -DPLUTO_SDR=ON -DSTANDALONE_MODE=ON

# Other SDR hardware
cmake .. -DPLUTO_SDR=OFF
```

**Dependencies:**
- Requires: `libiio-dev`, `libiio-utils`
- Install: `sudo apt-get install libiio-dev`

**Auto-Detection:**
If `libiio` is not found, PLUTO_SDR is automatically disabled with a warning.

---

### BUILD_TESTS
**Type:** `BOOL`
**Default:** `ON`
**CMake Flag:** `-DBUILD_TESTS=ON|OFF`

Builds unit tests for core components.

**Test Coverage:**
- Resampler (decimation, interpolation, rational resampling)
- FM Modem (modulation, demodulation)
- NEON math functions (if enabled)
- UDP sockets (if enabled)
- Shared memory IPC (if enabled)
- POCSAG mode (if enabled)

**Test Executables:**
- `test_resampler`
- `test_fmmodem`
- `test_neon` (if USE_NEON=ON)
- `test_udpsocket` (if USE_UDP_MODEM=ON)
- `test_udpmodemport` (if USE_UDP_MODEM=ON)
- `test_shmem` (if USE_SHARED_MEMORY_IPC=ON)
- `test_pocsag` (if ENABLE_POCSAG=ON)

**Running Tests:**
```bash
# Build with tests
cmake .. -DBUILD_TESTS=ON
make

# Run all tests
ctest --verbose

# Run specific test
./test_resampler
```

**Example:**
```bash
# Development build with tests
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug

# Production build without tests
cmake .. -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
```

---

## Protocol and Transport Options

### PROTOCOL_VERSION
**Type:** `STRING`
**Default:** `"2"`
**Valid Values:** `"1"`, `"2"`
**CMake Flag:** `-DPROTOCOL_VERSION=1|2`

Sets the MMDVM protocol version.

**Version 1:**
- Original MMDVM protocol
- Compatible with older MMDVMHost versions
- Limited feature set

**Version 2:**
- Extended protocol with additional features
- Better error reporting
- Recommended for new deployments

**Example:**
```bash
# Protocol version 2 (recommended)
cmake .. -DPROTOCOL_VERSION=2

# Protocol version 1 (legacy compatibility)
cmake .. -DPROTOCOL_VERSION=1
```

**Defines:** `PROTOCOL_VERSION=1` or `PROTOCOL_VERSION=2`

---

### USE_UDP_MODEM
**Type:** `BOOL`
**Default:** `OFF`
**CMake Flag:** `-DUSE_UDP_MODEM=ON|OFF`

Uses UDP network socket instead of virtual PTY for modem communication.

**Benefits:**
- No MMDVMHost code modification required
- Network-transparent (modem can be on different machine)
- Easy debugging with Wireshark/tcpdump
- No PTY symlink management
- Better error handling

**When ON:**
- Includes `UDPSocket.cpp`, `UDPModemPort.cpp`
- Uses UDP sockets for MMDVM protocol transport
- Defines: `USE_UDP_MODEM`
- Compatible with MMDVMHost native UDP mode

**When OFF:**
- Uses virtual PTY (pseudo-terminal)
- Requires `SerialController.cpp`
- Requires MMDVMHost RTS patch

**Configuration:**
Edit `Config.h` to set UDP addresses and ports:
```cpp
#define UDP_MODEM_ADDRESS  "127.0.0.1"  // MMDVMHost IP
#define UDP_MODEM_PORT     3335          // MMDVMHost port
#define UDP_LOCAL_ADDRESS  "127.0.0.1"  // Modem bind IP
#define UDP_LOCAL_PORT     3334          // Modem bind port
```

**Example:**
```bash
# UDP modem mode
cmake .. -DUSE_UDP_MODEM=ON

# PTY mode (default)
cmake .. -DUSE_UDP_MODEM=OFF
```

**Dependencies:**
- Requires: POSIX sockets API (`sys/socket.h`)
- Automatically checked by CMake

**Status:** Implementation in progress
**Documentation:** See `docs/mmdvmhost/UDP_IMPLEMENTATION_PLAN.md`

---

### USE_SHARED_MEMORY_IPC
**Type:** `BOOL`
**Default:** `OFF`
**CMake Flag:** `-DUSE_SHARED_MEMORY_IPC=ON|OFF`

Enables shared memory IPC for inter-process communication (experimental).

**Use Cases:**
- High-performance IPC with minimal overhead
- Zero-copy data transfer
- Lower latency than UDP/sockets

**When ON:**
- Includes `SharedMemoryIPC.cpp/h`
- Links against `librt` (already linked)
- Defines: `USE_SHARED_MEMORY_IPC`
- Uses POSIX shared memory (`shm_open`, `mmap`)

**When OFF:**
- No shared memory support
- Uses standard IPC methods

**Example:**
```bash
# Enable shared memory IPC
cmake .. -DUSE_SHARED_MEMORY_IPC=ON
```

**Dependencies:**
- Requires: POSIX shared memory API (`sys/mman.h`)
- Automatically checked by CMake
- Already linked: `librt`

**Status:** Experimental - Not yet implemented
**Warning:** This feature is experimental and may not be stable.

---

## Mode Support Options

### ENABLE_FM_MODE
**Type:** `BOOL`
**Default:** `ON`
**CMake Flag:** `-DENABLE_FM_MODE=ON|OFF`

Enables FM analog mode support.

**Features:**
- FM voice modulation/demodulation
- Analog audio pass-through
- Compatible with commercial FM radios

**When ON:**
- Uses existing `FMModem.cpp` component
- Defines: `ENABLE_FM_MODE`

**When OFF:**
- FM mode disabled
- Digital modes only

**Note:** Dedicated FMRX/FMTX components (similar to other modes) are planned but not yet implemented. Currently uses the existing FMModem infrastructure.

**Example:**
```bash
# Enable FM mode
cmake .. -DENABLE_FM_MODE=ON -DSTANDALONE_MODE=ON

# Digital modes only
cmake .. -DENABLE_FM_MODE=OFF
```

**Dependencies:**
- Works with standalone mode
- Requires SDR hardware capable of FM bandwidth

---

### ENABLE_POCSAG
**Type:** `BOOL`
**Default:** `OFF`
**CMake Flag:** `-DENABLE_POCSAG=ON|OFF`

Enables POCSAG paging mode support.

**POCSAG Overview:**
- Post Office Code Standardisation Advisory Group
- Paging protocol (512, 1200, 2400 bps)
- Used for alphanumeric pagers
- Common in fire/EMS alerting

**When ON:**
- Includes `POCSAGRX.cpp`, `POCSAGTX.cpp`, `POCSAGDefines.h`
- Defines: `ENABLE_POCSAG`
- Adds POCSAG RX/TX support to modem

**When OFF:**
- POCSAG mode disabled
- Saves code space

**Example:**
```bash
# Enable POCSAG mode
cmake .. -DENABLE_POCSAG=ON

# Disable POCSAG mode
cmake .. -DENABLE_POCSAG=OFF
```

**Status:** Not yet implemented
**Auto-Handling:** If enabled but source files not found, CMake will automatically disable it with a warning.

---

## Build Examples

### Example 1: Raspberry Pi with PlutoSDR (Production)
```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DSTANDALONE_MODE=ON \
  -DPLUTO_SDR=ON \
  -DUSE_NEON=ON \
  -DUSE_TEXT_UI=ON \
  -DPROTOCOL_VERSION=2 \
  -DENABLE_FM_MODE=ON \
  -DBUILD_TESTS=OFF
make -j4
sudo make install
```

### Example 2: Development Build with All Features
```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSTANDALONE_MODE=ON \
  -DUSE_NEON=ON \
  -DUSE_TEXT_UI=ON \
  -DUSE_UDP_MODEM=ON \
  -DENABLE_FM_MODE=ON \
  -DENABLE_POCSAG=ON \
  -DBUILD_TESTS=ON
make -j4
ctest --verbose
```

### Example 3: Minimal Embedded Build (No UI, No Tests)
```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DSTANDALONE_MODE=ON \
  -DPLUTO_SDR=ON \
  -DUSE_NEON=ON \
  -DUSE_TEXT_UI=OFF \
  -DBUILD_TESTS=OFF
make -j4
```

### Example 4: x86 Testing Build (No NEON, UDP Modem)
```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSTANDALONE_MODE=OFF \
  -DUSE_NEON=OFF \
  -DUSE_UDP_MODEM=ON \
  -DBUILD_TESTS=ON
make -j4
```

### Example 5: Network Modem with UDP Transport
```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DSTANDALONE_MODE=ON \
  -DPLUTO_SDR=ON \
  -DUSE_NEON=ON \
  -DUSE_UDP_MODEM=ON \
  -DPROTOCOL_VERSION=2
make -j4

# Configure UDP addresses in Config.h before building
```

---

## Compiler Options

### CMAKE_BUILD_TYPE
**Valid Values:** `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`

**Debug:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```
- No optimization (-O0)
- Debug symbols (-g)
- Enables DEBUG macro
- Slower execution
- Larger binary

**Release:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```
- Full optimization (-O3)
- No debug symbols
- Fastest execution
- Smallest binary

**RelWithDebInfo:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```
- Optimization (-O2)
- Debug symbols (-g)
- Good for profiling

**MinSizeRel:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
```
- Size optimization (-Os)
- Minimal binary size
- Good for embedded systems

---

### Custom Compiler Flags

**Add custom flags:**
```bash
cmake .. -DCMAKE_CXX_FLAGS="-march=native -mtune=native"
```

**Cross-compilation example:**
```bash
cmake .. \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=arm \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
  -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++
```

---

## Dependencies

### Required (Always)
- **cmake** >= 3.10
- **g++** with C++11 support
- **pthread** (POSIX threads)
- **libm** (math library)
- **librt** (realtime library)

### Optional (Standalone Mode)
- **libiio-dev** >= 0.21 (PlutoSDR support)
  ```bash
  sudo apt-get install libiio-dev libiio-utils
  ```

- **libncurses5-dev** (Text UI)
  ```bash
  sudo apt-get install libncurses5-dev
  ```

### Optional (ZMQ Mode)
- **libzmq3-dev** >= 3.0 (GNU Radio integration)
  ```bash
  sudo apt-get install libzmq3-dev
  ```

### Build Tools
```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake git

# Fedora/RHEL
sudo dnf install gcc-c++ cmake git

# Arch Linux
sudo pacman -S base-devel cmake git
```

---

## Troubleshooting

### Problem: "libiio not found"
**Solution:**
```bash
# Install libiio
sudo apt-get install libiio-dev libiio-utils

# Or disable PlutoSDR support
cmake .. -DPLUTO_SDR=OFF
```

### Problem: "ncurses not found"
**Solution:**
```bash
# Install ncurses
sudo apt-get install libncurses5-dev

# Or disable text UI
cmake .. -DUSE_TEXT_UI=OFF
```

### Problem: "UDP modem source files not found"
**Solution:**
This is expected - UDP implementation is in progress. The build will continue with a warning. To implement:
1. See `docs/mmdvmhost/UDP_IMPLEMENTATION_PLAN.md`
2. Implement `UDPSocket.cpp/h` and `UDPModemPort.cpp/h`
3. Rebuild with `-DUSE_UDP_MODEM=ON`

### Problem: "POCSAG source files not found"
**Solution:**
POCSAG is not yet implemented. Either:
- Disable: `cmake .. -DENABLE_POCSAG=OFF`
- Or implement POCSAG RX/TX components (see existing mode implementations)

### Problem: "NEON optimizations not working"
**Check:**
```bash
# Verify ARM platform
uname -m
# Should show: armv7l, aarch64, or similar

# Check NEON support
cat /proc/cpuinfo | grep neon
# Should show "neon" in flags

# Force disable if needed
cmake .. -DUSE_NEON=OFF
```

### Problem: "undefined reference to symbol 'pthread_create'"
**Solution:**
```bash
# Should not happen with modern CMake
# If it does, ensure Threads::Threads is linked
# Check CMakeLists.txt has: find_package(Threads REQUIRED)
```

### Problem: Clean build after option changes
**Solution:**
```bash
# Remove build directory
rm -rf build

# Create fresh build
mkdir build && cd build
cmake .. [your options]
make -j4
```

---

## Build Option Reference Table

| Option | Type | Default | Purpose |
|--------|------|---------|---------|
| `STANDALONE_MODE` | BOOL | ON | Direct SDR control (no GNU Radio) |
| `USE_NEON` | BOOL | ON | ARM NEON SIMD optimizations |
| `USE_TEXT_UI` | BOOL | ON | ncurses text UI |
| `PLUTO_SDR` | BOOL | ON | ADALM PlutoSDR support |
| `BUILD_TESTS` | BOOL | ON | Build unit tests |
| `PROTOCOL_VERSION` | STRING | "2" | MMDVM protocol version (1 or 2) |
| `USE_UDP_MODEM` | BOOL | OFF | UDP socket transport |
| `USE_SHARED_MEMORY_IPC` | BOOL | OFF | Shared memory IPC (experimental) |
| `ENABLE_FM_MODE` | BOOL | ON | FM analog mode |
| `ENABLE_POCSAG` | BOOL | OFF | POCSAG paging mode |
| `CMAKE_BUILD_TYPE` | STRING | - | Debug, Release, RelWithDebInfo, MinSizeRel |

---

## Preprocessor Defines

These are automatically set based on build options:

| Define | Set When | Purpose |
|--------|----------|---------|
| `STANDALONE_MODE` | STANDALONE_MODE=ON | Enables standalone SDR code paths |
| `PLUTO_SDR` | PLUTO_SDR=ON and libiio found | Enables PlutoSDR driver |
| `HAVE_LIBIIO` | libiio detected | Indicates libiio availability |
| `USE_NEON` | USE_NEON=ON | Enables NEON intrinsics |
| `TEXT_UI` | USE_TEXT_UI=ON | Enables text UI code |
| `DEBUG` | CMAKE_BUILD_TYPE=Debug | Debug mode active |
| `RPI` | Always (Linux/ARM) | Raspberry Pi compatibility |
| `PROTOCOL_VERSION=X` | Always | MMDVM protocol version |
| `USE_UDP_MODEM` | USE_UDP_MODEM=ON | UDP transport active |
| `USE_SHARED_MEMORY_IPC` | USE_SHARED_MEMORY_IPC=ON | Shared memory active |
| `ENABLE_FM_MODE` | ENABLE_FM_MODE=ON | FM mode enabled |
| `ENABLE_POCSAG` | ENABLE_POCSAG=ON | POCSAG mode enabled |

---

## Version History

**1.0 (2025-11-17)**
- Initial documentation
- Core build options documented
- Protocol and transport options added
- Mode support options added
- Build examples provided

---

## See Also

- **README.md** - Project overview and quick start
- **STATUS.md** - Implementation status and roadmap
- **docs/mmdvmhost/UDP_IMPLEMENTATION_PLAN.md** - UDP modem implementation guide
- **docs/mmdvmhost/UDP_INTEGRATION.md** - UDP integration details
- **IMPLEMENTATION_SUMMARY.md** - Technical implementation details

---

**End of BUILD_OPTIONS.md**
