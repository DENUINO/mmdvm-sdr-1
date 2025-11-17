/*
 *   Copyright (C) 2024 by MMDVM-SDR Contributors
 *
 *   Shared Memory IPC Implementation
 *   Lock-free ring buffer for inter-core audio sample exchange
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "SharedMemoryIPC.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <time.h>

// ==================== Constructor / Destructor ====================

CSharedMemoryIPC::CSharedMemoryIPC(bool isCreator) :
    m_fd(-1),
    m_shm(nullptr),
    m_isCreator(isCreator),
    m_ready(false)
{
}

CSharedMemoryIPC::~CSharedMemoryIPC()
{
    if (m_shm != nullptr) {
        munmap(m_shm, sizeof(struct SharedMemoryRegion));
        m_shm = nullptr;
    }

    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }

    // Only creator unlinks the shared memory
    if (m_isCreator) {
        shm_unlink(SHMIPC_NAME);
    }
}

// ==================== Initialization ====================

bool CSharedMemoryIPC::init()
{
    if (m_ready) {
        return true;  // Already initialized
    }

    // Create or open shared memory
    if (m_isCreator) {
        // Remove any stale shared memory from previous run
        shm_unlink(SHMIPC_NAME);

        // Create new segment (O_EXCL ensures it's fresh after unlink)
        m_fd = shm_open(SHMIPC_NAME, O_RDWR | O_CREAT | O_EXCL, 0666);
        if (m_fd < 0) {
            fprintf(stderr, "SharedMemoryIPC: shm_open (create) failed: %s\n", strerror(errno));
            return false;
        }
    } else {
        // Open existing segment
        m_fd = shm_open(SHMIPC_NAME, O_RDWR, 0666);
        if (m_fd < 0) {
            fprintf(stderr, "SharedMemoryIPC: shm_open (open) failed: %s\n", strerror(errno));
            return false;
        }
    }

    // Set size (only creator needs to do this)
    if (m_isCreator) {
        if (ftruncate(m_fd, sizeof(struct SharedMemoryRegion)) < 0) {
            fprintf(stderr, "SharedMemoryIPC: ftruncate failed: %s\n", strerror(errno));
            close(m_fd);
            m_fd = -1;
            return false;
        }
    }

    // Map shared memory into process address space
    m_shm = (struct SharedMemoryRegion*)mmap(
        nullptr,                            // Let kernel choose address
        sizeof(struct SharedMemoryRegion),  // Size
        PROT_READ | PROT_WRITE,             // Read/write access
        MAP_SHARED,                         // Shared between processes
        m_fd,                               // File descriptor
        0                                   // Offset
    );

    if (m_shm == MAP_FAILED) {
        fprintf(stderr, "SharedMemoryIPC: mmap failed: %s\n", strerror(errno));
        close(m_fd);
        m_fd = -1;
        m_shm = nullptr;
        return false;
    }

    // Initialize ring buffers (only creator)
    if (m_isCreator) {
        // Zero out entire structure
        memset(m_shm, 0, sizeof(struct SharedMemoryRegion));

        // Initialize RX ring
        __atomic_store_n(&m_shm->rxRing.writeIndex, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_shm->rxRing.readIndex, 0, __ATOMIC_RELEASE);
        m_shm->rxRing.bufferSize = SHMIPC_BUFFER_SIZE;

        // Initialize TX ring
        __atomic_store_n(&m_shm->txRing.writeIndex, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_shm->txRing.readIndex, 0, __ATOMIC_RELEASE);
        m_shm->txRing.bufferSize = SHMIPC_BUFFER_SIZE;

        // Initialize stats
        __atomic_store_n(&m_shm->stats.rxOverruns, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_shm->stats.txUnderruns, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_shm->stats.rxSamples, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_shm->stats.txSamples, 0, __ATOMIC_RELEASE);

        // Initialize status
        __atomic_store_n(&m_shm->status.mmdvmStatus, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_shm->status.hostStatus, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_shm->status.mmdvmReady, false, __ATOMIC_RELEASE);
        __atomic_store_n(&m_shm->status.hostReady, false, __ATOMIC_RELEASE);

        printf("SharedMemoryIPC: Created shared memory segment (%zu bytes)\n",
               sizeof(struct SharedMemoryRegion));
    } else {
        printf("SharedMemoryIPC: Opened existing shared memory segment\n");
    }

    m_ready = true;
    return true;
}

void CSharedMemoryIPC::setReady()
{
    if (!m_ready || m_shm == nullptr) {
        return;
    }

    if (m_isCreator) {
        __atomic_store_n(&m_shm->status.mmdvmReady, true, __ATOMIC_RELEASE);
        printf("SharedMemoryIPC: mmdvm-sdr ready\n");
    } else {
        __atomic_store_n(&m_shm->status.hostReady, true, __ATOMIC_RELEASE);
        printf("SharedMemoryIPC: MMDVMHost ready\n");
    }
}

bool CSharedMemoryIPC::waitForOtherSide(uint32_t timeoutMs)
{
    if (!m_ready || m_shm == nullptr) {
        return false;
    }

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (true) {
        bool otherReady;

        if (m_isCreator) {
            otherReady = __atomic_load_n(&m_shm->status.hostReady, __ATOMIC_ACQUIRE);
        } else {
            otherReady = __atomic_load_n(&m_shm->status.mmdvmReady, __ATOMIC_ACQUIRE);
        }

        if (otherReady) {
            printf("SharedMemoryIPC: Other side is ready\n");
            return true;
        }

        // Check timeout
        if (timeoutMs > 0) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            uint32_t elapsedMs = (now.tv_sec - start.tv_sec) * 1000 +
                                (now.tv_nsec - start.tv_nsec) / 1000000;
            if (elapsedMs >= timeoutMs) {
                fprintf(stderr, "SharedMemoryIPC: Timeout waiting for other side\n");
                return false;
            }
        }

        // Sleep 10ms before checking again
        usleep(10000);
    }
}

// ==================== Ring Buffer Helper Functions ====================

uint32_t CSharedMemoryIPC::writeRingBuffer(
    struct SharedMemoryRingBuffer* ring,
    const int16_t* samples,
    uint32_t count,
    volatile uint32_t* overrunCounter)
{
    if (samples == nullptr || count == 0) {
        return 0;
    }

    // Load current indices with acquire semantics
    uint32_t writeIdx = __atomic_load_n(&ring->writeIndex, __ATOMIC_ACQUIRE);
    uint32_t readIdx = __atomic_load_n(&ring->readIndex, __ATOMIC_ACQUIRE);

    uint32_t written = 0;

    for (uint32_t i = 0; i < count; i++) {
        // Calculate next write position
        uint32_t nextWrite = (writeIdx + 1) & SHMIPC_BUFFER_MASK;

        // Check if buffer is full
        if (nextWrite == readIdx) {
            // Buffer full - increment overrun counter
            __atomic_fetch_add(overrunCounter, 1, __ATOMIC_RELAXED);
            break;
        }

        // Write sample (no atomic needed - single writer)
        ring->samples[writeIdx] = samples[i];

        // Update write index with release semantics
        __atomic_store_n(&ring->writeIndex, nextWrite, __ATOMIC_RELEASE);

        writeIdx = nextWrite;
        written++;
    }

    return written;
}

uint32_t CSharedMemoryIPC::readRingBuffer(
    struct SharedMemoryRingBuffer* ring,
    int16_t* samples,
    uint32_t count,
    volatile uint32_t* underrunCounter)
{
    if (samples == nullptr || count == 0) {
        return 0;
    }

    // Load current indices with acquire semantics
    uint32_t readIdx = __atomic_load_n(&ring->readIndex, __ATOMIC_ACQUIRE);
    uint32_t writeIdx = __atomic_load_n(&ring->writeIndex, __ATOMIC_ACQUIRE);

    uint32_t read = 0;

    for (uint32_t i = 0; i < count; i++) {
        // Check if buffer is empty
        if (readIdx == writeIdx) {
            // Buffer empty - increment underrun counter
            if (underrunCounter != nullptr && i > 0) {
                // Only count as underrun if we expected data
                __atomic_fetch_add(underrunCounter, 1, __ATOMIC_RELAXED);
            }
            break;
        }

        // Read sample (no atomic needed - single reader)
        samples[i] = ring->samples[readIdx];

        // Calculate next read position
        uint32_t nextRead = (readIdx + 1) & SHMIPC_BUFFER_MASK;

        // Update read index with release semantics
        __atomic_store_n(&ring->readIndex, nextRead, __ATOMIC_RELEASE);

        readIdx = nextRead;
        read++;
    }

    return read;
}

uint32_t CSharedMemoryIPC::getRingAvailable(const struct SharedMemoryRingBuffer* ring) const
{
    uint32_t writeIdx = __atomic_load_n(&ring->writeIndex, __ATOMIC_ACQUIRE);
    uint32_t readIdx = __atomic_load_n(&ring->readIndex, __ATOMIC_ACQUIRE);

    // Calculate available samples (handle wraparound)
    return (writeIdx - readIdx) & SHMIPC_BUFFER_MASK;
}

uint32_t CSharedMemoryIPC::getRingSpace(const struct SharedMemoryRingBuffer* ring) const
{
    uint32_t available = getRingAvailable(ring);

    // Maximum usable space is BUFFER_SIZE - 1 (to distinguish full from empty)
    return (SHMIPC_BUFFER_SIZE - 1) - available;
}

// ==================== RX Ring Buffer (mmdvm-sdr → MMDVMHost) ====================

uint32_t CSharedMemoryIPC::writeRX(const int16_t* samples, uint32_t count)
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    uint32_t written = writeRingBuffer(&m_shm->rxRing, samples, count,
                                       &m_shm->stats.rxOverruns);

    // Update sample counter
    if (written > 0) {
        __atomic_fetch_add(&m_shm->stats.rxSamples, written, __ATOMIC_RELAXED);
    }

    return written;
}

uint32_t CSharedMemoryIPC::readRX(int16_t* samples, uint32_t count)
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return readRingBuffer(&m_shm->rxRing, samples, count, nullptr);
}

uint32_t CSharedMemoryIPC::getRXAvailable() const
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return getRingAvailable(&m_shm->rxRing);
}

uint32_t CSharedMemoryIPC::getRXSpace() const
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return getRingSpace(&m_shm->rxRing);
}

// ==================== TX Ring Buffer (MMDVMHost → mmdvm-sdr) ====================

uint32_t CSharedMemoryIPC::writeTX(const int16_t* samples, uint32_t count)
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return writeRingBuffer(&m_shm->txRing, samples, count, nullptr);
}

uint32_t CSharedMemoryIPC::readTX(int16_t* samples, uint32_t count)
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    uint32_t read = readRingBuffer(&m_shm->txRing, samples, count,
                                   &m_shm->stats.txUnderruns);

    // Update sample counter
    if (read > 0) {
        __atomic_fetch_add(&m_shm->stats.txSamples, read, __ATOMIC_RELAXED);
    }

    return read;
}

uint32_t CSharedMemoryIPC::getTXAvailable() const
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return getRingAvailable(&m_shm->txRing);
}

uint32_t CSharedMemoryIPC::getTXSpace() const
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return getRingSpace(&m_shm->txRing);
}

// ==================== Statistics ====================

uint32_t CSharedMemoryIPC::getRXOverruns() const
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return __atomic_load_n(&m_shm->stats.rxOverruns, __ATOMIC_ACQUIRE);
}

uint32_t CSharedMemoryIPC::getTXUnderruns() const
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return __atomic_load_n(&m_shm->stats.txUnderruns, __ATOMIC_ACQUIRE);
}

uint32_t CSharedMemoryIPC::getRXSampleCount() const
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return __atomic_load_n(&m_shm->stats.rxSamples, __ATOMIC_ACQUIRE);
}

uint32_t CSharedMemoryIPC::getTXSampleCount() const
{
    if (!m_ready || m_shm == nullptr) {
        return 0;
    }

    return __atomic_load_n(&m_shm->stats.txSamples, __ATOMIC_ACQUIRE);
}

void CSharedMemoryIPC::resetStats()
{
    if (!m_ready || m_shm == nullptr) {
        return;
    }

    __atomic_store_n(&m_shm->stats.rxOverruns, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_shm->stats.txUnderruns, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_shm->stats.rxSamples, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_shm->stats.txSamples, 0, __ATOMIC_RELEASE);

    printf("SharedMemoryIPC: Statistics reset\n");
}

void CSharedMemoryIPC::printStats() const
{
    if (!m_ready || m_shm == nullptr) {
        printf("SharedMemoryIPC: Not initialized\n");
        return;
    }

    printf("========== Shared Memory IPC Statistics ==========\n");
    printf("RX Ring Buffer:\n");
    printf("  Available: %u samples\n", getRXAvailable());
    printf("  Free space: %u samples\n", getRXSpace());
    printf("  Total written: %u samples\n", getRXSampleCount());
    printf("  Overruns: %u\n", getRXOverruns());
    printf("\n");
    printf("TX Ring Buffer:\n");
    printf("  Available: %u samples\n", getTXAvailable());
    printf("  Free space: %u samples\n", getTXSpace());
    printf("  Total read: %u samples\n", getTXSampleCount());
    printf("  Underruns: %u\n", getTXUnderruns());
    printf("\n");
    printf("Status:\n");
    printf("  mmdvm-sdr ready: %s\n",
           __atomic_load_n(&m_shm->status.mmdvmReady, __ATOMIC_ACQUIRE) ? "YES" : "NO");
    printf("  MMDVMHost ready: %s\n",
           __atomic_load_n(&m_shm->status.hostReady, __ATOMIC_ACQUIRE) ? "YES" : "NO");
    printf("==================================================\n");
}

// ==================== C Interface ====================

extern "C" {

shmipc_handle_t shmipc_init(bool isCreator)
{
    CSharedMemoryIPC* ipc = new CSharedMemoryIPC(isCreator);
    if (!ipc->init()) {
        delete ipc;
        return nullptr;
    }

    ipc->setReady();
    return (shmipc_handle_t)ipc;
}

void shmipc_close(shmipc_handle_t handle)
{
    if (handle != nullptr) {
        CSharedMemoryIPC* ipc = (CSharedMemoryIPC*)handle;
        delete ipc;
    }
}

uint32_t shmipc_write_rx(shmipc_handle_t handle, const int16_t* samples, uint32_t count)
{
    if (handle == nullptr) {
        return 0;
    }
    CSharedMemoryIPC* ipc = (CSharedMemoryIPC*)handle;
    return ipc->writeRX(samples, count);
}

uint32_t shmipc_read_rx(shmipc_handle_t handle, int16_t* samples, uint32_t count)
{
    if (handle == nullptr) {
        return 0;
    }
    CSharedMemoryIPC* ipc = (CSharedMemoryIPC*)handle;
    return ipc->readRX(samples, count);
}

uint32_t shmipc_write_tx(shmipc_handle_t handle, const int16_t* samples, uint32_t count)
{
    if (handle == nullptr) {
        return 0;
    }
    CSharedMemoryIPC* ipc = (CSharedMemoryIPC*)handle;
    return ipc->writeTX(samples, count);
}

uint32_t shmipc_read_tx(shmipc_handle_t handle, int16_t* samples, uint32_t count)
{
    if (handle == nullptr) {
        return 0;
    }
    CSharedMemoryIPC* ipc = (CSharedMemoryIPC*)handle;
    return ipc->readTX(samples, count);
}

} // extern "C"
