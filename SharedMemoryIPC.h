/*
 *   Copyright (C) 2024 by MMDVM-SDR Contributors
 *
 *   Shared Memory IPC for Dual-Core DMR Hotspot
 *   Lock-free ring buffer implementation for audio sample exchange
 *   Designed for Xilinx Zynq-7010 dual-core ARM Cortex-A9
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#if !defined(SHAREDMEMORYIPC_H)
#define SHAREDMEMORYIPC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ==================== Configuration Constants ====================

/**
 * Buffer size (must be power of 2 for efficient modulo)
 * 65536 samples = 131 KB @ 16-bit samples
 * At 24 kHz sample rate: ~2.73 seconds of audio
 */
#define SHMIPC_BUFFER_SIZE 65536U
#define SHMIPC_BUFFER_MASK (SHMIPC_BUFFER_SIZE - 1U)

/**
 * Cache line size for ARM Cortex-A9
 * Used for alignment to prevent false sharing
 */
#define CACHE_LINE_SIZE 32

/**
 * Shared memory name (POSIX shm_open)
 */
#define SHMIPC_NAME "/mmdvm_ipc"

// ==================== Aligned Ring Buffer Structure ====================

/**
 * Lock-free ring buffer for single-producer, single-consumer
 * Aligned to cache lines to prevent false sharing between cores
 */
struct __attribute__((aligned(CACHE_LINE_SIZE))) SharedMemoryRingBuffer {
    // Producer writes here (Core 0 for RX, Core 1 for TX)
    volatile uint32_t writeIndex;
    uint8_t _padding1[CACHE_LINE_SIZE - sizeof(uint32_t)];

    // Consumer reads here (Core 1 for RX, Core 0 for TX)
    volatile uint32_t readIndex;
    uint8_t _padding2[CACHE_LINE_SIZE - sizeof(uint32_t)];

    // Buffer capacity (constant, but stored for verification)
    uint32_t bufferSize;
    uint8_t _padding3[CACHE_LINE_SIZE - sizeof(uint32_t)];

    // Audio samples (Q15 format: int16_t)
    int16_t samples[SHMIPC_BUFFER_SIZE];
};

/**
 * Statistics structure for monitoring performance
 * Aligned to cache line
 */
struct __attribute__((aligned(CACHE_LINE_SIZE))) SharedMemoryStats {
    volatile uint32_t rxOverruns;    // RX buffer overflow count
    volatile uint32_t txUnderruns;   // TX buffer underflow count
    volatile uint32_t rxSamples;     // Total RX samples written
    volatile uint32_t txSamples;     // Total TX samples read
    uint8_t _padding[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];
};

/**
 * Status flags for bidirectional communication
 * Aligned to cache line
 */
struct __attribute__((aligned(CACHE_LINE_SIZE))) SharedMemoryStatus {
    volatile uint32_t mmdvmStatus;   // Core 0 (mmdvm-sdr) status
    volatile uint32_t hostStatus;    // Core 1 (MMDVMHost) status
    volatile bool mmdvmReady;        // Core 0 initialization complete
    volatile bool hostReady;         // Core 1 initialization complete
    uint8_t _padding[CACHE_LINE_SIZE - 2 * sizeof(uint32_t) - 2 * sizeof(bool)];
};

// ==================== Main Shared Memory Structure ====================

/**
 * Complete shared memory layout
 * Total size: ~264 KB (fits comfortably in DDR3 or OCM)
 */
struct __attribute__((aligned(CACHE_LINE_SIZE))) SharedMemoryRegion {
    // RX buffer: mmdvm-sdr writes, MMDVMHost reads
    struct SharedMemoryRingBuffer rxRing;

    // TX buffer: MMDVMHost writes, mmdvm-sdr reads
    struct SharedMemoryRingBuffer txRing;

    // Statistics
    struct SharedMemoryStats stats;

    // Status flags
    struct SharedMemoryStatus status;
};

// ==================== C++ Class Interface ====================

#ifdef __cplusplus

/**
 * SharedMemoryIPC class - High-level interface for shared memory IPC
 *
 * Features:
 * - Lock-free ring buffer operations
 * - Zero-copy sample transfer
 * - Cache-coherent on Zynq (hardware SCU)
 * - Thread-safe for single producer/consumer
 * - Overflow/underflow detection
 */
class CSharedMemoryIPC {
public:
    /**
     * Constructor
     * @param isCreator True if this process should create the shared memory (mmdvm-sdr)
     */
    CSharedMemoryIPC(bool isCreator);

    /**
     * Destructor - unmaps shared memory
     */
    ~CSharedMemoryIPC();

    // ==================== Initialization ====================

    /**
     * Initialize shared memory segment
     * @return true on success, false on error
     */
    bool init();

    /**
     * Check if shared memory is initialized
     * @return true if ready
     */
    bool isReady() const { return m_ready; }

    /**
     * Set this process as ready
     */
    void setReady();

    /**
     * Wait for other process to be ready
     * @param timeoutMs Timeout in milliseconds (0 = no timeout)
     * @return true if other process is ready
     */
    bool waitForOtherSide(uint32_t timeoutMs = 0);

    // ==================== RX Ring Buffer (mmdvm-sdr → MMDVMHost) ====================

    /**
     * Write samples to RX buffer (Producer: mmdvm-sdr)
     * @param samples Pointer to sample array
     * @param count Number of samples to write
     * @return Number of samples actually written
     */
    uint32_t writeRX(const int16_t* samples, uint32_t count);

    /**
     * Read samples from RX buffer (Consumer: MMDVMHost)
     * @param samples Pointer to output buffer
     * @param count Number of samples to read
     * @return Number of samples actually read
     */
    uint32_t readRX(int16_t* samples, uint32_t count);

    /**
     * Get number of samples available in RX buffer
     * @return Number of readable samples
     */
    uint32_t getRXAvailable() const;

    /**
     * Get free space in RX buffer
     * @return Number of samples that can be written
     */
    uint32_t getRXSpace() const;

    // ==================== TX Ring Buffer (MMDVMHost → mmdvm-sdr) ====================

    /**
     * Write samples to TX buffer (Producer: MMDVMHost)
     * @param samples Pointer to sample array
     * @param count Number of samples to write
     * @return Number of samples actually written
     */
    uint32_t writeTX(const int16_t* samples, uint32_t count);

    /**
     * Read samples from TX buffer (Consumer: mmdvm-sdr)
     * @param samples Pointer to output buffer
     * @param count Number of samples to read
     * @return Number of samples actually read
     */
    uint32_t readTX(int16_t* samples, uint32_t count);

    /**
     * Get number of samples available in TX buffer
     * @return Number of readable samples
     */
    uint32_t getTXAvailable() const;

    /**
     * Get free space in TX buffer
     * @return Number of samples that can be written
     */
    uint32_t getTXSpace() const;

    // ==================== Statistics ====================

    /**
     * Get RX overrun count
     * @return Number of times RX buffer overflowed
     */
    uint32_t getRXOverruns() const;

    /**
     * Get TX underrun count
     * @return Number of times TX buffer underflowed
     */
    uint32_t getTXUnderruns() const;

    /**
     * Get total RX samples written
     * @return Total sample count
     */
    uint32_t getRXSampleCount() const;

    /**
     * Get total TX samples read
     * @return Total sample count
     */
    uint32_t getTXSampleCount() const;

    /**
     * Reset statistics counters
     */
    void resetStats();

    /**
     * Print statistics to console
     */
    void printStats() const;

private:
    // Shared memory control
    int m_fd;                           // File descriptor
    struct SharedMemoryRegion* m_shm;   // Mapped memory pointer
    bool m_isCreator;                   // Creator flag
    bool m_ready;                       // Initialization flag

    // Helper functions
    uint32_t writeRingBuffer(struct SharedMemoryRingBuffer* ring,
                             const int16_t* samples,
                             uint32_t count,
                             volatile uint32_t* overrunCounter);

    uint32_t readRingBuffer(struct SharedMemoryRingBuffer* ring,
                           int16_t* samples,
                           uint32_t count,
                           volatile uint32_t* underrunCounter);

    uint32_t getRingAvailable(const struct SharedMemoryRingBuffer* ring) const;
    uint32_t getRingSpace(const struct SharedMemoryRingBuffer* ring) const;
};

#endif // __cplusplus

// ==================== C Interface (Optional) ====================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * C-compatible functions for shared memory access
 */
typedef struct SharedMemoryRegion* shmipc_handle_t;

/**
 * Create/open shared memory
 * @param isCreator True to create new segment
 * @return Handle or NULL on error
 */
shmipc_handle_t shmipc_init(bool isCreator);

/**
 * Close shared memory
 * @param handle Shared memory handle
 */
void shmipc_close(shmipc_handle_t handle);

/**
 * Write to RX ring buffer
 */
uint32_t shmipc_write_rx(shmipc_handle_t handle, const int16_t* samples, uint32_t count);

/**
 * Read from RX ring buffer
 */
uint32_t shmipc_read_rx(shmipc_handle_t handle, int16_t* samples, uint32_t count);

/**
 * Write to TX ring buffer
 */
uint32_t shmipc_write_tx(shmipc_handle_t handle, const int16_t* samples, uint32_t count);

/**
 * Read from TX ring buffer
 */
uint32_t shmipc_read_tx(shmipc_handle_t handle, int16_t* samples, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif // SHAREDMEMORYIPC_H
