# UDP Implementation Plan for MMDVM-SDR

**Document Version:** 1.0
**Date:** 2025-11-16
**Estimated Effort:** 2-3 days development + 1 day testing
**Complexity:** Medium
**Risk Level:** Low (non-breaking change)

---

## Executive Summary

This document provides a detailed, step-by-step implementation plan for adding UDP modem support to mmdvm-sdr. The implementation follows MMDVMHost's proven UDPController pattern and can be completed incrementally without disrupting existing PTY functionality.

**Implementation Strategy:**
- ✅ Additive approach (no removal of existing PTY code)
- ✅ Compile-time or runtime selection between PTY and UDP
- ✅ Minimal changes to existing codebase
- ✅ Backward compatible (PTY still works)
- ✅ Easy to test incrementally

**Deliverables:**
1. UDPSocket class (network primitives)
2. UDPModemPort class (MMDVM transport)
3. Configuration updates
4. Build system integration
5. Documentation and examples
6. Test suite

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Phase 1: UDP Socket Layer](#phase-1-udp-socket-layer)
3. [Phase 2: UDP Modem Port](#phase-2-udp-modem-port)
4. [Phase 3: Integration](#phase-3-integration)
5. [Phase 4: Configuration](#phase-4-configuration)
6. [Phase 5: Testing](#phase-5-testing)
7. [Phase 6: Documentation](#phase-6-documentation)
8. [Phase 7: Deployment](#phase-7-deployment)
9. [Timeline & Milestones](#timeline--milestones)
10. [Risk Mitigation](#risk-mitigation)

---

## Prerequisites

### Development Environment

**Required Tools:**
```bash
# Compiler and build tools
sudo apt-get install build-essential cmake git

# Development libraries
sudo apt-get install libpthread-stubs0-dev

# Optional: for testing
sudo apt-get install netcat tcpdump wireshark

# Optional: for documentation
sudo apt-get install doxygen graphviz
```

**Repository Setup:**
```bash
# Clone or update repository
cd /home/user/mmdvm-sdr
git status
git pull

# Create feature branch
git checkout -b feature/udp-modem

# Verify current build works
mkdir -p build
cd build
cmake ..
make
./mmdvm --version  # or whatever test confirms it runs
```

### Knowledge Requirements

**Developer Should Understand:**
1. C++ programming (classes, inheritance, virtual functions)
2. POSIX sockets API (socket, bind, sendto, recvfrom)
3. UDP protocol basics
4. mmdvm-sdr codebase structure
5. MMDVM protocol (frame format, commands)

**Recommended Reading:**
- mmdvm-sdr/ANALYSIS.md (if exists)
- MMDVMHost UDPController.cpp/h
- POSIX socket programming tutorials
- MMDVM protocol specification

### Testing Environment

**Minimum Test Setup:**
```
┌────────────────────────────────┐
│ Single Computer (Localhost)    │
│                                │
│  MMDVM-SDR ◄─UDP─► MMDVMHost  │
│  127.0.0.1:3334    127.0.0.1:3335│
└────────────────────────────────┘
```

**Ideal Test Setup:**
```
┌──────────────┐         ┌──────────────┐
│ Dev Machine  │   LAN   │ Test Pi/PC   │
│ MMDVMHost    │◄───────►│ MMDVM-SDR    │
│ 192.168.1.10 │         │ 192.168.1.100│
└──────────────┘         └──────────────┘
```

---

## Phase 1: UDP Socket Layer

**Goal:** Create low-level UDP socket abstraction
**Duration:** 4-6 hours
**Complexity:** Low
**Dependencies:** None

### Step 1.1: Create UDPSocket.h

**Location:** `/home/user/mmdvm-sdr/UDPSocket.h`

**Content:**
```cpp
/*
 * Copyright (C) 2025 MMDVM-SDR Project
 * Based on MMDVMHost UDPSocket by Jonathan Naylor G4KLX
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef UDPSOCKET_H
#define UDPSOCKET_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * @brief Low-level UDP socket wrapper
 *
 * Provides a simple abstraction over POSIX UDP sockets for
 * MMDVM modem communication. Handles address resolution,
 * binding, and send/receive operations.
 */
class CUDPSocket {
public:
    /**
     * @brief Constructor
     * @param address Local IP address to bind to
     * @param port Local UDP port to bind to
     */
    CUDPSocket(const std::string& address, unsigned int port);

    /**
     * @brief Destructor
     */
    ~CUDPSocket();

    /**
     * @brief Open and bind the UDP socket
     * @return true on success, false on failure
     */
    bool open();

    /**
     * @brief Close the UDP socket
     */
    void close();

    /**
     * @brief Read data from socket (non-blocking)
     * @param buffer Buffer to store received data
     * @param length Maximum bytes to read
     * @param addr Source address of received packet (output)
     * @param addrLen Length of source address (output)
     * @return Number of bytes read, 0 if no data, -1 on error
     */
    int read(unsigned char* buffer, unsigned int length,
             sockaddr_storage& addr, unsigned int& addrLen);

    /**
     * @brief Write data to socket
     * @param buffer Data to send
     * @param length Number of bytes to send
     * @param addr Destination address
     * @param addrLen Length of destination address
     * @return Number of bytes sent, -1 on error
     */
    int write(const unsigned char* buffer, unsigned int length,
              const sockaddr_storage& addr, unsigned int addrLen);

    /**
     * @brief Resolve hostname/IP to socket address
     * @param hostname Hostname or IP address
     * @param port Port number
     * @param addr Resolved address (output)
     * @param addrLen Length of resolved address (output)
     * @return true on success, false on failure
     */
    static bool lookup(const std::string& hostname, unsigned int port,
                      sockaddr_storage& addr, unsigned int& addrLen);

    /**
     * @brief Compare two socket addresses for equality
     * @param addr1 First address
     * @param addr2 Second address
     * @return true if addresses match, false otherwise
     */
    static bool match(const sockaddr_storage& addr1,
                     const sockaddr_storage& addr2);

private:
    std::string  m_address;   ///< Local bind address
    unsigned int m_port;      ///< Local bind port
    int          m_fd;        ///< Socket file descriptor
};

#endif // UDPSOCKET_H
```

**Verification:**
```bash
# Check syntax
g++ -c -std=c++11 -I. UDPSocket.h -o /dev/null
# Should compile without errors
```

---

### Step 1.2: Create UDPSocket.cpp

**Location:** `/home/user/mmdvm-sdr/UDPSocket.cpp`

**Content:**
```cpp
/*
 * Copyright (C) 2025 MMDVM-SDR Project
 * Based on MMDVMHost UDPSocket by Jonathan Naylor G4KLX
 */

#include "UDPSocket.h"
#include "Log.h"

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>

CUDPSocket::CUDPSocket(const std::string& address, unsigned int port) :
    m_address(address),
    m_port(port),
    m_fd(-1)
{
}

CUDPSocket::~CUDPSocket()
{
    close();
}

bool CUDPSocket::open()
{
    // Create UDP socket
    m_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_fd < 0) {
        LogError("Cannot create UDP socket, errno=%d", errno);
        return false;
    }

    // Set socket to non-blocking mode
    int flags = fcntl(m_fd, F_GETFL, 0);
    if (flags < 0) {
        LogError("Cannot get socket flags, errno=%d", errno);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    if (fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LogError("Cannot set socket non-blocking, errno=%d", errno);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // Set socket to reuse address (allow quick restart)
    int opt = 1;
    if (setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        LogError("Cannot set socket options, errno=%d", errno);
        // Non-fatal, continue
    }

    // Bind to local address and port
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);

    if (m_address.empty() || m_address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, m_address.c_str(), &addr.sin_addr) != 1) {
            LogError("Invalid IP address: %s", m_address.c_str());
            ::close(m_fd);
            m_fd = -1;
            return false;
        }
    }

    if (bind(m_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        LogError("Cannot bind UDP socket to %s:%u, errno=%d",
                 m_address.c_str(), m_port, errno);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    LogMessage("UDP socket opened on %s:%u",
               m_address.c_str(), m_port);
    return true;
}

void CUDPSocket::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
        LogMessage("UDP socket closed");
    }
}

int CUDPSocket::read(unsigned char* buffer, unsigned int length,
                     sockaddr_storage& addr, unsigned int& addrLen)
{
    if (m_fd < 0)
        return -1;

    addrLen = sizeof(sockaddr_storage);
    socklen_t sockLen = addrLen;

    int ret = recvfrom(m_fd, buffer, length, 0,
                       (sockaddr*)&addr, &sockLen);

    if (ret < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LogError("UDP recvfrom error, errno=%d", errno);
            return -1;
        }
        return 0;  // No data available (non-blocking)
    }

    addrLen = sockLen;
    return ret;
}

int CUDPSocket::write(const unsigned char* buffer, unsigned int length,
                      const sockaddr_storage& addr, unsigned int addrLen)
{
    if (m_fd < 0)
        return -1;

    int ret = sendto(m_fd, buffer, length, 0,
                     (const sockaddr*)&addr, addrLen);

    if (ret < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LogError("UDP sendto error, errno=%d", errno);
            return -1;
        }
        return 0;  // Would block (rare for UDP)
    }

    return ret;
}

bool CUDPSocket::lookup(const std::string& hostname, unsigned int port,
                        sockaddr_storage& addr, unsigned int& addrLen)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4 only for now
    hints.ai_socktype = SOCK_DGRAM;   // UDP
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* result;
    int ret = getaddrinfo(hostname.c_str(), NULL, &hints, &result);
    if (ret != 0) {
        LogError("Cannot resolve hostname '%s': %s",
                 hostname.c_str(), gai_strerror(ret));
        return false;
    }

    // Copy first result
    memcpy(&addr, result->ai_addr, result->ai_addrlen);
    addrLen = result->ai_addrlen;

    // Set port (getaddrinfo doesn't handle port)
    ((sockaddr_in*)&addr)->sin_port = htons(port);

    freeaddrinfo(result);

    LogMessage("Resolved %s to %s:%u", hostname.c_str(),
               inet_ntoa(((sockaddr_in*)&addr)->sin_addr), port);

    return true;
}

bool CUDPSocket::match(const sockaddr_storage& addr1,
                       const sockaddr_storage& addr2)
{
    if (addr1.ss_family != addr2.ss_family)
        return false;

    if (addr1.ss_family == AF_INET) {
        const sockaddr_in* in1 = (const sockaddr_in*)&addr1;
        const sockaddr_in* in2 = (const sockaddr_in*)&addr2;

        return (in1->sin_addr.s_addr == in2->sin_addr.s_addr) &&
               (in1->sin_port == in2->sin_port);
    }

    // IPv6 not implemented
    return false;
}
```

**Verification:**
```bash
# Compile
g++ -c -std=c++11 -I. UDPSocket.cpp -o UDPSocket.o

# Check for undefined symbols
nm UDPSocket.o | grep " U "
# Should show: LogError, LogMessage (expected)
```

---

### Step 1.3: Create Unit Test for UDP Socket

**Location:** `/home/user/mmdvm-sdr/tests/test_udpsocket.cpp`

**Content:**
```cpp
/*
 * Unit tests for CUDPSocket class
 */

#include "../UDPSocket.h"
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <iostream>

// Minimal Log stub for testing
void LogMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void LogError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void test_socket_create_bind() {
    std::cout << "Test: Socket create and bind...";

    CUDPSocket socket("127.0.0.1", 33340);
    assert(socket.open() == true);
    socket.close();

    std::cout << " PASS" << std::endl;
}

void test_socket_lookup() {
    std::cout << "Test: Address lookup...";

    sockaddr_storage addr;
    unsigned int addrLen;

    // Test IP address
    assert(CUDPSocket::lookup("127.0.0.1", 3334, addr, addrLen) == true);

    // Test hostname
    assert(CUDPSocket::lookup("localhost", 3334, addr, addrLen) == true);

    // Test invalid
    assert(CUDPSocket::lookup("invalid.invalid.invalid", 3334,
                              addr, addrLen) == false);

    std::cout << " PASS" << std::endl;
}

void test_socket_match() {
    std::cout << "Test: Address matching...";

    sockaddr_storage addr1, addr2;
    unsigned int addrLen;

    CUDPSocket::lookup("127.0.0.1", 3334, addr1, addrLen);
    CUDPSocket::lookup("127.0.0.1", 3334, addr2, addrLen);

    // Same address and port should match
    assert(CUDPSocket::match(addr1, addr2) == true);

    // Different port should not match
    CUDPSocket::lookup("127.0.0.1", 3335, addr2, addrLen);
    assert(CUDPSocket::match(addr1, addr2) == false);

    std::cout << " PASS" << std::endl;
}

void test_loopback_send_receive() {
    std::cout << "Test: Loopback send/receive...";

    CUDPSocket sender("127.0.0.1", 33341);
    CUDPSocket receiver("127.0.0.1", 33342);

    assert(sender.open() == true);
    assert(receiver.open() == true);

    // Resolve receiver address
    sockaddr_storage recvAddr;
    unsigned int recvAddrLen;
    assert(CUDPSocket::lookup("127.0.0.1", 33342,
                              recvAddr, recvAddrLen) == true);

    // Send data
    unsigned char txData[] = {0xE0, 0x04, 0x00, 0x01};
    int sent = sender.write(txData, 4, recvAddr, recvAddrLen);
    assert(sent == 4);

    // Small delay for packet delivery
    usleep(10000);

    // Receive data
    unsigned char rxData[10];
    sockaddr_storage srcAddr;
    unsigned int srcAddrLen;
    int received = receiver.read(rxData, 10, srcAddr, srcAddrLen);

    assert(received == 4);
    assert(memcmp(rxData, txData, 4) == 0);

    sender.close();
    receiver.close();

    std::cout << " PASS" << std::endl;
}

int main() {
    std::cout << "=== UDP Socket Unit Tests ===" << std::endl;

    test_socket_create_bind();
    test_socket_lookup();
    test_socket_match();
    test_loopback_send_receive();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
```

**Verification:**
```bash
# Compile test
cd /home/user/mmdvm-sdr
g++ -std=c++11 -I. tests/test_udpsocket.cpp UDPSocket.cpp \
    -o tests/test_udpsocket

# Run test
./tests/test_udpsocket

# Expected output:
# === UDP Socket Unit Tests ===
# Test: Socket create and bind... PASS
# Test: Address lookup... PASS
# Test: Address matching... PASS
# Test: Loopback send/receive... PASS
#
# All tests passed!
```

**Milestone 1.1 Complete:** UDP socket layer working and tested

---

## Phase 2: UDP Modem Port

**Goal:** Create MMDVM transport layer using UDP
**Duration:** 6-8 hours
**Complexity:** Medium
**Dependencies:** Phase 1 complete

### Step 2.1: Create UDPModemPort.h

**Location:** `/home/user/mmdvm-sdr/UDPModemPort.h`

**Content:**
```cpp
/*
 * Copyright (C) 2025 MMDVM-SDR Project
 * Based on MMDVMHost UDPController by Jonathan Naylor G4KLX
 */

#ifndef UDPMODEMPORT_H
#define UDPMODEMPORT_H

#include "ISerialPort.h"
#include "UDPSocket.h"
#include <string>
#include <vector>

/**
 * @brief UDP-based MMDVM modem transport
 *
 * Implements ISerialPort interface using UDP network sockets.
 * Compatible with MMDVMHost's UDP modem protocol.
 *
 * Example usage:
 *   CUDPModemPort port("192.168.1.10", 3335, "192.168.1.100", 3334);
 *   port.open();
 *   port.write(data, length);
 *   port.read(buffer, length);
 *   port.close();
 */
class CUDPModemPort : public ISerialPort {
public:
    /**
     * @brief Constructor
     * @param modemAddress IP address of MMDVMHost (remote)
     * @param modemPort UDP port of MMDVMHost (remote)
     * @param localAddress IP address to bind to (local)
     * @param localPort UDP port to bind to (local)
     */
    CUDPModemPort(const std::string& modemAddress,
                  unsigned int modemPort,
                  const std::string& localAddress,
                  unsigned int localPort);

    /**
     * @brief Destructor
     */
    virtual ~CUDPModemPort();

    /**
     * @brief Open UDP connection
     * @return true on success, false on failure
     */
    virtual bool open();

    /**
     * @brief Close UDP connection
     */
    virtual void close();

    /**
     * @brief Read data from modem
     * @param buffer Buffer to store received data
     * @param length Maximum bytes to read
     * @return Number of bytes read, 0 if no data available
     */
    virtual int read(unsigned char* buffer, unsigned int length);

    /**
     * @brief Write data to modem
     * @param buffer Data to send
     * @param length Number of bytes to send
     * @return Number of bytes written
     */
    virtual int write(const unsigned char* buffer, unsigned int length);

private:
    CUDPSocket m_socket;               ///< UDP socket instance
    sockaddr_storage m_modemAddr;      ///< Remote modem address
    unsigned int m_modemAddrLen;       ///< Length of modem address

    // Ring buffer for received data
    std::vector<unsigned char> m_buffer;  ///< Receive buffer
    unsigned int m_bufferHead;            ///< Write position
    unsigned int m_bufferTail;            ///< Read position

    /**
     * @brief Add data to ring buffer
     * @param data Data to add
     * @param length Number of bytes
     */
    void addToBuffer(const unsigned char* data, unsigned int length);

    /**
     * @brief Get data from ring buffer
     * @param data Output buffer
     * @param length Maximum bytes to read
     * @return Number of bytes read
     */
    unsigned int getFromBuffer(unsigned char* data, unsigned int length);
};

#endif // UDPMODEMPORT_H
```

---

### Step 2.2: Create UDPModemPort.cpp

**Location:** `/home/user/mmdvm-sdr/UDPModemPort.cpp`

**Content:**
```cpp
/*
 * Copyright (C) 2025 MMDVM-SDR Project
 */

#include "UDPModemPort.h"
#include "Log.h"
#include <cstring>

// Ring buffer size (2KB, same as MMDVMHost)
#define BUFFER_SIZE 2000U

CUDPModemPort::CUDPModemPort(const std::string& modemAddress,
                             unsigned int modemPort,
                             const std::string& localAddress,
                             unsigned int localPort) :
    m_socket(localAddress, localPort),
    m_modemAddr(),
    m_modemAddrLen(0),
    m_buffer(BUFFER_SIZE),
    m_bufferHead(0),
    m_bufferTail(0)
{
    // Resolve modem address at construction time
    if (!CUDPSocket::lookup(modemAddress, modemPort,
                           m_modemAddr, m_modemAddrLen)) {
        LogError("Failed to resolve modem address: %s:%u",
                 modemAddress.c_str(), modemPort);
    } else {
        LogMessage("UDP modem target: %s:%u",
                   modemAddress.c_str(), modemPort);
    }
}

CUDPModemPort::~CUDPModemPort()
{
    close();
}

bool CUDPModemPort::open()
{
    // Verify modem address was resolved
    if (m_modemAddrLen == 0) {
        LogError("Cannot open UDP port: modem address not resolved");
        return false;
    }

    // Open and bind local socket
    if (!m_socket.open()) {
        LogError("Failed to open UDP socket");
        return false;
    }

    LogMessage("UDP modem port opened successfully");
    return true;
}

void CUDPModemPort::close()
{
    m_socket.close();

    // Clear ring buffer
    m_bufferHead = 0;
    m_bufferTail = 0;
}

int CUDPModemPort::read(unsigned char* buffer, unsigned int length)
{
    // First, try to drain buffered data
    unsigned int available = getFromBuffer(buffer, length);
    if (available > 0)
        return available;

    // No buffered data, try to receive new packet
    unsigned char tempBuffer[600];  // Max MMDVM frame size
    sockaddr_storage srcAddr;
    unsigned int srcAddrLen;

    int ret = m_socket.read(tempBuffer, 600, srcAddr, srcAddrLen);
    if (ret <= 0)
        return 0;  // No data or error

    // Validate source address (security feature)
    if (!CUDPSocket::match(srcAddr, m_modemAddr)) {
        LogWarning("Rejected UDP packet from unauthorized source");
        return 0;
    }

    // Add received data to ring buffer
    addToBuffer(tempBuffer, ret);

    // Return requested data from buffer
    return getFromBuffer(buffer, length);
}

int CUDPModemPort::write(const unsigned char* buffer, unsigned int length)
{
    // Write directly to modem (no TX buffering)
    return m_socket.write(buffer, length, m_modemAddr, m_modemAddrLen);
}

void CUDPModemPort::addToBuffer(const unsigned char* data,
                                unsigned int length)
{
    for (unsigned int i = 0; i < length; i++) {
        m_buffer[m_bufferHead] = data[i];
        m_bufferHead = (m_bufferHead + 1) % BUFFER_SIZE;

        // Buffer full - drop oldest data
        if (m_bufferHead == m_bufferTail) {
            m_bufferTail = (m_bufferTail + 1) % BUFFER_SIZE;
            LogWarning("UDP buffer overflow, dropping oldest data");
        }
    }
}

unsigned int CUDPModemPort::getFromBuffer(unsigned char* data,
                                          unsigned int length)
{
    unsigned int count = 0;

    while (count < length && m_bufferHead != m_bufferTail) {
        data[count++] = m_buffer[m_bufferTail];
        m_bufferTail = (m_bufferTail + 1) % BUFFER_SIZE;
    }

    return count;
}
```

**Verification:**
```bash
# Compile
g++ -c -std=c++11 -I. UDPModemPort.cpp -o UDPModemPort.o

# Check symbols
nm UDPModemPort.o | grep " T "
# Should show: read, write, open, close methods
```

---

### Step 2.3: Create Unit Test for UDP Modem Port

**Location:** `/home/user/mmdvm-sdr/tests/test_udpmodemport.cpp`

**Content:**
```cpp
/*
 * Unit tests for CUDPModemPort class
 */

#include "../UDPModemPort.h"
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <iostream>

// Log stubs
void LogMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void LogError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void LogWarning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void test_modem_port_create() {
    std::cout << "Test: Modem port creation...";

    CUDPModemPort port("127.0.0.1", 3335, "127.0.0.1", 3334);
    assert(port.open() == true);
    port.close();

    std::cout << " PASS" << std::endl;
}

void test_bidirectional_communication() {
    std::cout << "Test: Bidirectional communication...";

    // Create modem and host ports
    CUDPModemPort modem("127.0.0.1", 3335, "127.0.0.1", 3334);
    CUDPModemPort host("127.0.0.1", 3334, "127.0.0.1", 3335);

    assert(modem.open() == true);
    assert(host.open() == true);

    // Host sends GET_VERSION to modem
    unsigned char cmd[] = {0xE0, 0x03, 0x00};
    assert(host.write(cmd, 3) == 3);

    usleep(10000);

    // Modem receives command
    unsigned char rxBuf[10];
    int len = modem.read(rxBuf, 10);
    assert(len == 3);
    assert(memcmp(rxBuf, cmd, 3) == 0);

    // Modem sends ACK response
    unsigned char ack[] = {0xE0, 0x04, 0x70, 0x00};
    assert(modem.write(ack, 4) == 4);

    usleep(10000);

    // Host receives ACK
    len = host.read(rxBuf, 10);
    assert(len == 4);
    assert(memcmp(rxBuf, ack, 4) == 0);

    modem.close();
    host.close();

    std::cout << " PASS" << std::endl;
}

void test_ring_buffer_overflow() {
    std::cout << "Test: Ring buffer overflow handling...";

    CUDPModemPort receiver("127.0.0.1", 3337, "127.0.0.1", 3336);
    CUDPModemPort sender("127.0.0.1", 3336, "127.0.0.1", 3337);

    assert(receiver.open() == true);
    assert(sender.open() == true);

    // Send many packets quickly (more than buffer can hold)
    unsigned char data[100];
    memset(data, 0xAA, 100);

    for (int i = 0; i < 30; i++) {
        sender.write(data, 100);
    }

    usleep(50000);  // Let packets arrive

    // Try to read - should get data but possibly with drops
    unsigned char rxBuf[100];
    int totalRead = 0;

    for (int i = 0; i < 50; i++) {
        int len = receiver.read(rxBuf, 100);
        totalRead += len;
        if (len == 0)
            break;
        usleep(1000);
    }

    // Should have received some data (not all due to overflow)
    assert(totalRead > 0);
    std::cout << " (read " << totalRead << " bytes) PASS" << std::endl;

    receiver.close();
    sender.close();
}

void test_source_validation() {
    std::cout << "Test: Source address validation...";

    // Receiver expects packets from 127.0.0.1:3339
    CUDPModemPort receiver("127.0.0.1", 3339, "127.0.0.1", 3338);

    // Sender on DIFFERENT port (should be rejected)
    CUDPModemPort wrongSender("127.0.0.1", 3338, "127.0.0.1", 3340);

    assert(receiver.open() == true);
    assert(wrongSender.open() == true);

    // Send from wrong sender
    unsigned char data[] = {0xE0, 0x03, 0x00};
    wrongSender.write(data, 3);

    usleep(10000);

    // Receiver should reject (wrong source port)
    unsigned char rxBuf[10];
    int len = receiver.read(rxBuf, 10);
    assert(len == 0);  // Should be rejected

    std::cout << " PASS" << std::endl;

    receiver.close();
    wrongSender.close();
}

int main() {
    std::cout << "=== UDP Modem Port Unit Tests ===" << std::endl;

    test_modem_port_create();
    test_bidirectional_communication();
    test_ring_buffer_overflow();
    test_source_validation();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
```

**Verification:**
```bash
# Compile
g++ -std=c++11 -I. tests/test_udpmodemport.cpp \
    UDPModemPort.cpp UDPSocket.cpp \
    -o tests/test_udpmodemport

# Run
./tests/test_udpmodemport

# Expected: All tests pass
```

**Milestone 2.1 Complete:** UDP modem port working and tested

---

## Phase 3: Integration

**Goal:** Integrate UDP transport into mmdvm-sdr
**Duration:** 4-6 hours
**Complexity:** Medium
**Dependencies:** Phase 1 & 2 complete

### Step 3.1: Modify ISerialPort Interface (Optional)

**Note:** Only needed if ISerialPort doesn't already support polymorphism

**Location:** `/home/user/mmdvm-sdr/ISerialPort.h`

**Check Current Content:**
```bash
cat /home/user/mmdvm-sdr/ISerialPort.h
```

**If needed, modify to:**
```cpp
#ifndef ISERIALPORT_H
#define ISERIALPORT_H

/**
 * @brief Abstract modem port interface
 *
 * Allows different transport mechanisms (PTY, UDP, etc.)
 * to be used interchangeably.
 */
class ISerialPort {
public:
    virtual ~ISerialPort() {}

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual int read(unsigned char* buffer, unsigned int length) = 0;
    virtual int write(const unsigned char* buffer, unsigned int length) = 0;
};

#endif
```

---

### Step 3.2: Modify SerialPort.h to Support Port Injection

**Location:** `/home/user/mmdvm-sdr/SerialPort.h`

**Current Design:**
```cpp
// Find this section in SerialPort.h
private:
  // ...
  CSerialRB m_repeat;
#if defined(RPI)
  CSerialController m_controller;  // ← Hard-coded PTY controller
#endif
```

**Modified Design:**
```cpp
private:
  // ...
  CSerialRB m_repeat;
  ISerialPort* m_port;  // ← Polymorphic port (PTY or UDP)

public:
  // Add this method to header
  void setPort(ISerialPort* port);
```

**Verification:**
```bash
# Check syntax
g++ -c -std=c++11 -I. SerialPort.h -o /dev/null
```

---

### Step 3.3: Modify SerialPort.cpp to Use Injected Port

**Location:** `/home/user/mmdvm-sdr/SerialPort.cpp`

**Find Constructor:**
```cpp
CSerialPort::CSerialPort() :
m_buffer(),
m_ptr(0U),
m_len(0U),
m_debug(false),
m_repeat()
#if defined(RPI)
,m_controller()  // ← Remove this
#endif
{
}
```

**Modify to:**
```cpp
CSerialPort::CSerialPort() :
m_buffer(),
m_ptr(0U),
m_len(0U),
m_debug(false),
m_repeat(),
m_port(nullptr)  // ← Add this
{
}
```

**Add setPort() Method:**
```cpp
void CSerialPort::setPort(ISerialPort* port) {
    m_port = port;
}
```

**Find readInt/writeInt Methods:**
```cpp
// Current (example):
int availableInt(uint8_t n) {
#if defined(RPI)
    return m_controller.available();  // ← Hard-coded
#endif
}

uint8_t readInt(uint8_t n) {
#if defined(RPI)
    return m_controller.read();  // ← Hard-coded
#endif
}

void writeInt(uint8_t n, const uint8_t* data, uint16_t length, bool flush) {
#if defined(RPI)
    m_controller.write(data, length);  // ← Hard-coded
#endif
}
```

**Modify to use m_port:**
```cpp
int availableInt(uint8_t n) {
    // Not needed for UDP (non-blocking read returns 0 if no data)
    return 1;  // Always indicate data might be available
}

uint8_t readInt(uint8_t n) {
    if (m_port == nullptr)
        return 0;

    unsigned char byte;
    if (m_port->read(&byte, 1) == 1)
        return byte;

    return 0;
}

void writeInt(uint8_t n, const uint8_t* data, uint16_t length, bool flush) {
    if (m_port != nullptr)
        m_port->write(data, length);
}
```

**Verification:**
```bash
# Compile (will have undefined symbols, that's OK)
g++ -c -std=c++11 -I. SerialPort.cpp -o SerialPort.o
```

---

### Step 3.4: Modify MMDVM.cpp for Transport Selection

**Location:** `/home/user/mmdvm-sdr/MMDVM.cpp`

**Add includes at top:**
```cpp
#include "Config.h"
#include "Globals.h"
#include "Log.h"
#include "unistd.h"

// Transport layer includes
#ifdef USE_UDP_MODEM
  #include "UDPModemPort.h"
#else
  #include "SerialController.h"
#endif
```

**Modify setup() function:**
```cpp
void setup()
{
    LogDebug("MMDVM modem setup()");

    // Create appropriate transport
    ISerialPort* port = nullptr;

#ifdef USE_UDP_MODEM
    // UDP transport
    LogMessage("Using UDP modem transport");

    port = new CUDPModemPort(
        UDP_MODEM_ADDRESS,
        UDP_MODEM_PORT,
        UDP_LOCAL_ADDRESS,
        UDP_LOCAL_PORT
    );

    if (!port->open()) {
        LogError("Failed to open UDP modem port");
        delete port;
        return;
    }
#else
    // PTY transport (existing code)
    LogMessage("Using PTY transport");

#if defined(RPI)
    port = new CSerialController();

    if (!port->open()) {
        LogError("Failed to open PTY");
        delete port;
        return;
    }
#endif
#endif

    // Inject transport into serial port handler
    serial.setPort(port);

    // Rest of setup (unchanged)
    serial.start();
}
```

**Verification:**
```bash
# Syntax check
g++ -c -std=c++11 -DUSE_UDP_MODEM -I. MMDVM.cpp -o MMDVM.o
```

---

## Phase 4: Configuration

**Goal:** Add configuration system for UDP mode
**Duration:** 2-3 hours
**Complexity:** Low
**Dependencies:** Phase 3 complete

### Step 4.1: Add UDP Configuration to Config.h

**Location:** `/home/user/mmdvm-sdr/Config.h`

**Add UDP section:**
```cpp
// ============================================================================
// MODEM TRANSPORT CONFIGURATION
// ============================================================================

// Uncomment ONE of these options:

// Option 1: UDP Modem (Network-based)
#define USE_UDP_MODEM

// Option 2: PTY Modem (Virtual serial - default)
// (Leave USE_UDP_MODEM undefined)

// ----------------------------------------------------------------------------
// UDP Modem Configuration (when USE_UDP_MODEM is defined)
// ----------------------------------------------------------------------------
#ifdef USE_UDP_MODEM

// MMDVMHost connection (remote)
#ifndef UDP_MODEM_ADDRESS
  #define UDP_MODEM_ADDRESS   "127.0.0.1"  // MMDVMHost IP
#endif

#ifndef UDP_MODEM_PORT
  #define UDP_MODEM_PORT      3335         // MMDVMHost UDP port
#endif

// Local binding (this modem)
#ifndef UDP_LOCAL_ADDRESS
  #define UDP_LOCAL_ADDRESS   "127.0.0.1"  // Bind to localhost
#endif

#ifndef UDP_LOCAL_PORT
  #define UDP_LOCAL_PORT      3334         // Modem UDP port
#endif

#endif // USE_UDP_MODEM

// ----------------------------------------------------------------------------
// PTY Modem Configuration (when USE_UDP_MODEM is NOT defined)
// ----------------------------------------------------------------------------
#ifndef USE_UDP_MODEM

#ifndef SERIAL_PORT
  #define SERIAL_PORT "/dev/ptmx"
#endif

#ifndef SERIAL_SPEED
  #define SERIAL_SPEED SERIAL_115200
#endif

#endif // !USE_UDP_MODEM
```

**Example Configurations:**

**For Localhost Testing:**
```cpp
#define USE_UDP_MODEM
#define UDP_MODEM_ADDRESS   "127.0.0.1"
#define UDP_MODEM_PORT      3335
#define UDP_LOCAL_ADDRESS   "127.0.0.1"
#define UDP_LOCAL_PORT      3334
```

**For LAN Deployment:**
```cpp
#define USE_UDP_MODEM
#define UDP_MODEM_ADDRESS   "192.168.1.10"  // Server
#define UDP_MODEM_PORT      3335
#define UDP_LOCAL_ADDRESS   "192.168.1.100"  // This modem
#define UDP_LOCAL_PORT      3334
```

**For PTY Mode (traditional):**
```cpp
// #define USE_UDP_MODEM  ← Commented out = PTY mode
```

---

### Step 4.2: Update CMakeLists.txt

**Location:** `/home/user/mmdvm-sdr/CMakeLists.txt`

**Add UDP source files to build:**

**Find the source file list:**
```cmake
set(SOURCES
    MMDVM.cpp
    SerialPort.cpp
    SerialController.cpp
    # ... other files
)
```

**Add UDP files:**
```cmake
set(SOURCES
    MMDVM.cpp
    SerialPort.cpp

    # Transport layer
    $<$<BOOL:${USE_UDP}>:UDPSocket.cpp UDPModemPort.cpp>
    $<$<NOT:$<BOOL:${USE_UDP}>>:SerialController.cpp>

    # ... rest of files
)
```

**Add build option:**
```cmake
# Build Options
option(STANDALONE_MODE "Build standalone SDR mode" ON)
option(USE_NEON "Enable ARM NEON optimizations" OFF)
option(USE_TEXT_UI "Enable text UI" OFF)
option(USE_UDP "Use UDP modem transport" OFF)  # ← New option
option(PLUTO_SDR "Enable PlutoSDR support" OFF)
option(BUILD_TESTS "Build unit tests" OFF)
```

**Add conditional define:**
```cmake
# UDP modem support
if(USE_UDP)
    add_definitions(-DUSE_UDP_MODEM)
    message(STATUS "UDP modem transport: ENABLED")
else()
    message(STATUS "UDP modem transport: DISABLED (using PTY)")
endif()
```

**Update build summary:**
```cmake
message(STATUS "")
message(STATUS "Build Configuration Summary:")
message(STATUS "  Standalone mode:    ${STANDALONE_MODE}")
message(STATUS "  UDP transport:      ${USE_UDP}")  # ← Add
message(STATUS "  NEON optimization:  ${USE_NEON}")
message(STATUS "  Text UI:            ${USE_TEXT_UI}")
message(STATUS "  PlutoSDR support:   ${PLUTO_SDR}")
message(STATUS "  Unit tests:         ${BUILD_TESTS}")
message(STATUS "")
```

**Verification:**
```bash
cd /home/user/mmdvm-sdr/build

# Test UDP mode
cmake .. -DUSE_UDP=ON
make

# Test PTY mode
cmake .. -DUSE_UDP=OFF
make

# Test combined with standalone
cmake .. -DSTANDALONE_MODE=ON -DUSE_UDP=ON -DUSE_NEON=ON
make
```

---

## Phase 5: Testing

**Goal:** Comprehensive testing of UDP implementation
**Duration:** 6-8 hours
**Complexity:** Medium
**Dependencies:** All previous phases complete

### Step 5.1: Unit Tests (Already Done)

**Completed in Phases 1 & 2:**
- test_udpsocket.cpp
- test_udpmodemport.cpp

**Run all unit tests:**
```bash
cd /home/user/mmdvm-sdr
./tests/test_udpsocket
./tests/test_udpmodemport
```

---

### Step 5.2: Integration Test - Localhost

**Goal:** Test MMDVM-SDR with MMDVMHost on same machine

**Step 1: Build MMDVM-SDR in UDP mode**
```bash
cd /home/user/mmdvm-sdr/build
cmake .. -DUSE_UDP=ON
make
```

**Step 2: Configure MMDVMHost**

**MMDVM.ini:**
```ini
[Modem]
Protocol=udp
ModemAddress=127.0.0.1
ModemPort=3334
LocalAddress=127.0.0.1
LocalPort=3335
# ... rest of config
```

**Step 3: Start MMDVM-SDR**
```bash
cd /home/user/mmdvm-sdr/build
./mmdvm
```

**Expected output:**
```
MMDVM modem setup()
Using UDP modem transport
UDP modem target: 127.0.0.1:3335
UDP socket opened on 127.0.0.1:3334
UDP modem port opened successfully
```

**Step 4: Start MMDVMHost (in another terminal)**
```bash
cd /path/to/MMDVMHost
./MMDVMHost MMDVM.ini
```

**Expected output:**
```
MMDVMHost-20YYMMDD
...
Opening the modem...
MMDVM protocol version: 1
Description: MMDVM ...
Modem Ready
```

**Step 5: Monitor UDP traffic**
```bash
# In third terminal
sudo tcpdump -i lo -X 'udp port 3334 or udp port 3335'
```

**Expected:** See MMDVM protocol frames (starting with 0xE0)

**Success Criteria:**
- ✅ Both processes start without errors
- ✅ MMDVMHost shows "Modem Ready"
- ✅ tcpdump shows bidirectional UDP traffic
- ✅ MMDVM frames visible in packet capture

---

### Step 5.3: Integration Test - Network

**Goal:** Test across network (2 machines)

**Machine 1: MMDVM-SDR (Modem)**

**Config.h:**
```cpp
#define USE_UDP_MODEM
#define UDP_MODEM_ADDRESS   "192.168.1.10"   // Machine 2 IP
#define UDP_MODEM_PORT      3335
#define UDP_LOCAL_ADDRESS   "192.168.1.100"  // Machine 1 IP
#define UDP_LOCAL_PORT      3334
```

**Build and run:**
```bash
cd /home/user/mmdvm-sdr/build
cmake .. -DUSE_UDP=ON
make
./mmdvm
```

**Machine 2: MMDVMHost (Host)**

**MMDVM.ini:**
```ini
[Modem]
Protocol=udp
ModemAddress=192.168.1.100
ModemPort=3334
LocalAddress=192.168.1.10
LocalPort=3335
```

**Run:**
```bash
./MMDVMHost MMDVM.ini
```

**Network Tests:**

**Test 1: Connectivity**
```bash
# From Machine 1
ping 192.168.1.10

# From Machine 2
ping 192.168.1.100
```

**Test 2: UDP Port Open**
```bash
# On Machine 1
sudo netstat -tulpn | grep 3334
# Should show mmdvm process

# On Machine 2
sudo netstat -tulpn | grep 3335
# Should show MMDVMHost process
```

**Test 3: Firewall**
```bash
# On both machines, allow UDP
sudo ufw allow 3334/udp
sudo ufw allow 3335/udp
```

**Test 4: Packet Capture**
```bash
# On Machine 1
sudo tcpdump -i eth0 -X 'host 192.168.1.10 and (udp port 3334 or 3335)'

# Should see traffic to/from Machine 2
```

**Success Criteria:**
- ✅ Network connectivity verified
- ✅ UDP ports listening
- ✅ Firewall allows traffic
- ✅ Packets flowing bidirectionally
- ✅ MMDVMHost shows "Modem Ready"

---

### Step 5.4: Functional Tests

**Test 1: Version Query**

**Expected Flow:**
```
MMDVMHost → MMDVM-SDR: GET_VERSION (0xE0 0x03 0x00)
MMDVM-SDR → MMDVMHost: Version response
```

**Verify:**
```bash
# In tcpdump output, look for:
0xE0 0x03 0x00  # GET_VERSION command
0xE0 0x?? 0x00  # Version response (variable length)
```

**Test 2: Status Polling**

**Expected:** Status request every 250ms

**Verify:**
```bash
# Count status requests in 10 seconds
sudo timeout 10 tcpdump -i lo 'udp port 3334' 2>&1 | \
    grep -c "E0 03 01"

# Should be ~40 (10 sec / 0.25 sec = 40)
```

**Test 3: Configuration**

**Expected:**
```
MMDVMHost → MMDVM-SDR: SET_CONFIG (0xE0 0x10 0x02 ...)
MMDVM-SDR → MMDVMHost: ACK (0xE0 0x04 0x70 0x02)
```

**Verify:** MMDVMHost log shows configuration accepted

**Test 4: Mode Change**

**Expected:**
```
MMDVMHost → MMDVM-SDR: SET_MODE DMR (0xE0 0x04 0x03 0x01)
MMDVM-SDR → MMDVMHost: ACK (0xE0 0x04 0x70 0x03)
```

**Verify:** MMDVM-SDR log shows "Mode set to DMR"

---

### Step 5.5: Performance Tests

**Test 1: Latency**

**Script:** (See UDP_INTEGRATION.md Appendix C)
```bash
python3 test_latency.py
```

**Expected:**
- Average latency < 2ms (localhost)
- Average latency < 5ms (LAN)

**Test 2: Throughput**

**Script:** (See UDP_INTEGRATION.md Appendix C)
```bash
python3 test_throughput.py
```

**Expected:**
- Throughput > 1500 bytes/sec (DMR)
- No packet loss at sustained rate

**Test 3: Error Recovery**

**Kill MMDVMHost while MMDVM-SDR running:**
```bash
# While both running
pkill MMDVMHost

# Verify MMDVM-SDR still runs
ps aux | grep mmdvm

# Restart MMDVMHost
./MMDVMHost MMDVM.ini

# Verify reconnection successful
```

**Expected:**
- MMDVM-SDR survives host crash
- Reconnection works without restart

---

### Step 5.6: Regression Tests

**Goal:** Ensure PTY mode still works

**Build in PTY mode:**
```bash
cd /home/user/mmdvm-sdr/build
cmake .. -DUSE_UDP=OFF
make
./mmdvm
```

**Expected:**
- Compiles without errors
- Creates /dev/pts/X and ttyMMDVM0 symlink
- Works with MMDVMHost in UART mode

**Success Criteria:**
- ✅ PTY mode builds
- ✅ PTY mode runs
- ✅ No regressions in existing functionality

---

## Phase 6: Documentation

**Goal:** Document UDP feature for users and developers
**Duration:** 2-3 hours
**Complexity:** Low
**Dependencies:** Testing complete

### Step 6.1: Update README.md

**Location:** `/home/user/mmdvm-sdr/README.md`

**Add UDP section after existing PTY instructions:**

```markdown
## UDP Modem Mode (Alternative to Virtual PTY)

MMDVM-SDR now supports UDP-based modem communication, which eliminates the need for virtual PTY and **does not require** the MMDVMHost RTS patch.

### Advantages of UDP Mode

- ✅ No MMDVMHost code modification required
- ✅ Works with stock MMDVMHost
- ✅ Network-transparent (modem can be on different machine)
- ✅ Easy debugging with standard network tools
- ✅ Better error handling
- ✅ Simpler deployment

### Building with UDP Support

```bash
cd mmdvm-sdr
mkdir build && cd build

# Localhost mode (recommended for testing)
cmake .. -DUSE_UDP=ON
make

# Custom network configuration
# Edit Config.h first, then:
cmake .. -DUSE_UDP=ON
make
```

### MMDVMHost Configuration

In MMDVM.ini, change the [Modem] section:

```ini
[Modem]
Protocol=udp              # Change from 'uart' to 'udp'
ModemAddress=127.0.0.1    # MMDVM-SDR address
ModemPort=3334            # MMDVM-SDR port
LocalAddress=127.0.0.1    # MMDVMHost address
LocalPort=3335            # MMDVMHost port

# Rest of modem settings unchanged
TXInvert=1
RXInvert=1
# ...
```

**No RTS patch needed!**

### Running

```bash
# Terminal 1: Start MMDVM-SDR
cd mmdvm-sdr/build
./mmdvm

# Terminal 2: Start MMDVMHost
cd MMDVMHost
./MMDVMHost MMDVM.ini
```

### Network Deployment

For modem on different machine, edit `Config.h`:

```cpp
#define UDP_MODEM_ADDRESS   "192.168.1.10"   // MMDVMHost IP
#define UDP_MODEM_PORT      3335
#define UDP_LOCAL_ADDRESS   "192.168.1.100"  // This modem IP
#define UDP_LOCAL_PORT      3334
```

Rebuild and deploy.

### Troubleshooting

**"Failed to open UDP socket":**
- Check if port already in use: `sudo netstat -tulpn | grep 3334`
- Verify firewall allows UDP traffic
- Ensure address is valid

**"No response from modem":**
- Ping modem IP: `ping 192.168.1.100`
- Check firewall rules
- Verify MMDVMHost configuration matches modem

**Monitor traffic:**
```bash
sudo tcpdump -i any -X 'udp port 3334 or udp port 3335'
```

For detailed documentation, see:
- `MMDVMHOST_ANALYSIS.md` - MMDVMHost architecture and UDP feature
- `UDP_INTEGRATION.md` - Complete integration guide
- `UDP_IMPLEMENTATION_PLAN.md` - Development details
```

---

### Step 6.2: Create Quick Start Guide

**Location:** `/home/user/mmdvm-sdr/UDP_QUICKSTART.md`

```markdown
# UDP Modem Quick Start Guide

## 5-Minute Setup

### Step 1: Build MMDVM-SDR with UDP

```bash
cd mmdvm-sdr
mkdir -p build && cd build
cmake .. -DUSE_UDP=ON -DSTANDALONE_MODE=ON
make -j4
```

### Step 2: Configure MMDVMHost

Edit `MMDVM.ini`:
```ini
[Modem]
Protocol=udp
ModemAddress=127.0.0.1
ModemPort=3334
LocalAddress=127.0.0.1
LocalPort=3335
```

### Step 3: Run

```bash
# Terminal 1
cd mmdvm-sdr/build
./mmdvm

# Terminal 2
cd MMDVMHost
./MMDVMHost MMDVM.ini
```

### Step 4: Verify

Look for:
- MMDVM-SDR: "UDP modem port opened successfully"
- MMDVMHost: "Modem Ready"

Done! 🎉

## Network Deployment

**On modem device** (e.g., Raspberry Pi):
- Edit `Config.h`: Set `UDP_MODEM_ADDRESS` to server IP
- Build and run MMDVM-SDR

**On server** (MMDVMHost):
- Edit `MMDVM.ini`: Set `ModemAddress` to modem IP
- Run MMDVMHost

## Troubleshooting

**Problem:** Cannot bind socket
**Solution:** Check if port in use:
```bash
sudo lsof -i :3334
```

**Problem:** No communication
**Solution:** Test network:
```bash
nc -u modem_ip 3334  # Should connect
```

**Problem:** Firewall blocks
**Solution:** Allow UDP:
```bash
sudo ufw allow 3334/udp
sudo ufw allow 3335/udp
```

## Advanced

See full documentation in `UDP_INTEGRATION.md`.
```

---

### Step 6.3: Add Code Comments

**Add Doxygen-style comments to new files:**

**UDPSocket.h:** (Already done in Phase 1)

**UDPModemPort.h:** (Already done in Phase 2)

**Generate documentation:**
```bash
cd /home/user/mmdvm-sdr
doxygen -g Doxyfile

# Edit Doxyfile
# INPUT = . UDPSocket.h UDPModemPort.h
# EXTRACT_ALL = YES

doxygen Doxyfile

# Open docs
firefox html/index.html
```

---

## Phase 7: Deployment

**Goal:** Prepare for production use
**Duration:** 2-3 hours
**Complexity:** Low
**Dependencies:** All testing passed

### Step 7.1: Create Release Build

```bash
cd /home/user/mmdvm-sdr
mkdir -p release && cd release

# Optimized build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_UDP=ON \
  -DSTANDALONE_MODE=ON \
  -DUSE_NEON=ON \
  -DPLUTO_SDR=ON \
  -DBUILD_TESTS=OFF

make -j4

# Strip symbols (reduce size)
strip mmdvm

# Verify
./mmdvm --version
ldd mmdvm  # Check dependencies
```

---

### Step 7.2: Create Install Package

**Create install script:**

**install.sh:**
```bash
#!/bin/bash
# MMDVM-SDR UDP Install Script

set -e

echo "Installing MMDVM-SDR with UDP support..."

# Check dependencies
if ! command -v cmake &> /dev/null; then
    echo "ERROR: cmake not found. Install with: sudo apt-get install cmake"
    exit 1
fi

# Build
mkdir -p build && cd build
cmake .. -DUSE_UDP=ON -DSTANDALONE_MODE=ON -DUSE_NEON=ON -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install
sudo install -m 755 mmdvm /usr/local/bin/mmdvm-sdr
sudo mkdir -p /etc/mmdvm-sdr
sudo cp ../Config.h.example /etc/mmdvm-sdr/

echo ""
echo "Installation complete!"
echo ""
echo "Next steps:"
echo "1. Edit /etc/mmdvm-sdr/Config.h if needed"
echo "2. Configure MMDVMHost MMDVM.ini"
echo "3. Run: mmdvm-sdr"
echo ""
```

---

### Step 7.3: Create systemd Service

**Location:** `/etc/systemd/system/mmdvm-sdr.service`

```ini
[Unit]
Description=MMDVM-SDR UDP Modem
After=network.target
Wants=network.target

[Service]
Type=simple
User=mmdvm
WorkingDirectory=/opt/mmdvm-sdr
ExecStart=/usr/local/bin/mmdvm-sdr
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

# Resource limits
LimitNOFILE=4096
Nice=-10

[Install]
WantedBy=multi-user.target
```

**Install:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable mmdvm-sdr
sudo systemctl start mmdvm-sdr
sudo systemctl status mmdvm-sdr
```

---

### Step 7.4: Version and Changelog

**Update VERSION file:**
```
MMDVM-SDR v2.0.0
- Added UDP modem transport support
- Compatible with MMDVMHost native UDP mode
- Eliminates need for virtual PTY and RTS patch
- Network-transparent operation
- Improved error handling and diagnostics
```

**Update CHANGELOG.md:**
```markdown
# Changelog

## [2.0.0] - 2025-11-16

### Added
- UDP modem transport support (compatible with MMDVMHost)
- UDPSocket class for network I/O
- UDPModemPort class implementing ISerialPort
- Compile-time selection between PTY and UDP
- Ring buffer for received UDP data
- Source address validation for security
- Comprehensive unit tests
- Network deployment documentation

### Changed
- SerialPort now uses polymorphic ISerialPort interface
- MMDVM.cpp supports runtime port injection
- CMakeLists.txt with UDP build option

### Fixed
- None (new feature)

### Deprecated
- Virtual PTY still supported but UDP recommended

## [1.0.0] - Previous Release
...
```

---

## Timeline & Milestones

### Day 1: UDP Foundation (8 hours)

**Morning (4h):**
- ☑ Phase 1: UDP Socket Layer
  - Create UDPSocket.h/cpp
  - Write unit tests
  - Verify socket operations

**Afternoon (4h):**
- ☑ Phase 2: UDP Modem Port
  - Create UDPModemPort.h/cpp
  - Write unit tests
  - Verify MMDVM protocol transport

### Day 2: Integration & Testing (8 hours)

**Morning (4h):**
- ☑ Phase 3: Integration
  - Modify ISerialPort, SerialPort
  - Update MMDVM.cpp
  - First compile

**Afternoon (4h):**
- ☑ Phase 4: Configuration
  - Update Config.h
  - Update CMakeLists.txt
  - Build variants (PTY/UDP)

### Day 3: Testing & Documentation (8 hours)

**Morning (4h):**
- ☑ Phase 5: Testing
  - Unit tests
  - Integration tests
  - Network tests
  - Performance tests

**Afternoon (4h):**
- ☑ Phase 6: Documentation
  - Update README
  - Create quick start guide
  - Code comments

### Day 4: Polish & Deploy (4 hours)

**Morning (2h):**
- ☑ Phase 7: Deployment
  - Release build
  - Install script
  - systemd service

**Afternoon (2h):**
- ☑ Final verification
  - Clean install test
  - Documentation review
  - Git commit and push

---

## Risk Mitigation

### Risk 1: Breaking Existing PTY Functionality

**Probability:** Medium
**Impact:** High

**Mitigation:**
- Use additive approach (don't remove PTY code)
- Compile-time selection (#ifdef)
- Regression testing in PTY mode
- Keep PTY as default initially

**Contingency:**
- If PTY breaks, revert integration changes
- Fix in separate branch
- Re-merge when stable

---

### Risk 2: UDP Performance Issues

**Probability:** Low
**Impact:** Medium

**Mitigation:**
- Follow MMDVMHost's proven UDPController design
- Non-blocking I/O
- Ring buffer for RX
- Performance testing before release

**Contingency:**
- Increase ring buffer size
- Optimize read/write loops
- Add configurable buffer size

---

### Risk 3: Network Configuration Complexity

**Probability:** Medium
**Impact:** Low

**Mitigation:**
- Default to localhost (simplest)
- Clear documentation
- Example configurations
- Troubleshooting guide

**Contingency:**
- Create configuration wizard script
- Add auto-detection of network interfaces
- Provide pre-configured images

---

### Risk 4: Firewall/Security Issues

**Probability:** High
**Impact:** Low

**Mitigation:**
- Document firewall requirements
- Source address validation built-in
- Recommend localhost for single-machine setup
- VPN documentation for remote

**Contingency:**
- Create firewall setup script
- Add connection test utility
- Diagnostic mode to show rejected packets

---

## Success Criteria

### Minimum Viable Product (MVP)

**Must Have:**
- ✅ UDP socket layer works
- ✅ UDP modem port implements ISerialPort
- ✅ Compiles in both PTY and UDP modes
- ✅ Localhost integration test passes
- ✅ Basic documentation exists

**Should Have:**
- ✅ Network (LAN) integration test passes
- ✅ All unit tests pass
- ✅ Performance acceptable (<5ms latency)
- ✅ Quick start guide

**Nice to Have:**
- ⭕ Configuration file (runtime selection)
- ⭕ Systemd service
- ⭕ Install script
- ⭕ Doxygen documentation

### Acceptance Criteria

**Functional:**
1. MMDVM-SDR communicates with MMDVMHost over UDP
2. All MMDVM protocol commands work
3. DMR mode transmits and receives
4. Error recovery works (survive host crash)

**Performance:**
1. Latency < 2ms (localhost)
2. Latency < 5ms (LAN)
3. No packet loss at normal rates
4. CPU usage similar to PTY mode

**Quality:**
1. No compiler warnings
2. All unit tests pass
3. No memory leaks (valgrind clean)
4. Code documented

**Usability:**
1. Configuration clear and simple
2. Error messages helpful
3. Documentation complete
4. Examples provided

---

## Appendix A: Checklist

### Pre-Implementation
- [ ] Read MMDVMHOST_ANALYSIS.md
- [ ] Read UDP_INTEGRATION.md
- [ ] Set up development environment
- [ ] Verify current build works
- [ ] Create feature branch

### Phase 1: UDP Socket
- [ ] Create UDPSocket.h
- [ ] Create UDPSocket.cpp
- [ ] Create test_udpsocket.cpp
- [ ] All socket tests pass

### Phase 2: UDP Modem Port
- [ ] Create UDPModemPort.h
- [ ] Create UDPModemPort.cpp
- [ ] Create test_udpmodemport.cpp
- [ ] All modem port tests pass

### Phase 3: Integration
- [ ] Update ISerialPort.h (if needed)
- [ ] Update SerialPort.h
- [ ] Update SerialPort.cpp
- [ ] Update MMDVM.cpp
- [ ] Compiles without errors

### Phase 4: Configuration
- [ ] Update Config.h
- [ ] Update CMakeLists.txt
- [ ] Test UDP build
- [ ] Test PTY build (regression)

### Phase 5: Testing
- [ ] Unit tests pass
- [ ] Localhost integration works
- [ ] Network integration works
- [ ] Performance acceptable
- [ ] Error recovery works
- [ ] PTY mode still works

### Phase 6: Documentation
- [ ] Update README.md
- [ ] Create UDP_QUICKSTART.md
- [ ] Add code comments
- [ ] Generate Doxygen docs

### Phase 7: Deployment
- [ ] Create release build
- [ ] Create install script
- [ ] Create systemd service
- [ ] Update VERSION
- [ ] Update CHANGELOG.md

### Final
- [ ] Clean install test
- [ ] Code review
- [ ] Git commit
- [ ] Create pull request / merge

---

## Appendix B: Useful Commands

### Development
```bash
# Quick rebuild
make -j4

# Clean build
make clean && make -j4

# Syntax check only
g++ -fsyntax-only -std=c++11 -I. file.cpp

# Show symbols
nm -C file.o | grep " T "  # Defined symbols
nm -C file.o | grep " U "  # Undefined symbols
```

### Testing
```bash
# Run unit tests
./tests/test_udpsocket
./tests/test_udpmodemport

# Run with valgrind (memory leak check)
valgrind --leak-check=full ./mmdvm

# Stress test
for i in {1..100}; do ./tests/test_udpmodemport; done
```

### Debugging
```bash
# Packet capture
sudo tcpdump -i any -w capture.pcap 'udp port 3334 or 3335'
sudo tcpdump -r capture.pcap -X

# Process monitoring
ps aux | grep mmdvm
top -p $(pgrep mmdvm)

# Port status
sudo netstat -tulpn | grep 3334
sudo lsof -i :3334

# Firewall status
sudo ufw status
sudo iptables -L -n
```

### Network
```bash
# Test UDP connectivity
nc -u target_ip 3334

# Test latency
ping -c 100 target_ip | tail -1

# Test bandwidth
iperf3 -c target_ip -u -b 10K
```

---

## Appendix C: Troubleshooting Decision Tree

```
[START]
  │
  ├─ Can't compile?
  │  ├─ Missing UDPSocket.h? → Add to project
  │  ├─ Undefined symbols? → Check CMakeLists.txt includes all files
  │  ├─ Wrong -D flags? → Verify USE_UDP_MODEM defined
  │  └─ Header conflicts? → Check include order
  │
  ├─ Can't bind socket?
  │  ├─ Port in use? → Check with netstat, change port
  │  ├─ Permission denied? → Use port >1024 or run as root
  │  └─ Invalid address? → Verify IP format
  │
  ├─ No communication?
  │  ├─ Firewall? → Check ufw/iptables, allow ports
  │  ├─ Wrong IP? → Verify configuration matches
  │  ├─ Network down? → Ping target
  │  └─ Source mismatch? → Check tcpdump for rejected packets
  │
  ├─ High latency?
  │  ├─ WiFi? → Switch to Ethernet
  │  ├─ CPU overload? → Check top
  │  ├─ Swapping? → Check free -h
  │  └─ Network congestion? → Use dedicated LAN
  │
  ├─ Buffer overflow?
  │  ├─ Processing slow? → Optimize main loop
  │  ├─ Buffer too small? → Increase BUFFER_SIZE
  │  └─ Too much traffic? → Check for broadcast storms
  │
  └─ MMDVMHost says "No modem"?
     ├─ MMDVM-SDR running? → Check ps aux
     ├─ Config mismatch? → Verify addresses/ports
     ├─ Protocol mismatch? → Must be "udp" in MMDVM.ini
     └─ Network issue? → Test with nc
```

---

**End of Implementation Plan**

*Document Version 1.0*
*2025-11-16*

Ready to begin implementation!
