# Zynq 7010 Architecture Analysis for MMDVMHost Dual-Core Operation

**Date:** 2025-11-16
**Target Platform:** Xilinx Zynq-7010 SoC (PlutoSDR)
**Application:** Standalone DMR Hotspot (mmdvm-sdr + MMDVMHost)

---

## Executive Summary

The Xilinx Zynq-7010 is a System-on-Chip (SoC) that combines a dual-core ARM Cortex-A9 processor (Processing System - PS) with programmable logic (PL) on a single 28nm die. This analysis evaluates the feasibility of running **mmdvm-sdr on Core 0** and **MMDVMHost on Core 1** to create a fully standalone DMR hotspot on PlutoSDR.

**Key Finding:** ✅ **FEASIBLE** - The Zynq-7010 dual-core architecture can support both applications with proper CPU isolation, real-time kernel configuration, and optimized inter-core communication.

---

## 1. Zynq-7010 Hardware Architecture

### 1.1 Processing System (PS) Specifications

#### ARM Cortex-A9 Dual-Core Processor

| Specification | Value | Notes |
|--------------|-------|-------|
| **CPU Cores** | 2x ARM Cortex-A9 | Symmetric cores, full NEON SIMD |
| **Clock Frequency** | 667 MHz | PlutoSDR configuration |
| **Max Frequency** | 866 MHz | Can be overclocked (with thermal considerations) |
| **Architecture** | ARMv7-A | 32-bit instruction set |
| **NEON SIMD** | Yes | 128-bit vector operations |
| **FPU** | VFPv3 | Single/double precision |
| **Instruction Cache (L1i)** | 32 KB per core | 4-way associative |
| **Data Cache (L1d)** | 32 KB per core | 4-way associative |
| **L2 Cache (Shared)** | 512 KB | 8-way associative, PL310 controller |
| **On-Chip Memory (OCM)** | 256 KB | Low-latency shared memory |

#### Cache Performance Characteristics

| Cache Level | Latency (cycles) | Bandwidth | Configuration |
|-------------|-----------------|-----------|---------------|
| **L1 Instruction** | 1-2 cycles | ~10 GB/s @ 667MHz | 32KB, 4-way, per core |
| **L1 Data** | 1-4 cycles | ~8 GB/s @ 667MHz | 32KB, 4-way, per core |
| **L2 Unified** | 31-55 cycles | ~4 GB/s @ 667MHz | 512KB, 8-way, shared |
| **DDR3 RAM** | ~200 cycles | ~2.4 GB/s @ DDR3-1066 | External DRAM |

**Cache Coherency:**
- Hardware cache coherency via ARM Snoop Control Unit (SCU)
- Automatic invalidation/write-back between cores
- L1 caches are kept coherent with L2 through SCU
- No software intervention needed for most shared memory operations

### 1.2 Memory Subsystem

#### PlutoSDR Memory Configuration

| Component | Capacity | Type | Bandwidth | Usage |
|-----------|----------|------|-----------|-------|
| **DDR3 SDRAM** | 512 MB | DDR3-1066 (32-bit) | ~2.4 GB/s | Main system memory |
| **OCM (On-Chip)** | 256 KB | SRAM | ~10 GB/s | Critical buffers, shared memory |
| **Flash** | 32 MB | Quad-SPI | ~10 MB/s | Boot image, rootfs |
| **L2 Cache** | 512 KB | SRAM | ~4 GB/s | Shared cache |

#### Memory Map (Relevant Regions)

```
0x0000_0000 - 0x1FFF_FFFF : DDR3 RAM (512 MB)
    Core 0 Region: 0x0000_0000 - 0x0FFF_FFFF (256 MB)
    Core 1 Region: 0x1000_0000 - 0x1FFF_FFFF (256 MB)
    Shared Region: Can be allocated flexibly

0xFFFC_0000 - 0xFFFF_FFFF : On-Chip Memory (256 KB)
    Ideal for inter-core communication buffers
    Low latency, high bandwidth

0xE000_0000 - 0xE02F_FFFF : I/O Peripherals
    UART, GPIO, SPI, I2C, USB
```

### 1.3 Interrupt System

#### Generic Interrupt Controller (GIC)

The Zynq-7010 includes an ARM GIC-390 with the following capabilities:

| Feature | Specification | Application |
|---------|--------------|-------------|
| **Software Generated Interrupts (SGI)** | 0-15 | Inter-processor interrupts (IPI) |
| **Private Peripheral Interrupts (PPI)** | 16-31 | Per-core timers, watchdogs |
| **Shared Peripheral Interrupts (SPI)** | 32-95 | Common peripherals |
| **Interrupt Routing** | Programmable | Can route to specific core |
| **Priority Levels** | 32 levels | Hardware prioritization |

**Inter-Processor Interrupt (IPI) Mechanism:**
- Core 0 can signal Core 1 via SGI 0-15
- Latency: ~200-500 ns (sub-microsecond)
- Used by Linux SMP scheduler and OpenAMP framework

### 1.4 Programmable Logic (PL)

| Specification | Value | Notes |
|--------------|-------|-------|
| **Logic Cells** | 28,000 | Artix-7 equivalent |
| **DSP Slices** | 80 | For signal processing acceleration |
| **Block RAM** | 2.1 Mb | Fast on-chip memory |
| **AXI Interfaces** | Multiple | PS-PL interconnect |

**Note:** For this application, PL is used by the AD9363 RF transceiver interface. We won't modify the PL design, only leverage the existing PlutoSDR configuration.

---

## 2. PlutoSDR-Specific Configuration

### 2.1 AD9363 RF Transceiver Integration

The PlutoSDR uses the AD9363 RF transceiver connected to the Zynq-7010 via:
- **Data Interface:** 12-bit parallel LVDS to PL (FPGA fabric)
- **Control Interface:** SPI from PS to AD9363
- **Sample Rate:** 1-61.44 MSPS (we use 1 MSPS)
- **Frequency Range:** 325 MHz - 3.8 GHz (hackable to 70 MHz - 6 GHz)

### 2.2 Existing Software Stack

The PlutoSDR runs:
- **Linux Kernel:** 4.x / 5.x (Analog Devices customized)
- **Root Filesystem:** BusyBox-based (minimal)
- **IIO Drivers:** libiio for AD9363 control
- **USB Gadget:** Ethernet, serial, mass storage

### 2.3 Available Resources for Applications

| Resource | Total | Kernel/System | Available for Apps | Notes |
|----------|-------|---------------|-------------------|-------|
| **RAM** | 512 MB | ~150 MB | ~360 MB | Sufficient for both apps |
| **CPU Core 0** | 100% | ~5-10% idle | ~90% | mmdvm-sdr target: 30-35% |
| **CPU Core 1** | 100% | ~2-5% idle | ~95% | MMDVMHost target: 5-10% |
| **Flash** | 32 MB | ~25 MB | ~7 MB | Tight, but workable |

---

## 3. Dual-Core Operation Modes

The Zynq-7010 supports two primary multiprocessing models:

### 3.1 Symmetric Multiprocessing (SMP) - **RECOMMENDED**

**Description:** Both cores run a single Linux kernel instance with shared memory space.

**Advantages:**
- ✅ Standard Linux kernel (no custom builds needed)
- ✅ Automatic load balancing across cores
- ✅ Shared memory is trivial (just malloc)
- ✅ Well-supported, mature ecosystem
- ✅ Standard IPC mechanisms (pipes, shared memory, sockets)
- ✅ Easier debugging and monitoring

**Disadvantages:**
- ⚠️ Less deterministic (scheduler can move tasks)
- ⚠️ Cache coherency overhead (minimal for our use case)
- ⚠️ Requires CPU affinity/isolation for optimal performance

**For This Application:**
- Run mmdvm-sdr on Core 0 with CPU pinning (`taskset -c 0`)
- Run MMDVMHost on Core 1 with CPU pinning (`taskset -c 1`)
- Use kernel parameters: `isolcpus=0,1` to prevent scheduler interference
- Use shared memory or localhost UDP for communication

### 3.2 Asymmetric Multiprocessing (AMP)

**Description:** Each core runs a separate OS/application (e.g., Linux on Core 0, bare-metal on Core 1).

**Advantages:**
- ✅ True isolation between cores
- ✅ Deterministic real-time on bare-metal core
- ✅ No scheduler overhead on isolated core

**Disadvantages:**
- ❌ Requires custom kernel build with OpenAMP
- ❌ Complex setup (device tree, remoteproc, rpmsg)
- ❌ Harder debugging
- ❌ More development effort
- ❌ Limited to RPMsg buffers (512 bytes max)

**For This Application:**
- Overkill for our needs
- MMDVMHost doesn't require bare-metal performance
- Not recommended unless SMP proves insufficient

---

## 4. Inter-Core Communication Mechanisms

### 4.1 Shared Memory (POSIX SHM) - **RECOMMENDED**

**Implementation:** Standard POSIX shared memory API

```c
// Create shared memory region
int shm_fd = shm_open("/mmdvm_buffer", O_CREAT | O_RDWR, 0666);
ftruncate(shm_fd, BUFFER_SIZE);
void *ptr = mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
```

**Performance:**
- Latency: ~1-5 μs (memory copy only)
- Bandwidth: ~1-2 GB/s (L2 cache speed)
- Zero-copy possible with lock-free ring buffers

**Synchronization:**
- POSIX semaphores or futex for signaling
- Lock-free ring buffers for high throughput

**Use Case:** Large data transfers (audio samples, frames)

### 4.2 UDP Loopback (localhost)

**Implementation:** Standard BSD sockets via loopback interface

```c
// Core 0 (mmdvm-sdr) sends to Core 1 (MMDVMHost)
sendto(sock, data, len, 0, (struct sockaddr *)&addr, sizeof(addr));
// addr = 127.0.0.1:5000
```

**Performance:**
- Latency: ~20-50 μs (kernel stack overhead)
- Bandwidth: ~100-500 MB/s (kernel copy overhead)
- Mature, well-tested

**Use Case:** Control messages, small packets

### 4.3 Named Pipes (FIFOs)

**Implementation:** POSIX FIFOs for unidirectional streams

```bash
mkfifo /tmp/mmdvm_tx
mkfifo /tmp/mmdvm_rx
```

**Performance:**
- Latency: ~10-30 μs
- Bandwidth: ~200-800 MB/s
- Blocking I/O by default

**Use Case:** Audio sample streaming (similar to current virtual PTY)

### 4.4 Virtual PTY (Current Method)

**Implementation:** Pseudo-terminal for serial emulation

**Performance:**
- Latency: ~50-100 μs (TTY layer overhead)
- Bandwidth: ~10-50 MB/s (limited by serial emulation)
- Compatible with existing MMDVMHost code

**Use Case:** Maintain compatibility, minimal code changes

### 4.5 Comparison Table

| Method | Latency | Bandwidth | CPU Overhead | Code Changes | Recommendation |
|--------|---------|-----------|--------------|--------------|----------------|
| **Shared Memory** | 1-5 μs | 1-2 GB/s | Very Low | Moderate | ⭐ Best for audio |
| **UDP Loopback** | 20-50 μs | 100-500 MB/s | Low | Minimal | ⭐ Best for control |
| **Named Pipes** | 10-30 μs | 200-800 MB/s | Low | Minimal | Alternative |
| **Virtual PTY** | 50-100 μs | 10-50 MB/s | Medium | None (current) | Fallback |

**Recommended Hybrid Approach:**
1. **Virtual PTY** for modem commands (AT-style, low bandwidth)
2. **Shared Memory** for audio samples (24 kHz stream, ~48 KB/s)
3. **UDP Loopback** for network events (optional, for future enhancements)

---

## 5. Linux Kernel Configuration

### 5.1 SMP Kernel Configuration

The PlutoSDR default kernel is SMP-enabled. Key configurations:

```
CONFIG_SMP=y
CONFIG_NR_CPUS=2
CONFIG_ARM_GIC=y
CONFIG_ARM_ARCH_TIMER=y
CONFIG_CPU_FREQ=y  (can be disabled for determinism)
CONFIG_CPU_IDLE=y  (can be disabled for low latency)
```

### 5.2 Real-Time Kernel (Optional Enhancement)

For improved determinism, apply **PREEMPT_RT** patches:

```
CONFIG_PREEMPT_RT=y
CONFIG_HIGH_RES_TIMERS=y
CONFIG_NO_HZ_FULL=y
```

**Benefits:**
- Reduced IRQ latency: 50-100 μs → 10-20 μs
- More deterministic scheduling
- Better for DMR timing (30 ms TDMA slots)

**Effort:** Moderate (kernel recompile, testing)

### 5.3 Kernel Boot Parameters

**For Optimal Dual-Core Performance:**

```bash
# /boot/cmdline.txt or bootargs in uEnv.txt
isolcpus=0,1           # Isolate both cores from general scheduling
nohz_full=0,1          # Disable periodic timer ticks on isolated cores
rcu_nocbs=0,1          # Offload RCU callbacks from isolated cores
irqaffinity=0          # Route hardware IRQs to Core 0 by default
maxcpus=2              # Enable both cores (PlutoSDR default may be 1)
```

**Effect:**
- Core 0 dedicated to mmdvm-sdr (no other tasks scheduled)
- Core 1 dedicated to MMDVMHost (no other tasks scheduled)
- Minimal scheduler interference
- Lower latency variance

### 5.4 CPU Affinity at Runtime

Use `taskset` to pin processes to specific cores:

```bash
# Pin mmdvm-sdr to Core 0
taskset -c 0 /usr/bin/mmdvm-sdr &

# Pin MMDVMHost to Core 1
taskset -c 1 /usr/bin/MMDVMHost /etc/MMDVM.ini &
```

Verify with:
```bash
ps -eo pid,psr,comm | grep -E "mmdvm|MMDVM"
#   PID  PSR  COMMAND
#  1234   0   mmdvm-sdr
#  1235   1   MMDVMHost
```

---

## 6. Power and Thermal Considerations

### 6.1 Power Consumption

| Configuration | Power Draw | Notes |
|--------------|------------|-------|
| **Single Core (667 MHz)** | ~0.8-1.0 W | Idle, no RF |
| **Dual Core (667 MHz)** | ~1.2-1.5 W | Both cores active |
| **With AD9363 RX** | +0.5 W | RF receive active |
| **With AD9363 TX** | +1.0 W | RF transmit active |
| **Total Hotspot Operation** | ~2.5-3.0 W | Dual-core + RX + TX |

**PlutoSDR Power Budget:**
- USB 2.0: 2.5 W (500 mA @ 5V) - **MARGINAL**
- USB 3.0: 4.5 W (900 mA @ 5V) - **SAFE**

**Note:** For reliable operation, use a powered USB 3.0 port or external 5V power supply.

### 6.2 Thermal Management

| Component | Typical Temp | Max Temp | Notes |
|-----------|--------------|----------|-------|
| **Zynq-7010** | 45-65°C | 85°C | With passive cooling |
| **AD9363** | 50-70°C | 85°C | Higher during TX |

**Recommendations:**
- PlutoSDR has a built-in heatsink (adequate for normal use)
- For 24/7 hotspot operation, consider:
  - Small fan (5V, 40mm)
  - Improved airflow
  - Temperature monitoring via IIO (`cat /sys/bus/iio/devices/iio:device*/in_temp0_input`)

---

## 7. Performance Projections

### 7.1 Single-Core vs Dual-Core Performance

**Current Single-Core Scenario:**
- mmdvm-sdr: ~30-35% CPU @ 667 MHz (with NEON optimizations)
- MMDVMHost: ~5-10% CPU @ 667 MHz
- System overhead: ~5-10%
- **Total: 40-55% of single core** ✅ Feasible on one core

**Dual-Core Scenario:**
- Core 0 (mmdvm-sdr): ~30-35% utilization
- Core 1 (MMDVMHost): ~5-10% utilization
- **Headroom: 60-70% on each core** ✅ Excellent margin

**Benefits of Dual-Core:**
1. **Isolation:** No scheduling contention between mmdvm-sdr and MMDVMHost
2. **Lower Latency:** Dedicated CPU reduces jitter
3. **Scalability:** Room for future features (web UI, monitoring)
4. **Reliability:** One task can't starve the other

### 7.2 Expected Latency Budget

**DMR TDMA Timing Constraint:** 30 ms per slot

| Path | Latency | Budget Used |
|------|---------|-------------|
| **RX: PlutoSDR → mmdvm-sdr** | 1-3 ms | 10% |
| **RX: mmdvm-sdr → MMDVMHost** | 0.1-0.5 ms | 2% |
| **MMDVMHost Processing** | 1-2 ms | 7% |
| **TX: MMDVMHost → mmdvm-sdr** | 0.1-0.5 ms | 2% |
| **TX: mmdvm-sdr → PlutoSDR** | 1-3 ms | 10% |
| **Total** | **3.3-9.0 ms** | **31%** |

**Margin:** 21 ms (69% headroom) ✅ Excellent for 30 ms TDMA slots

---

## 8. Challenges and Mitigations

### 8.1 Identified Challenges

| Challenge | Impact | Mitigation |
|-----------|--------|------------|
| **Cache Coherency Overhead** | Low | SCU handles automatically, minimal penalty |
| **Scheduler Jitter** | Medium | Use `isolcpus` and RT priority |
| **Inter-Core Synchronization** | Low | Lock-free ring buffers, minimize locks |
| **Power Budget (USB 2.0)** | Medium | Require USB 3.0 or external power |
| **Flash Storage Limit (32 MB)** | Medium | Strip binaries, use compressed rootfs |
| **Thermal in Enclosed Case** | Low | Monitor temps, add fan if needed |

### 8.2 Risk Assessment

| Risk | Probability | Severity | Mitigation Success |
|------|-------------|----------|-------------------|
| **Insufficient CPU** | Very Low | High | Already proven on single core |
| **Memory Overflow** | Low | Medium | 360 MB available >> 50 MB needed |
| **Timing Violations** | Low | High | 21 ms margin, use RT kernel |
| **Hardware Failure** | Low | High | PlutoSDR is proven platform |
| **Software Bugs** | Medium | Medium | Thorough testing, incremental rollout |

**Overall Risk:** **LOW** - Well-bounded problem with proven components.

---

## 9. Zynq-7010 vs Alternative Platforms

### 9.1 Comparison with Other ARM Platforms

| Platform | Cores | Clock | RAM | NEON | Score | Notes |
|----------|-------|-------|-----|------|-------|-------|
| **Zynq-7010** | 2x A9 | 667 MHz | 512 MB | Yes | 9/10 | ⭐ Ideal, dual-core, PL for RF |
| **RPi Zero 2 W** | 4x A53 | 1 GHz | 512 MB | Yes | 7/10 | No integrated RF, higher power |
| **RPi 3B+** | 4x A53 | 1.4 GHz | 1 GB | Yes | 8/10 | More powerful, but no RF |
| **RPi 4** | 4x A72 | 1.5 GHz | 2-8 GB | Yes | 9/10 | Overkill, expensive |

**Zynq-7010 Unique Advantages:**
1. Integrated RF (AD9363 + PL) - no external SDR needed
2. Dual-core sufficient for task
3. Lower power than RPi
4. Industrial-grade reliability
5. Programmable logic for future enhancements

---

## 10. Conclusion and Recommendations

### 10.1 Feasibility Assessment

✅ **HIGHLY FEASIBLE** - The Zynq-7010 dual-core architecture is well-suited for running mmdvm-sdr and MMDVMHost concurrently.

**Key Strengths:**
1. Dual-core ARM Cortex-A9 provides sufficient compute power (40-55% total utilization)
2. 512 MB RAM is adequate (360 MB available >> 50 MB needed)
3. Hardware cache coherency simplifies shared memory
4. Standard Linux SMP eliminates need for custom AMP kernel
5. Multiple IPC mechanisms available (shared memory, UDP, pipes, PTY)
6. Existing PlutoSDR software stack is mature and stable

**Key Constraints:**
1. USB 2.0 power budget tight (use USB 3.0 or external power)
2. Flash storage limited (32 MB, need stripped binaries)
3. Thermal management for 24/7 operation (add fan if needed)

### 10.2 Recommended Architecture

**Operating Mode:** Linux SMP (Symmetric Multiprocessing)

**Core Allocation:**
- **Core 0:** mmdvm-sdr (SDR modem, DSP-intensive)
- **Core 1:** MMDVMHost (protocol handling, networking)

**Communication:**
- Virtual PTY for command/control (backward compatible)
- Shared memory for audio samples (low latency, high bandwidth)
- UDP loopback for future enhancements (optional)

**Kernel Configuration:**
- Standard SMP kernel (no PREEMPT_RT initially)
- Boot parameters: `isolcpus=0,1 nohz_full=0,1 maxcpus=2`
- Runtime: `taskset -c 0 mmdvm-sdr`, `taskset -c 1 MMDVMHost`

**Power:**
- Require USB 3.0 port (900 mA) or external 5V/1A supply
- Monitor `iio:device*/in_temp0_input` for thermal issues

### 10.3 Next Steps

See **IMPLEMENTATION_ROADMAP.md** for detailed implementation plan.

---

## Appendix A: Technical Specifications Summary

### Zynq-7010 Quick Reference

```
CPU:        2x ARM Cortex-A9 @ 667 MHz (866 MHz max)
NEON:       Yes, 128-bit SIMD per core
L1 Cache:   32 KB I + 32 KB D per core
L2 Cache:   512 KB shared
OCM:        256 KB on-chip SRAM
RAM:        512 MB DDR3-1066 (PlutoSDR)
Flash:      32 MB Quad-SPI (PlutoSDR)
PL:         28K logic cells (Artix-7 class)
RF:         AD9363 (325 MHz - 3.8 GHz)
Power:      ~2.5-3.0 W (dual-core + RF)
Thermal:    45-65°C typical, 85°C max
```

### Key Linux Commands

```bash
# Enable second core (if disabled)
echo 1 > /sys/devices/system/cpu/cpu1/online

# Check core status
cat /proc/cpuinfo | grep processor

# Pin process to core
taskset -c 0 <command>  # Run on Core 0
taskset -c 1 <command>  # Run on Core 1

# Monitor per-core usage
top -d 1   # Press '1' to show per-core stats

# Check temperature
cat /sys/bus/iio/devices/iio:device*/in_temp0_input  # Zynq temp (millidegrees C)

# Check memory
free -h

# Monitor IRQ affinity
cat /proc/interrupts
```

---

**Document Version:** 1.0
**Last Updated:** 2025-11-16
**Author:** mmdvm-sdr Development Team
**License:** GNU GPL v2.0
