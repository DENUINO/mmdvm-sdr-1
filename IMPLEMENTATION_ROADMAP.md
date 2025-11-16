# Implementation Roadmap: Dual-Core Standalone DMR Hotspot

**Project:** mmdvm-sdr + MMDVMHost on Zynq-7010 Dual-Core
**Target:** PlutoSDR (Analog Devices ADALM-PLUTO)
**Timeline:** 8-12 weeks (estimated)
**Complexity:** Intermediate to Advanced

---

## Document Purpose

This roadmap provides a **step-by-step implementation plan** for transforming mmdvm-sdr into a fully standalone dual-core DMR hotspot on PlutoSDR. It covers:

1. **Current State Assessment** - What's already done
2. **Phase-by-Phase Implementation** - Detailed tasks with timelines
3. **Testing Strategy** - Validation at each stage
4. **Risk Mitigation** - Handling blockers
5. **Deployment** - Production rollout

---

## Current Implementation Status

### ✅ Completed (85% of Core Functionality)

Based on `STATUS.md`, the following components are **already implemented**:

| Component | Status | Files | Lines | Notes |
|-----------|--------|-------|-------|-------|
| **PlutoSDR Interface** | ✅ Complete | PlutoSDR.h/cpp | 693 | libiio-based, full RX/TX |
| **FM Modem** | ✅ Complete | FMModem.h/cpp | 753 | NEON hooks ready |
| **Resampler** | ✅ Complete | Resampler.h/cpp | 685 | Polyphase FIR |
| **NEON DSP Library** | ✅ Complete | arm_math_neon.h/cpp | 709 | 8-way SIMD |
| **Build System** | ✅ Complete | CMakeLists.txt | 304 | Conditional compilation |
| **Unit Tests** | ✅ Complete | test_*.cpp | 323 | Resampler, FM, NEON |
| **I/O Integration** | ✅ Complete | IORPiSDR.cpp | 348 | Standalone mode |
| **Configuration** | ✅ Complete | Config.h | - | Compile-time options |
| **Documentation** | ✅ Complete | *.md | 1917 | Analysis & summary |

**Remaining Work (15%):**
- Text UI implementation (UIStats.cpp, TextUI.h/cpp)
- Hardware testing on actual PlutoSDR
- Performance profiling and tuning
- MMDVMHost integration testing

---

## Implementation Phases

### Phase 0: Pre-Implementation Setup (Week 1)

**Goal:** Prepare development environment and hardware.

#### Tasks

**0.1 Hardware Acquisition**
- [ ] Acquire PlutoSDR (Rev C or later)
- [ ] Verify USB 3.0 host support (or 5V/2A power supply)
- [ ] Obtain DMR handheld radio (any brand, Tier II)
- [ ] Procure antenna (144 MHz or dual-band 144/430 MHz)
- [ ] Prepare Ethernet/Wi-Fi for PlutoSDR internet access

**0.2 Development Environment Setup**
- [ ] Install ARM cross-compiler toolchain:
  ```bash
  # Ubuntu/Debian
  sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

  # Or use Xilinx SDK
  wget https://www.xilinx.com/member/forms/download/xef.html?filename=Xilinx_SDK_2019.1.tar.gz
  ```
- [ ] Install dependencies:
  ```bash
  sudo apt-get install cmake libiio-dev libncurses5-dev git
  ```
- [ ] Clone repositories:
  ```bash
  git clone https://github.com/r4d10n/mmdvm-sdr
  git clone https://github.com/g4klx/MMDVMHost
  ```
- [ ] Verify PlutoSDR connectivity:
  ```bash
  iio_info -u ip:192.168.2.1
  # Should show ad9361-phy device
  ```

**0.3 Baseline Testing**
- [ ] Test PlutoSDR with existing GNU Radio flowgraph
- [ ] Verify DMR reception with handheld radio
- [ ] Benchmark existing performance (CPU, latency)
- [ ] Document baseline for comparison

**Deliverables:**
- Working development environment
- Verified PlutoSDR hardware
- Baseline performance metrics

**Estimated Time:** 1 week

---

### Phase 1: Standalone Build Validation (Week 2)

**Goal:** Verify standalone build system and core components compile correctly.

#### Tasks

**1.1 Standalone Build (Already Complete)**
- [x] Build in standalone mode:
  ```bash
  cd mmdvm-sdr/build
  cmake .. \
    -DSTANDALONE_MODE=ON \
    -DUSE_NEON=ON \
    -DPLUTO_SDR=ON \
    -DBUILD_TESTS=ON \
    -DCMAKE_BUILD_TYPE=Release
  make -j4
  ```
- [x] Verify all unit tests pass:
  ```bash
  ctest --verbose
  # Expected: All tests PASSED
  ```

**1.2 Cross-Compilation for Zynq**
- [ ] Create ARM toolchain file (`Toolchain-pluto.cmake`):
  ```cmake
  set(CMAKE_SYSTEM_NAME Linux)
  set(CMAKE_SYSTEM_PROCESSOR arm)
  set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
  set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
  set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_C_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard -O3")
  set(CMAKE_CXX_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard -O3")
  ```
- [ ] Cross-compile for ARM:
  ```bash
  mkdir build-arm && cd build-arm
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../Toolchain-pluto.cmake \
           -DSTANDALONE_MODE=ON -DUSE_NEON=ON -DPLUTO_SDR=ON
  make -j4
  ```
- [ ] Strip binary to reduce size:
  ```bash
  arm-linux-gnueabihf-strip mmdvm-sdr
  ls -lh mmdvm-sdr  # Should be ~800 KB
  ```

**1.3 Transfer to PlutoSDR**
- [ ] SSH into PlutoSDR:
  ```bash
  ssh root@192.168.2.1  # Password: analog
  ```
- [ ] Transfer binary:
  ```bash
  scp build-arm/mmdvm-sdr root@192.168.2.1:/tmp/
  ```
- [ ] Test execution (should fail gracefully without hardware init):
  ```bash
  ssh root@192.168.2.1
  /tmp/mmdvm-sdr --help
  # Should show usage info
  ```

**Deliverables:**
- ARM-compiled binary
- Successful execution on PlutoSDR
- Binary size <1 MB

**Estimated Time:** 3-5 days

---

### Phase 2: Hardware Integration (Week 3-4)

**Goal:** Integrate with PlutoSDR hardware, verify RF I/O.

#### Tasks

**2.1 PlutoSDR Initialization**
- [ ] Modify `main()` to initialize PlutoSDR:
  ```cpp
  // In main.cpp or IO.cpp
  #ifdef STANDALONE_MODE
  CPlutoSDR pluto;
  if (!pluto.init("ip:192.168.2.1", 1000000, 32768)) {
      fprintf(stderr, "Failed to init PlutoSDR\n");
      return 1;
  }
  pluto.setRXFrequency(144500000);  // 144.5 MHz
  pluto.setTXFrequency(145000000);  // 145.0 MHz
  pluto.setRXGain(50);
  pluto.setTXAttenuation(-30);
  #endif
  ```
- [ ] Test on PlutoSDR:
  ```bash
  ./mmdvm-sdr
  # Should initialize without errors
  ```

**2.2 RX Path Validation**
- [ ] Implement RX sample reading in main loop:
  ```cpp
  int16_t i_samples[32768], q_samples[32768];
  int nread = pluto.readRXSamples(i_samples, q_samples, 32768);
  // Process samples...
  ```
- [ ] Add RSSI/signal strength monitoring
- [ ] Test with RF signal (key up DMR handheld)
- [ ] Verify IQ samples are valid (not all zeros)
- [ ] Plot spectrum using GNU Radio Companion or inspectrum

**2.3 TX Path Validation**
- [ ] Implement TX sample writing:
  ```cpp
  int nwritten = pluto.writeTXSamples(i_samples, q_samples, numSamples);
  ```
- [ ] Generate test tone (1 kHz sine wave)
- [ ] Transmit and verify with spectrum analyzer or SDR receiver
- [ ] Measure frequency accuracy (should be ±1 kHz)

**2.4 Duplex Operation**
- [ ] Enable simultaneous RX and TX
- [ ] Test RX/TX frequency separation (500 kHz typical)
- [ ] Monitor for DMA underruns/overruns
- [ ] Verify no self-interference

**Deliverables:**
- Working PlutoSDR interface
- Verified RX and TX paths
- Spectrum plots demonstrating RF output

**Estimated Time:** 1-2 weeks

---

### Phase 3: FM Modem and Resampler Integration (Week 5)

**Goal:** Complete signal chain from RF to baseband.

#### Tasks

**3.1 RX Signal Chain**
- [ ] Integrate resampler (1 MHz → 24 kHz):
  ```cpp
  CDecimatingResampler rxResampler(3, 125, firCoeffs, 42);

  // In RX loop:
  pluto.readRXSamples(iq_1mhz, ...);
  rxResampler.decimate(iq_1mhz, iq_24khz, nsamples);
  ```
- [ ] Add FM demodulator:
  ```cpp
  CFMModem fmModem;
  fmModem.demodulate(iq_24khz, audio_24khz, nsamples);
  ```
- [ ] Apply RRC filter (existing code in IO.cpp):
  ```cpp
  arm_fir_fast_q15_neon(&rrcFilter, audio_24khz, symbols, nsamples);
  ```

**3.2 TX Signal Chain**
- [ ] Integrate FM modulator:
  ```cpp
  fmModem.modulate(symbols, iq_24khz, nsamples);
  ```
- [ ] Apply RRC filter (pre-modulation)
- [ ] Integrate resampler (24 kHz → 1 MHz):
  ```cpp
  CInterpolatingResampler txResampler(125, 3, firCoeffs, 42);
  txResampler.interpolate(iq_24khz, iq_1mhz, nsamples);
  ```
- [ ] Write to PlutoSDR TX

**3.3 Loopback Testing**
- [ ] Create RX→TX loopback (no modem processing)
- [ ] Inject audio tone at 1 kHz
- [ ] Verify tone appears at RF output
- [ ] Measure SNR and distortion (THD <1%)

**3.4 NEON Optimization Validation**
- [ ] Profile hotspots using `perf`:
  ```bash
  perf record -F 99 -g ./mmdvm-sdr
  perf report
  ```
- [ ] Verify NEON functions are being used (check assembly)
- [ ] Benchmark NEON vs scalar:
  ```bash
  # Build with and without NEON, compare CPU usage
  top -d 1  # Press '1' for per-core view
  ```

**Deliverables:**
- Complete RX and TX signal chains
- Loopback test passing
- NEON optimizations verified

**Estimated Time:** 1 week

---

### Phase 4: DMR Mode Integration (Week 6-7)

**Goal:** Enable DMR protocol operation.

#### Tasks

**4.1 DMR RX Path**
- [ ] Enable DMRRX.cpp mode in main loop
- [ ] Verify frame sync detection (0x75, 0x4F, ...)
- [ ] Test slot type decoding (voice, data, CACH)
- [ ] Validate embedded LC parsing
- [ ] Monitor BER (Bit Error Rate)

**4.2 DMR TX Path**
- [ ] Enable DMRTX.cpp mode
- [ ] Generate DMR preamble and sync patterns
- [ ] Test voice frame encoding
- [ ] Verify TDMA slot timing (30 ms slots)

**4.3 Virtual PTY Creation**
- [ ] Implement PTY master in mmdvm-sdr:
  ```cpp
  int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
  grantpt(master_fd);
  unlockpt(master_fd);
  char *ptyPath = ptsname(master_fd);
  printf("Modem PTY: %s\n", ptyPath);

  // Create symlink for stable path
  symlink(ptyPath, "/tmp/ttyMMDVM0");
  ```
- [ ] Test read/write from host:
  ```bash
  echo "AT+GET_STATUS" > /tmp/ttyMMDVM0
  cat /tmp/ttyMMDVM0
  ```

**4.4 Modem Protocol Implementation**
- [ ] Implement MMDVM serial protocol (existing in IO.cpp):
  - GET_VERSION
  - GET_STATUS
  - SET_MODE
  - DMR_DATA (RX frames)
  - DMR_DATA (TX frames)
- [ ] Test with manual commands via `screen /tmp/ttyMMDVM0 115200`

**Deliverables:**
- Working DMR RX/TX
- Virtual PTY communication
- MMDVM protocol responses

**Estimated Time:** 1-2 weeks

---

### Phase 5: MMDVMHost Integration (Week 8)

**Goal:** Connect MMDVMHost to mmdvm-sdr and achieve end-to-end operation.

#### Tasks

**5.1 MMDVMHost Compilation**
- [ ] Clone and modify MMDVMHost:
  ```bash
  git clone https://github.com/g4klx/MMDVMHost
  cd MMDVMHost

  # Disable RTS check (Modem.cpp line 120)
  sed -i 's/m_serial->setRTS(true)/m_serial->setRTS(false)/' Modem.cpp

  # Cross-compile for ARM
  export CXX=arm-linux-gnueabihf-g++
  make -j4
  arm-linux-gnueabihf-strip MMDVMHost
  ```
- [ ] Transfer to PlutoSDR:
  ```bash
  scp MMDVMHost root@192.168.2.1:/usr/bin/
  ```

**5.2 Configuration**
- [ ] Create `/etc/MMDVM.ini` on PlutoSDR:
  ```ini
  [Modem]
  Port=/tmp/ttyMMDVM0
  TXInvert=1
  RXInvert=1
  # ... (see STANDALONE_DESIGN.md for full config)

  [DMR Network]
  Enable=1
  Address=dmr.example.com  # Your DMR network
  Port=62031
  Password=your_password
  ```

**5.3 Manual Testing**
- [ ] Start mmdvm-sdr:
  ```bash
  ssh root@192.168.2.1
  taskset -c 0 /usr/bin/mmdvm-sdr &
  ```
- [ ] Start MMDVMHost:
  ```bash
  taskset -c 1 /usr/bin/MMDVMHost /etc/MMDVM.ini
  ```
- [ ] Check logs for successful connection:
  ```
  M: 2025-11-16 12:00:00.000 MMDVM protocol version: 1, description: MMDVM_SDR
  M: 2025-11-16 12:00:00.001 DMR, Transmit: 145.0000 MHz, Receive: 144.5000 MHz
  ```

**5.4 End-to-End Testing**
- [ ] Key up DMR handheld on RX frequency (144.5 MHz)
- [ ] Verify MMDVMHost receives frames:
  ```
  D: 2025-11-16 12:01:00.000 DMR Slot 2, received RF voice header from N0CALL to TG 99
  ```
- [ ] Verify network transmission (check DMR master logs)
- [ ] Test reverse path (network → mmdvm-sdr → RF)
- [ ] Measure latency (RX air → network, network → TX air)

**Deliverables:**
- Working end-to-end DMR hotspot
- Voice traffic passing RX and TX
- Network connectivity verified

**Estimated Time:** 1 week

---

### Phase 6: Dual-Core Optimization (Week 9)

**Goal:** Implement CPU isolation and real-time tuning.

#### Tasks

**6.1 Kernel Configuration**
- [ ] Enable second CPU core:
  ```bash
  echo 1 > /sys/devices/system/cpu/cpu1/online
  cat /proc/cpuinfo | grep processor  # Should show 0 and 1
  ```
- [ ] Set CPU governor to performance:
  ```bash
  echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
  echo performance > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
  ```

**6.2 Boot Parameters (Persistent)**
- [ ] Modify PlutoSDR boot configuration:
  ```bash
  # On PlutoSDR, edit /media/mmcblk0p1/uEnv.txt or /boot/cmdline.txt
  nano /media/mmcblk0p1/uEnv.txt

  # Add to bootargs:
  bootargs=... isolcpus=0,1 nohz_full=0,1 rcu_nocbs=0,1 irqaffinity=0 maxcpus=2

  # Reboot
  reboot
  ```
- [ ] Verify isolation after reboot:
  ```bash
  cat /proc/cmdline | grep isolcpus
  ```

**6.3 Process Affinity**
- [ ] Pin mmdvm-sdr to Core 0:
  ```bash
  taskset -c 0 chrt -f 80 /usr/bin/mmdvm-sdr &
  ```
- [ ] Pin MMDVMHost to Core 1:
  ```bash
  taskset -c 1 chrt -f 70 /usr/bin/MMDVMHost /etc/MMDVM.ini &
  ```
- [ ] Verify affinity:
  ```bash
  ps -eo pid,psr,rtprio,comm | grep -E "mmdvm|MMDVM"
  #   PID  PSR  RTPRIO  COMMAND
  #  1234   0    80     mmdvm-sdr
  #  1235   1    70     MMDVMHost
  ```

**6.4 IRQ Affinity**
- [ ] Route hardware IRQs to Core 0:
  ```bash
  # Find IRQ numbers
  cat /proc/interrupts | grep -E "eth0|iio"

  # Set affinity (example)
  echo 1 > /proc/irq/54/smp_affinity  # Core 0 only (bitmask)
  ```

**6.5 Performance Monitoring**
- [ ] Measure CPU usage per core:
  ```bash
  top -d 1  # Press '1' to show per-core
  # Expected: Core 0 ~30-35%, Core 1 ~5-10%
  ```
- [ ] Measure context switches:
  ```bash
  vmstat 1
  # "cs" column should be <1000/sec
  ```
- [ ] Measure latency jitter:
  ```bash
  # (Requires custom timestamping in code)
  # Log RX-to-TX latency for 1000 frames
  # Calculate: mean, stddev, max
  ```

**Deliverables:**
- Dual-core operation with CPU isolation
- Verified process affinity (Core 0/1)
- Performance metrics (CPU, latency, jitter)

**Estimated Time:** 1 week

---

### Phase 7: Shared Memory IPC (Week 10) - Optional Enhancement

**Goal:** Replace virtual PTY with high-performance shared memory.

**Note:** This phase is optional but recommended for optimal performance.

#### Tasks

**7.1 Shared Memory Implementation (mmdvm-sdr side)**
- [ ] Create shared memory region:
  ```cpp
  #include <sys/mman.h>
  #include <fcntl.h>

  int shm_fd = shm_open("/mmdvm_ipc", O_CREAT | O_RDWR, 0666);
  ftruncate(shm_fd, sizeof(struct mmdvm_shared_memory));
  struct mmdvm_shared_memory *shm = mmap(NULL, sizeof(*shm),
      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  ```
- [ ] Implement lock-free ring buffers (see STANDALONE_DESIGN.md §4.3)
- [ ] Write RX audio samples to `shm->rx_ring`
- [ ] Read TX audio samples from `shm->tx_ring`

**7.2 MMDVMHost Modification**
- [ ] Modify `Modem.cpp` to support shared memory mode:
  ```cpp
  #ifdef USE_SHARED_MEMORY
  int shm_fd = shm_open("/mmdvm_ipc", O_RDWR, 0666);
  struct mmdvm_shared_memory *shm = mmap(...);

  // In readDMRData1():
  while (available()) {
      int16_t sample = ringbuffer_read(&shm->rx_ring);
      // ... decode
  }
  #endif
  ```
- [ ] Add compile-time flag `-DUSE_SHARED_MEMORY`
- [ ] Test both modes (PTY and shared memory)

**7.3 Performance Comparison**
- [ ] Measure latency: PTY vs Shared Memory
- [ ] Measure CPU overhead: PTY vs Shared Memory
- [ ] Measure jitter: PTY vs Shared Memory
- [ ] Document results

**Deliverables:**
- Shared memory IPC implementation
- Performance comparison data
- Recommendation for production mode

**Estimated Time:** 1 week (if pursued)

---

### Phase 8: System Integration and Testing (Week 11)

**Goal:** Comprehensive testing and hardening.

#### Tasks

**8.1 Systemd Service**
- [ ] Create `/etc/systemd/system/mmdvm-hotspot.service`:
  ```ini
  [Unit]
  Description=MMDVM Standalone Hotspot
  After=network.target

  [Service]
  Type=forking
  ExecStartPre=/bin/sh -c 'echo 1 > /sys/devices/system/cpu/cpu1/online'
  ExecStart=/usr/bin/taskset -c 0 /usr/bin/chrt -f 80 /usr/bin/mmdvm-sdr --daemon
  ExecStartPost=/bin/sleep 2
  ExecStartPost=/usr/bin/taskset -c 1 /usr/bin/chrt -f 70 /usr/bin/MMDVMHost /etc/MMDVM.ini &
  Restart=on-failure

  [Install]
  WantedBy=multi-user.target
  ```
- [ ] Enable and test:
  ```bash
  systemctl enable mmdvm-hotspot.service
  systemctl start mmdvm-hotspot.service
  systemctl status mmdvm-hotspot.service
  ```
- [ ] Test auto-restart on failure:
  ```bash
  killall mmdvm-sdr
  # Should auto-restart within 10 seconds
  ```

**8.2 Stability Testing**
- [ ] 24-hour burn-in test:
  - Monitor CPU temperature
  - Check for memory leaks (`free -h` every hour)
  - Count crashes and restarts
  - Monitor network connectivity
- [ ] Stress test:
  - Continuous RX traffic (leave handheld keyed)
  - Continuous TX traffic (network PTT script)
  - Rapid mode switching (if supported)
- [ ] Edge cases:
  - Network disconnect/reconnect
  - RF interference
  - Power brownout (if using USB)

**8.3 Error Handling**
- [ ] Test PlutoSDR disconnect/reconnect
- [ ] Test buffer underrun recovery
- [ ] Test invalid DMR frames
- [ ] Verify watchdog timers trigger correctly

**8.4 Logging and Monitoring**
- [ ] Implement syslog integration:
  ```cpp
  #include <syslog.h>
  openlog("mmdvm-sdr", LOG_PID, LOG_DAEMON);
  syslog(LOG_INFO, "PlutoSDR initialized");
  ```
- [ ] Create monitoring script:
  ```bash
  #!/bin/bash
  # /usr/local/bin/mmdvm-monitor.sh
  while true; do
      CPU=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}')
      TEMP=$(cat /sys/bus/iio/devices/iio:device*/in_temp0_input)
      MEM=$(free -m | awk 'NR==2{print $3}')
      echo "$(date) CPU:$CPU% TEMP:$((TEMP/1000))°C MEM:${MEM}MB"
      sleep 60
  done
  ```

**Deliverables:**
- Systemd service working
- 24-hour stability test passed
- Error recovery verified
- Monitoring infrastructure in place

**Estimated Time:** 1 week

---

### Phase 9: Performance Tuning (Week 12)

**Goal:** Optimize for maximum performance and efficiency.

#### Tasks

**9.1 Profiling**
- [ ] Install `perf` on PlutoSDR:
  ```bash
  # May need to cross-compile perf for ARM
  # Or use gprof-based profiling
  ```
- [ ] Profile mmdvm-sdr:
  ```bash
  perf record -F 99 -g -p $(pidof mmdvm-sdr) sleep 30
  perf report
  ```
- [ ] Identify top 5 hotspots
- [ ] Document current performance baseline

**9.2 NEON Optimization Tuning**
- [ ] Review NEON assembly output:
  ```bash
  arm-linux-gnueabihf-objdump -d build-arm/mmdvm-sdr | grep -A20 "arm_fir_fast_q15_neon"
  ```
- [ ] Verify vectorization is happening:
  - Look for `vmlal.s16` (NEON multiply-accumulate)
  - Look for `vld1` / `vst1` (NEON load/store)
- [ ] Tune loop unrolling and alignment
- [ ] Benchmark before/after

**9.3 Cache Optimization**
- [ ] Align critical data structures to cache lines (32 bytes):
  ```cpp
  struct __attribute__((aligned(32))) RingBuffer {
      uint32_t write_index;
      uint8_t _pad1[28];
      uint32_t read_index;
      uint8_t _pad2[28];
      int16_t samples[65536];
  };
  ```
- [ ] Use `__builtin_prefetch()` for predictable access:
  ```cpp
  __builtin_prefetch(&buffer[i + 8], 0, 3);  // Prefetch for read
  ```
- [ ] Measure L1/L2 cache miss rates:
  ```bash
  perf stat -e cache-references,cache-misses ./mmdvm-sdr
  ```

**9.4 DMA and Buffer Tuning**
- [ ] Experiment with buffer sizes:
  - Small (16 KB): Lower latency, higher CPU
  - Medium (32 KB): Balanced (current)
  - Large (64 KB): Lower CPU, higher latency
- [ ] Tune based on measurements:
  - Target: <10 ms latency, <35% CPU
- [ ] Verify no underruns/overruns under load

**9.5 Compiler Optimization**
- [ ] Test different optimization levels:
  ```bash
  # Current: -O3
  # Try: -O3 -ffast-math -funroll-loops -ftree-vectorize
  ```
- [ ] Enable Link-Time Optimization (LTO):
  ```cmake
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  ```
- [ ] Profile-Guided Optimization (PGO):
  ```bash
  # 1. Build with instrumentation
  CFLAGS="-fprofile-generate" make
  # 2. Run workload
  ./mmdvm-sdr --profile-run
  # 3. Rebuild with profiling data
  CFLAGS="-fprofile-use" make
  ```

**9.6 Final Benchmarks**
- [ ] Measure final CPU usage (target: <35%)
- [ ] Measure final latency (target: <10 ms)
- [ ] Measure jitter (target: <1 ms stddev)
- [ ] Measure temperature (target: <70°C)
- [ ] Compare to baseline from Phase 0

**Deliverables:**
- Optimized binary with benchmarks
- Performance report (before/after)
- Recommended settings for production

**Estimated Time:** 1 week

---

### Phase 10: Documentation and Release (Week 13+)

**Goal:** Prepare for public release and community adoption.

#### Tasks

**10.1 User Documentation**
- [ ] Update README.md with standalone mode instructions
- [ ] Create BUILD.md (compilation guide)
- [ ] Create INSTALL.md (deployment guide)
- [ ] Create CONFIG.md (configuration reference)
- [ ] Create TROUBLESHOOTING.md (common issues)

**10.2 Developer Documentation**
- [ ] Document code architecture (Doxygen)
- [ ] Create CONTRIBUTING.md (how to contribute)
- [ ] Document NEON optimizations (guide for other devs)
- [ ] Create API reference

**10.3 Testing Suite**
- [ ] Create automated test scripts:
  - Hardware loopback test
  - DMR protocol test vectors
  - Performance regression tests
- [ ] Document test procedures

**10.4 Release Preparation**
- [ ] Tag release version:
  ```bash
  git tag -a v2.0-standalone -m "Standalone dual-core release"
  git push origin v2.0-standalone
  ```
- [ ] Create GitHub release with binaries:
  - Pre-compiled ARM binary for PlutoSDR
  - Source tarball
  - Release notes
- [ ] Update project website / wiki

**10.5 Community Outreach**
- [ ] Post on ham radio forums (Reddit r/amateurradio, etc.)
- [ ] Create demonstration video (YouTube)
- [ ] Write blog post / article
- [ ] Submit to PlutoSDR community showcase

**Deliverables:**
- Complete documentation set
- GitHub release with binaries
- Community awareness

**Estimated Time:** 1-2 weeks (ongoing)

---

## Testing Strategy

### Unit Testing

**Test Coverage:**
- [x] Resampler accuracy (frequency response, aliasing)
- [x] FM modulator/demodulator (THD, deviation)
- [x] NEON functions (equivalence to scalar)
- [ ] PlutoSDR interface (mock hardware)
- [ ] Ring buffer thread safety (stress test)
- [ ] DMR frame encoding/decoding

**Tools:**
- CTest (CMake test framework)
- Google Test (for C++ unit tests)
- Valgrind (memory leak detection)

**Command:**
```bash
cd build
ctest --verbose
valgrind --leak-check=full ./test_resampler
```

### Integration Testing

**Test Scenarios:**
1. **Loopback Test:** mmdvm-sdr RX → TX (no modem processing)
2. **RF Loopback:** PlutoSDR TX → Attenuator → PlutoSDR RX
3. **End-to-End:** Handheld → mmdvm-sdr → MMDVMHost → Network
4. **Reverse Path:** Network → MMDVMHost → mmdvm-sdr → Handheld

**Validation:**
- [ ] No crashes after 24 hours
- [ ] CPU usage stable (no creep)
- [ ] Memory usage stable (no leaks)
- [ ] Temperature stable (<70°C)
- [ ] BER <1% on clean RF signal

### Performance Testing

**Benchmarks:**
- CPU usage (target: <35% Core 0, <10% Core 1)
- Latency (target: <10 ms end-to-end)
- Jitter (target: <1 ms stddev)
- Throughput (target: 50 fps DMR voice)
- Temperature (target: <70°C sustained)

**Tools:**
- `top`, `htop` (CPU monitoring)
- `perf` (profiling)
- Custom timestamping in code
- IIO temperature sensor

---

## Risk Mitigation

### Identified Risks

| Risk | Probability | Impact | Mitigation Strategy |
|------|------------|--------|---------------------|
| **Insufficient CPU** | Low | High | NEON optimizations proven, 55% headroom expected |
| **PlutoSDR Hardware Bugs** | Medium | High | Test with multiple PlutoSDR units, firmware updates |
| **Cache Coherency Issues** | Low | Medium | Use atomic operations, test under stress |
| **Thermal Throttling** | Medium | Medium | Monitor temps, add cooling if needed |
| **Memory Leaks** | Medium | High | Valgrind testing, 24-hour soak tests |
| **Network Instability** | High | Low | Implement reconnect logic, keep-alive timers |
| **MMDVMHost Incompatibility** | Low | High | Test with multiple versions, maintain backward compat |
| **Flash Storage Full** | Medium | Medium | Use stripped binaries, SquashFS compression |

### Contingency Plans

**If CPU usage exceeds 50%:**
- Reduce sample rate from 1 MHz to 500 kHz
- Disable non-critical features (UI, logging)
- Profile and optimize hotspots further
- Consider using PL (FPGA) for FIR offload

**If latency exceeds 10 ms:**
- Reduce buffer sizes
- Increase process priority (RT 90+)
- Apply PREEMPT_RT kernel patch
- Use OCM for critical buffers

**If temperature exceeds 75°C:**
- Add active cooling (fan)
- Reduce TX power
- Lower CPU frequency to 500 MHz
- Improve airflow in enclosure

**If memory exceeds 100 MB:**
- Reduce buffer sizes
- Disable debug logging
- Strip unnecessary features
- Use jemalloc or tcmalloc (better allocators)

---

## Success Criteria

### Minimum Viable Product (MVP)

- [x] Compiles in standalone mode
- [ ] Runs on PlutoSDR hardware
- [ ] RX path: RF → DMR frames
- [ ] TX path: DMR frames → RF
- [ ] MMDVMHost integration (PTY)
- [ ] Network connectivity (DMR+/BM)
- [ ] Stable for 1 hour

### Production Ready

- [ ] Stable for 24+ hours
- [ ] CPU usage <50% total
- [ ] Latency <10 ms
- [ ] Temperature <70°C
- [ ] Automatic restart on failure
- [ ] Systemd service integration
- [ ] User documentation complete

### Stretch Goals

- [ ] Shared memory IPC (lower latency)
- [ ] Text UI (ncurses status display)
- [ ] Web UI (remote configuration)
- [ ] OLED display support
- [ ] Multi-mode (DMR + POCSAG)
- [ ] PL offload (FPGA acceleration)

---

## Timeline Summary

| Week | Phase | Key Deliverable |
|------|-------|----------------|
| 1 | Phase 0 | Development environment ready |
| 2 | Phase 1 | ARM binary compiles and runs |
| 3-4 | Phase 2 | PlutoSDR RF I/O working |
| 5 | Phase 3 | FM modem + resampler integrated |
| 6-7 | Phase 4 | DMR protocol operational |
| 8 | Phase 5 | MMDVMHost end-to-end working |
| 9 | Phase 6 | Dual-core optimization complete |
| 10 | Phase 7 | Shared memory IPC (optional) |
| 11 | Phase 8 | System integration and testing |
| 12 | Phase 9 | Performance tuning |
| 13+ | Phase 10 | Documentation and release |

**Total Time:** 8-12 weeks (2-3 months)

**Critical Path:** Phases 0-6, 8-9 (Phases 7 and 10 can overlap)

---

## Resource Requirements

### Personnel

- **Lead Developer:** 1 person, 20-30 hours/week
- **Tester:** 1 person, 10-15 hours/week (can be same person)
- **Technical Writer:** 1 person, 5-10 hours/week (Phase 10)

### Hardware

- PlutoSDR (Rev C or later): ~$150-200
- DMR handheld radio: ~$100-300
- Antenna: ~$20-50
- USB 3.0 hub or 5V PSU: ~$10-20
- Optional: Spectrum analyzer or second SDR for validation

**Total Cost:** ~$300-600

### Software

- Development PC (Linux recommended)
- Cross-compiler toolchain (free)
- Git, CMake, Perf (free)
- Optional: MATLAB/Octave for filter design (free)

---

## Conclusion

This implementation roadmap provides a **structured, phase-by-phase approach** to transforming mmdvm-sdr into a fully standalone dual-core DMR hotspot on PlutoSDR. With 85% of the core functionality already complete, the remaining work focuses on:

1. **Hardware integration** (PlutoSDR RF I/O)
2. **Protocol testing** (DMR end-to-end)
3. **Optimization** (dual-core, real-time tuning)
4. **Hardening** (stability, error recovery)
5. **Documentation** (user/developer guides)

**Expected Timeline:** 8-12 weeks from start to production release.

**Expected Outcome:** A robust, efficient, and community-ready standalone DMR hotspot that maximizes the Zynq-7010's dual-core capabilities.

---

**Document Version:** 1.0
**Last Updated:** 2025-11-16
**Author:** mmdvm-sdr Development Team
**License:** GNU GPL v2.0
**Next Review:** After Phase 6 completion
