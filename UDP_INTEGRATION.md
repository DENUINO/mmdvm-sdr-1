# UDP Integration Guide for MMDVM-SDR

**Document Version:** 1.0
**Date:** 2025-11-16
**Target:** mmdvm-sdr UDP modem support
**Reference:** MMDVMHost UDPController implementation

---

## Executive Summary

This document provides a comprehensive guide for integrating UDP-based modem communication into mmdvm-sdr, replacing the current virtual PTY implementation with a network-based solution that is compatible with MMDVMHost's native UDP modem support.

**Key Benefits:**
- ✅ No MMDVMHost code modification required (eliminates RTS patch)
- ✅ Network-transparent operation (modem can be on different machine)
- ✅ Standard, debuggable protocol (Wireshark, tcpdump)
- ✅ Better error handling and diagnostics
- ✅ Simpler deployment (no PTY symlinks or permissions)
- ✅ Production-proven (already used in MMDVMHost deployments)

---

## Table of Contents

1. [UDP Feature Overview](#udp-feature-overview)
2. [Protocol Specification](#protocol-specification)
3. [Current vs. UDP Architecture](#current-vs-udp-architecture)
4. [Integration Requirements](#integration-requirements)
5. [Code Changes Required](#code-changes-required)
6. [Configuration Changes](#configuration-changes)
7. [Network Setup](#network-setup)
8. [Testing Strategy](#testing-strategy)
9. [Deployment Scenarios](#deployment-scenarios)
10. [Troubleshooting Guide](#troubleshooting-guide)

---

## UDP Feature Overview

### What is UDP Modem Communication?

Instead of emulating a serial port using virtual PTY (pseudo-terminal), the modem communicates with MMDVMHost over a **UDP network socket**. The same MMDVM binary protocol is used, but transported over UDP packets instead of serial I/O.

### Architecture Comparison

**Current Implementation (Virtual PTY):**
```
┌─────────────────────────────────────────────┐
│ MMDVM-SDR Process                           │
│ ┌─────────────────────────────────────────┐ │
│ │ SerialController                        │ │
│ │ - posix_openpt() → /dev/pts/X          │ │
│ │ - Create symlink: ttyMMDVM0 → pts/X    │ │
│ │ - write()/read() on PTY master         │ │
│ └─────────────────────────────────────────┘ │
└────────────┬────────────────────────────────┘
             │ Virtual PTY
             │ (kernel TTY subsystem)
             ▼
┌─────────────────────────────────────────────┐
│ MMDVMHost Process                           │
│ ┌─────────────────────────────────────────┐ │
│ │ UARTController                          │ │
│ │ - open("/path/to/ttyMMDVM0")           │ │
│ │ - read()/write() on PTY slave          │ │
│ │ - **Requires RTS check disable patch** │ │
│ └─────────────────────────────────────────┘ │
└─────────────────────────────────────────────┘
```

**UDP Implementation:**
```
┌─────────────────────────────────────────────┐
│ MMDVM-SDR Process (Modem)                   │
│ ┌─────────────────────────────────────────┐ │
│ │ UDPModemPort                            │ │
│ │ - Bind UDP socket to 192.168.2.100:3334│ │
│ │ - sendto()/recvfrom() on socket        │ │
│ └─────────────────────────────────────────┘ │
└────────────┬────────────────────────────────┘
             │ UDP Network
             │ (Ethernet, WiFi, or localhost)
             ▼
┌─────────────────────────────────────────────┐
│ MMDVMHost Process (Host)                    │
│ ┌─────────────────────────────────────────┐ │
│ │ UDPController (stock MMDVMHost)         │ │
│ │ - Bind UDP socket to 192.168.2.1:3335  │ │
│ │ - sendto() → 192.168.2.100:3334        │ │
│ │ - recvfrom() with source validation    │ │
│ │ - **No code modification needed!**     │ │
│ └─────────────────────────────────────────┘ │
└─────────────────────────────────────────────┘
```

### Why UDP is Better

| Aspect | Virtual PTY | UDP Network |
|--------|-------------|-------------|
| **MMDVMHost Modification** | Required (RTS patch) | None (stock) |
| **Deployment** | Complex (symlinks, permissions) | Simple (config only) |
| **Location** | Same machine only | Network-capable |
| **Debugging** | Difficult (strace) | Easy (Wireshark) |
| **Error Handling** | Implicit (TTY errors) | Explicit (network errors) |
| **Latency** | ~1-2ms | ~0.5-2ms |
| **Scalability** | One PTY per modem | Multiple modems easily |
| **Production Use** | Custom solution | Standard MMDVMHost feature |

---

## Protocol Specification

### UDP Socket Parameters

**Modem Side (mmdvm-sdr):**
```cpp
Local Address:  192.168.2.100  (or 127.0.0.1 for localhost)
Local Port:     3334           (configurable)
Remote Address: 192.168.2.1    (MMDVMHost address)
Remote Port:    3335           (MMDVMHost listening port)
```

**Host Side (MMDVMHost):**
```cpp
Local Address:  192.168.2.1    (or 127.0.0.1 for localhost)
Local Port:     3335           (configurable)
Remote Address: 192.168.2.100  (Modem address)
Remote Port:    3334           (Modem listening port)
```

### UDP Packet Format

```
┌──────────────────────────────────────────────────┐
│ UDP Header (8 bytes)                             │
│ - Source Port: 3334                              │
│ - Dest Port: 3335                                │
│ - Length: (MMDVM frame size + 8)                 │
│ - Checksum: (calculated by UDP layer)            │
├──────────────────────────────────────────────────┤
│ MMDVM Frame                                      │
│ ┌──────┬────────┬──────┬──────────────────────┐ │
│ │ 0xE0 │ LENGTH │ TYPE │     PAYLOAD          │ │
│ └──────┴────────┴──────┴──────────────────────┘ │
└──────────────────────────────────────────────────┘
```

**Key Points:**
- One MMDVM frame per UDP packet
- No fragmentation (MMDVM frames are small, typically 20-100 bytes)
- UDP checksum provides integrity check
- No TCP overhead (connection establishment, ACKs, etc.)

### Communication Flow

**1. Startup Handshake:**
```
MMDVMHost                           MMDVM-SDR Modem
    │                                      │
    │──── GET_VERSION (0xE0 0x04 0x00) ──►│
    │                                      │
    │◄──── Version Response ──────────────│
    │      (protocol=1, description)      │
    │                                      │
    │──── SET_CONFIG (0xE0 0x10 0x02...) ─►│
    │                                      │
    │◄──── ACK (0xE0 0x04 0x70 0x02) ─────│
    │                                      │
    │──── SET_MODE (0xE0 0x04 0x03 0x00) ─►│
    │      (STATE_IDLE)                   │
    │                                      │
    │◄──── ACK (0xE0 0x04 0x70 0x03) ─────│
    │                                      │
```

**2. Status Polling (Every 250ms):**
```
MMDVMHost                           MMDVM-SDR Modem
    │                                      │
    │──── GET_STATUS (0xE0 0x03 0x01) ────►│
    │                                      │
    │◄──── Status Response ───────────────│
    │      (modes, state, TX/RX, buffers) │
    │                                      │
    [Repeat every 250ms]
```

**3. TX Data Frame (DMR Example):**
```
MMDVMHost                           MMDVM-SDR Modem
    │                                      │
    │──── DMR_DATA2 (0xE0 0x21 0x1A...) ──►│
    │      (33-byte DMR frame)            │
    │                                      │
    │                                      │ [Buffer frame]
    │                                      │
    │──── DMR_START (0xE0 0x04 0x1D 0x01)─►│
    │                                      │
    │                                      │ [Begin TX]
    │                                      │ [RF transmission]
    │                                      │
```

**4. RX Data Frame:**
```
MMDVMHost                           MMDVM-SDR Modem
    │                                      │
    │                                      │ [RF reception]
    │                                      │ [Demodulation]
    │                                      │
    │◄──── DMR_DATA2 (0xE0 0x21 0x1A...) ─│
    │      (received DMR frame)           │
    │                                      │
```

### Security Features

**Source Address Validation:**
```cpp
// MMDVMHost UDPController::read() pseudocode
int UDPController::read(unsigned char* buffer, unsigned int length) {
    // Receive UDP packet
    int ret = m_socket.read(tempBuffer, 600, m_addr, m_addrLen);

    // Validate source address matches configured modem
    if (!CUDPSocket::match(m_addr, m_modemAddr)) {
        // Reject packet from unauthorized source
        return 0;
    }

    // Accept and buffer the data
    m_buffer.addData(tempBuffer, ret);
    return m_buffer.getData(buffer, length);
}
```

**Protection Against:**
- Spoofed packets from rogue sources
- Command injection from unauthorized hosts
- Man-in-the-middle attacks (on isolated network)

**Does NOT Protect Against:**
- Eavesdropping (use VPN/IPSec if needed)
- Replay attacks (MMDVM protocol has no challenge-response)

---

## Current vs. UDP Architecture

### Current Implementation Analysis

**Files Involved:**
```
mmdvm-sdr/
├── SerialController.h        # PTY master management
├── SerialController.cpp      # posix_openpt(), grantpt(), etc.
├── SerialPort.h              # MMDVM protocol handler
├── SerialPort.cpp            # Frame parsing, command processing
├── ISerialPort.h             # Abstract interface
└── MMDVM.cpp                 # Main loop
```

**Key Code Flow (Current):**

1. **PTY Creation (SerialController.cpp:52-85):**
```cpp
bool CSerialController::open() {
    // Open PTY master
    m_fd = ::open(m_device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY, 0);

    // Grant access and unlock
    grantpt(m_fd);
    unlockpt(m_fd);

    // Get slave PTY name
    char* pts_name = ptsname(m_fd);

    // Create symlink: ttyMMDVM0 → /dev/pts/X
    symlink(pts_name, "ttyMMDVM0");

    // Configure terminal settings
    termios termios;
    tcgetattr(m_fd, &termios);
    // ... (set raw mode, baud rate, etc.)
    tcsetattr(m_fd, TCSANOW, &termios);
}
```

2. **Data I/O (SerialController.cpp:170-256):**
```cpp
int CSerialController::read(unsigned char* buffer, unsigned int length) {
    // Blocking read with select()
    fd_set fds;
    FD_SET(m_fd, &fds);
    select(m_fd + 1, &fds, NULL, NULL, &tv);

    return ::read(m_fd, buffer, length);
}

int CSerialController::write(const unsigned char* buffer, unsigned int length) {
    return ::write(m_fd, buffer, length);
}
```

3. **Protocol Handling (SerialPort.cpp:579-893):**
```cpp
void CSerialPort::process() {
    while (availableInt(1U)) {
        uint8_t c = readInt(1U);

        // Frame assembly state machine
        if (m_ptr == 0U && c == MMDVM_FRAME_START) {
            m_buffer[0U] = c;
            m_ptr = 1U;
        } else if (m_ptr == 1U) {
            m_len = c;  // Frame length
            m_ptr = 2U;
        } else {
            m_buffer[m_ptr++] = c;

            if (m_ptr == m_len) {
                // Complete frame received
                processCommand(m_buffer);
                m_ptr = 0U;
            }
        }
    }
}
```

**Observation:** The protocol handling is **completely independent** of the transport mechanism. Only `SerialController` needs to change!

### UDP Implementation Structure

**New/Modified Files:**
```
mmdvm-sdr/
├── UDPSocket.h               # NEW: UDP socket wrapper
├── UDPSocket.cpp             # NEW: Socket operations
├── UDPModemPort.h            # NEW: UDP transport (implements ISerialPort)
├── UDPModemPort.cpp          # NEW: UDP I/O with ring buffer
├── ISerialPort.h             # MODIFIED: Rename to IModemPort (optional)
├── SerialPort.h              # NO CHANGE
├── SerialPort.cpp            # NO CHANGE
├── Config.h                  # MODIFIED: Add UDP configuration
└── MMDVM.cpp                 # MODIFIED: Select transport based on config
```

**New Architecture:**

```
┌─────────────────────────────────────────────────┐
│                   MMDVM.cpp                     │
│                   main loop                     │
└────────────────┬────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────┐
│              CSerialPort                        │
│         (MMDVM Protocol Handler)                │
│  - Frame parsing                                │
│  - Command dispatch                             │
│  - Mode management                              │
└────────────────┬────────────────────────────────┘
                 │ Uses ISerialPort interface
                 │
         ┌───────┴────────┐
         │                │
         ▼                ▼
┌──────────────┐  ┌──────────────┐
│ CSerial      │  │ CUDPModem    │
│ Controller   │  │ Port         │
│              │  │              │
│ PTY I/O      │  │ UDP I/O      │
└──────────────┘  └──────────────┘
```

**Key Insight:** Both PTY and UDP implement the same interface, so `CSerialPort` doesn't know or care which transport is used!

---

## Integration Requirements

### System Requirements

**Operating System:**
- Linux (primary target)
- POSIX sockets API
- UDP/IP stack (standard on all modern Linux)

**Libraries:**
- Standard C library (libc)
- POSIX threads (pthread)
- Socket libraries (typically built-in)
- Math library (libm)

**Optional:**
- libiio (for PlutoSDR in standalone mode)
- libzmq (for GNU Radio integration mode)

### Network Requirements

**Minimum Network Configuration:**

1. **Localhost Mode (Simplest):**
   ```
   Modem:  127.0.0.1:3334
   Host:   127.0.0.1:3335

   Requirements: None (loopback interface always available)
   Latency: <0.5ms
   Bandwidth: Unlimited
   Security: Maximum (no network exposure)
   ```

2. **LAN Mode:**
   ```
   Modem:  192.168.X.100:3334
   Host:   192.168.X.1:3335

   Requirements:
   - Ethernet or WiFi connection
   - Static IP or DHCP reservation
   - Firewall rules (allow UDP 3334↔3335)

   Latency: 1-2ms
   Bandwidth: ~10 KB/sec
   Security: Medium (LAN-only)
   ```

3. **Remote Mode (VPN):**
   ```
   Modem:  10.8.0.100:3334  (VPN address)
   Host:   10.8.0.1:3335    (VPN address)

   Requirements:
   - VPN tunnel (Wireguard, OpenVPN, etc.)
   - VPN routing configuration
   - Encrypted connection

   Latency: Variable (depends on VPN and distance)
   Bandwidth: ~10 KB/sec (encrypted)
   Security: High (encrypted tunnel)
   ```

**Bandwidth Requirements:**

MMDVM protocol is very lightweight:

| Traffic Type | Rate | Size |
|--------------|------|------|
| Status polls | 4/sec | 20 bytes each = 80 B/sec |
| DMR frames | 50/sec | 33 bytes each = 1,650 B/sec |
| D-Star frames | 50/sec | ~30 bytes each = 1,500 B/sec |
| **Total Peak** | - | **~5 KB/sec** |

**Conclusion:** Any network connection (even slow WiFi) is adequate.

### Firewall Configuration

**iptables (Linux):**
```bash
# Allow UDP traffic between modem and host
iptables -A INPUT -p udp --sport 3334 --dport 3335 -s 192.168.2.100 -j ACCEPT
iptables -A OUTPUT -p udp --sport 3335 --dport 3334 -d 192.168.2.100 -j ACCEPT

# Block all other UDP traffic to these ports
iptables -A INPUT -p udp --dport 3335 -j DROP
```

**ufw (Simpler Linux firewall):**
```bash
# Allow from modem IP only
ufw allow from 192.168.2.100 to any port 3335 proto udp
```

**Windows Firewall:**
```powershell
# Create inbound rule
New-NetFirewallRule -DisplayName "MMDVM UDP" -Direction Inbound `
    -Protocol UDP -LocalPort 3335 -RemoteAddress 192.168.2.100 -Action Allow
```

---

## Code Changes Required

### 1. Create UDP Socket Wrapper (UDPSocket.h/cpp)

**Purpose:** Low-level UDP socket operations

**UDPSocket.h:**
```cpp
#ifndef UDPSOCKET_H
#define UDPSOCKET_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class CUDPSocket {
public:
    CUDPSocket(const std::string& address, unsigned int port);
    ~CUDPSocket();

    bool open();
    void close();

    int read(unsigned char* buffer, unsigned int length,
             sockaddr_storage& addr, unsigned int& addrLen);

    int write(const unsigned char* buffer, unsigned int length,
              const sockaddr_storage& addr, unsigned int addrLen);

    static bool lookup(const std::string& hostname, unsigned int port,
                      sockaddr_storage& addr, unsigned int& addrLen);

    static bool match(const sockaddr_storage& addr1,
                     const sockaddr_storage& addr2);

private:
    std::string  m_address;
    unsigned int m_port;
    int          m_fd;
};

#endif
```

**UDPSocket.cpp (Key Functions):**
```cpp
#include "UDPSocket.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>

CUDPSocket::CUDPSocket(const std::string& address, unsigned int port) :
    m_address(address),
    m_port(port),
    m_fd(-1)
{
}

bool CUDPSocket::open() {
    // Create UDP socket
    m_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_fd < 0)
        return false;

    // Set socket options (non-blocking, reuse address)
    int opt = 1;
    setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to local address
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_address.c_str(), &addr.sin_addr);

    if (bind(m_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close();
        return false;
    }

    return true;
}

int CUDPSocket::read(unsigned char* buffer, unsigned int length,
                     sockaddr_storage& addr, unsigned int& addrLen) {
    addrLen = sizeof(sockaddr_storage);
    return recvfrom(m_fd, buffer, length, 0,
                   (sockaddr*)&addr, (socklen_t*)&addrLen);
}

int CUDPSocket::write(const unsigned char* buffer, unsigned int length,
                     const sockaddr_storage& addr, unsigned int addrLen) {
    return sendto(m_fd, buffer, length, 0,
                 (const sockaddr*)&addr, addrLen);
}

bool CUDPSocket::lookup(const std::string& hostname, unsigned int port,
                       sockaddr_storage& addr, unsigned int& addrLen) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* result;
    if (getaddrinfo(hostname.c_str(), NULL, &hints, &result) != 0)
        return false;

    memcpy(&addr, result->ai_addr, result->ai_addrlen);
    ((sockaddr_in*)&addr)->sin_port = htons(port);
    addrLen = result->ai_addrlen;

    freeaddrinfo(result);
    return true;
}

bool CUDPSocket::match(const sockaddr_storage& addr1,
                      const sockaddr_storage& addr2) {
    if (addr1.ss_family != addr2.ss_family)
        return false;

    if (addr1.ss_family == AF_INET) {
        const sockaddr_in* in1 = (const sockaddr_in*)&addr1;
        const sockaddr_in* in2 = (const sockaddr_in*)&addr2;

        return (in1->sin_addr.s_addr == in2->sin_addr.s_addr) &&
               (in1->sin_port == in2->sin_port);
    }

    return false;  // IPv6 not implemented
}

void CUDPSocket::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}
```

**Lines of Code:** ~150 lines

---

### 2. Create UDP Modem Port (UDPModemPort.h/cpp)

**Purpose:** Implements ISerialPort interface using UDP

**UDPModemPort.h:**
```cpp
#ifndef UDPMODEMPORT_H
#define UDPMODEMPORT_H

#include "ISerialPort.h"
#include "UDPSocket.h"
#include <string>
#include <vector>

class CUDPModemPort : public ISerialPort {
public:
    CUDPModemPort(const std::string& modemAddress,
                  unsigned int modemPort,
                  const std::string& localAddress,
                  unsigned int localPort);

    virtual ~CUDPModemPort();

    virtual bool open();
    virtual void close();
    virtual int read(unsigned char* buffer, unsigned int length);
    virtual int write(const unsigned char* buffer, unsigned int length);

private:
    CUDPSocket m_socket;
    sockaddr_storage m_modemAddr;
    unsigned int m_modemAddrLen;

    // Ring buffer for received data
    std::vector<unsigned char> m_buffer;
    unsigned int m_bufferHead;
    unsigned int m_bufferTail;

    void addToBuffer(const unsigned char* data, unsigned int length);
    unsigned int getFromBuffer(unsigned char* data, unsigned int length);
};

#endif
```

**UDPModemPort.cpp:**
```cpp
#include "UDPModemPort.h"
#include "Log.h"
#include <cstring>

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
    // Resolve modem address
    if (!CUDPSocket::lookup(modemAddress, modemPort,
                           m_modemAddr, m_modemAddrLen)) {
        LogError("Failed to resolve modem address: %s:%u",
                 modemAddress.c_str(), modemPort);
    }
}

CUDPModemPort::~CUDPModemPort() {
    close();
}

bool CUDPModemPort::open() {
    if (!m_socket.open()) {
        LogError("Failed to open UDP socket");
        return false;
    }

    LogMessage("UDP modem port opened successfully");
    return true;
}

void CUDPModemPort::close() {
    m_socket.close();
}

int CUDPModemPort::read(unsigned char* buffer, unsigned int length) {
    // First, try to drain any buffered data
    unsigned int available = getFromBuffer(buffer, length);
    if (available > 0)
        return available;

    // Receive new UDP packet
    unsigned char tempBuffer[600];
    sockaddr_storage addr;
    unsigned int addrLen;

    int ret = m_socket.read(tempBuffer, 600, addr, addrLen);
    if (ret <= 0)
        return 0;

    // Validate source address (security)
    if (!CUDPSocket::match(addr, m_modemAddr)) {
        LogWarning("Rejected UDP packet from unauthorized source");
        return 0;
    }

    // Add to ring buffer
    addToBuffer(tempBuffer, ret);

    // Return requested data
    return getFromBuffer(buffer, length);
}

int CUDPModemPort::write(const unsigned char* buffer, unsigned int length) {
    // Write directly to modem address (no buffering)
    return m_socket.write(buffer, length, m_modemAddr, m_modemAddrLen);
}

void CUDPModemPort::addToBuffer(const unsigned char* data, unsigned int length) {
    for (unsigned int i = 0; i < length; i++) {
        m_buffer[m_bufferHead] = data[i];
        m_bufferHead = (m_bufferHead + 1) % BUFFER_SIZE;

        // Buffer overflow - drop oldest data
        if (m_bufferHead == m_bufferTail) {
            m_bufferTail = (m_bufferTail + 1) % BUFFER_SIZE;
            LogWarning("UDP buffer overflow, dropping data");
        }
    }
}

unsigned int CUDPModemPort::getFromBuffer(unsigned char* data, unsigned int length) {
    unsigned int count = 0;

    while (count < length && m_bufferHead != m_bufferTail) {
        data[count++] = m_buffer[m_bufferTail];
        m_bufferTail = (m_bufferTail + 1) % BUFFER_SIZE;
    }

    return count;
}
```

**Lines of Code:** ~120 lines

---

### 3. Modify Configuration (Config.h)

**Add UDP Configuration Section:**

```cpp
// Existing serial configuration
//#define SERIAL_PORT "/dev/ptmx"  // Disabled by default

// UDP modem configuration
#define USE_UDP_MODEM              // Enable UDP mode
#define UDP_MODEM_ADDRESS  "127.0.0.1"
#define UDP_MODEM_PORT     3334
#define UDP_LOCAL_ADDRESS  "127.0.0.1"
#define UDP_LOCAL_PORT     3335

// Alternative: Use #ifdef to select transport
// #define USE_UDP_MODEM
// #undef USE_UDP_MODEM  // Comment out to use PTY
```

**Lines Changed:** ~10 lines

---

### 4. Modify Main Loop (MMDVM.cpp)

**Conditional Transport Selection:**

```cpp
#include "Config.h"
#include "SerialPort.h"

#ifdef USE_UDP_MODEM
  #include "UDPModemPort.h"
#else
  #include "SerialController.h"
#endif

CSerialPort serial;
CIO io;

void setup() {
    LogDebug("MMDVM modem setup()");

#ifdef USE_UDP_MODEM
    // Create UDP transport
    CUDPModemPort* port = new CUDPModemPort(
        UDP_MODEM_ADDRESS,
        UDP_MODEM_PORT,
        UDP_LOCAL_ADDRESS,
        UDP_LOCAL_PORT
    );

    if (!port->open()) {
        LogError("Failed to open UDP modem port");
        return;
    }

    serial.setPort(port);  // Inject UDP transport
    LogMessage("Using UDP modem transport");
#else
    // Create PTY transport
    CSerialController* controller = new CSerialController();
    if (!controller->open()) {
        LogError("Failed to open PTY");
        return;
    }

    serial.setPort(controller);  // Inject PTY transport
    LogMessage("Using PTY transport");
#endif

    serial.start();
}

// Rest of loop() unchanged
```

**Alternative: Runtime Configuration:**

```cpp
void setup() {
    // Read config file
    std::string protocol = readConfig("Modem", "Protocol");

    if (protocol == "udp") {
        std::string modemAddr = readConfig("Modem", "ModemAddress");
        int modemPort = readConfigInt("Modem", "ModemPort");
        std::string localAddr = readConfig("Modem", "LocalAddress");
        int localPort = readConfigInt("Modem", "LocalPort");

        CUDPModemPort* port = new CUDPModemPort(
            modemAddr, modemPort, localAddr, localPort);

        serial.setPort(port);
    } else {
        // Default to PTY
        CSerialController* controller = new CSerialController();
        serial.setPort(controller);
    }
}
```

**Lines Changed:** ~30 lines

---

### 5. Modify SerialPort Interface (Optional)

**If you want to support both PTY and UDP at runtime:**

**ISerialPort.h:**
```cpp
class ISerialPort {
public:
    virtual ~ISerialPort() {}

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual int read(unsigned char* buffer, unsigned int length) = 0;
    virtual int write(const unsigned char* buffer, unsigned int length) = 0;
};
```

**SerialPort.h - Add Port Injection:**
```cpp
class CSerialPort {
public:
    CSerialPort();

    void setPort(ISerialPort* port);  // NEW: Inject transport

    void start();
    void process();

    // ... rest unchanged

private:
    ISerialPort* m_port;  // NEW: Polymorphic port
    // ... rest unchanged
};
```

**SerialPort.cpp:**
```cpp
CSerialPort::CSerialPort() :
    m_port(nullptr),  // NEW
    m_buffer(),
    // ... rest unchanged
{
}

void CSerialPort::setPort(ISerialPort* port) {
    m_port = port;
}

void CSerialPort::start() {
    if (m_port != nullptr)
        m_port->open();

    // ... rest of initialization
}

// Modify readInt/writeInt to use m_port instead of m_controller
int CSerialPort::readInt(uint8_t n) {
    unsigned char byte;
    if (m_port->read(&byte, 1) == 1)
        return byte;
    return -1;
}

void CSerialPort::writeInt(uint8_t n, const uint8_t* data,
                           uint16_t length, bool flush) {
    m_port->write(data, length);
}
```

**Lines Changed:** ~20 lines

---

### Summary of Code Changes

| File | Status | Lines |
|------|--------|-------|
| UDPSocket.h | NEW | ~50 |
| UDPSocket.cpp | NEW | ~150 |
| UDPModemPort.h | NEW | ~40 |
| UDPModemPort.cpp | NEW | ~120 |
| Config.h | MODIFIED | ~10 |
| MMDVM.cpp | MODIFIED | ~30 |
| ISerialPort.h | MODIFIED (optional) | ~5 |
| SerialPort.h | MODIFIED (optional) | ~5 |
| SerialPort.cpp | MODIFIED (optional) | ~20 |
| **TOTAL** | | **~430 lines** |

**Effort Estimate:** 1-2 days for implementation + testing

---

## Configuration Changes

### MMDVM-SDR Configuration

**Option 1: Compile-Time Configuration (Simple)**

**Config.h:**
```cpp
// Modem Transport Selection
#define USE_UDP_MODEM              // Comment out to use PTY

#ifdef USE_UDP_MODEM
  // UDP Configuration
  #define UDP_MODEM_ADDRESS  "127.0.0.1"  // MMDVMHost address
  #define UDP_MODEM_PORT     3335          // MMDVMHost port
  #define UDP_LOCAL_ADDRESS  "127.0.0.1"  // Modem bind address
  #define UDP_LOCAL_PORT     3334          // Modem bind port
#else
  // PTY Configuration
  #define SERIAL_PORT "/dev/ptmx"
  #define SERIAL_SPEED SERIAL_115200
#endif
```

**Usage:**
- To switch modes: Edit Config.h and rebuild
- Simple, no runtime overhead
- Good for embedded deployments

**Option 2: Configuration File (Advanced)**

**mmdvm.conf:**
```ini
[Modem]
# Transport: pty or udp
Transport=udp

# UDP Settings (when Transport=udp)
UDP.ModemAddress=127.0.0.1
UDP.ModemPort=3335
UDP.LocalAddress=127.0.0.1
UDP.LocalPort=3334

# PTY Settings (when Transport=pty)
PTY.Device=/dev/ptmx
PTY.Speed=115200

[SDR]
# SDR-specific settings
Device=pluto
SampleRate=1000000
Frequency=435000000

[Network]
# ZMQ settings (if not standalone)
ZMQ.TXPort=5990
ZMQ.RXPort=5991
```

**Configuration Parser:**
```cpp
#include <fstream>
#include <map>

class CConfig {
public:
    bool load(const std::string& filename);
    std::string get(const std::string& section,
                   const std::string& key);
    int getInt(const std::string& section,
              const std::string& key);

private:
    std::map<std::string, std::map<std::string, std::string>> m_config;
};

// Usage in setup():
CConfig config;
config.load("mmdvm.conf");

std::string transport = config.get("Modem", "Transport");
if (transport == "udp") {
    // Create UDP port
} else {
    // Create PTY
}
```

**Usage:**
- Runtime configuration without rebuild
- Easy to switch modes
- More user-friendly
- Can have multiple config files for different setups

---

### MMDVMHost Configuration

**MMDVM.ini - [Modem] Section:**

```ini
[Modem]
# IMPORTANT: Change Protocol from 'uart' to 'udp'
Protocol=udp

# UDP Modem Configuration
ModemAddress=127.0.0.1    # Address of MMDVM-SDR modem
ModemPort=3334            # Port MMDVM-SDR is listening on
LocalAddress=127.0.0.1    # Address MMDVMHost binds to
LocalPort=3335            # Port MMDVMHost listens on

# Alternative for PTY mode (old method):
#Protocol=uart
#UARTPort=/path/to/ttyMMDVM0
#UARTSpeed=115200

# Modem Parameters (same for both transports)
TXInvert=1
RXInvert=1
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
Debug=1
```

**Key Changes:**
1. `Protocol=uart` → `Protocol=udp`
2. Add `ModemAddress`, `ModemPort`, `LocalAddress`, `LocalPort`
3. Remove `UARTPort` and `UARTSpeed` (not needed)
4. **No RTS patch needed!** (major benefit)

---

## Network Setup

### Deployment Scenario 1: Localhost (Same Machine)

**Use Case:**
- MMDVM-SDR and MMDVMHost on same computer
- Maximum security (no network exposure)
- Simplest configuration

**Network Diagram:**
```
┌──────────────────────────────────────────┐
│         Single Computer                  │
│                                          │
│  ┌──────────────┐    ┌──────────────┐  │
│  │ MMDVM-SDR    │    │ MMDVMHost    │  │
│  │ 127.0.0.1    │◄──►│ 127.0.0.1    │  │
│  │ Port: 3334   │    │ Port: 3335   │  │
│  └──────────────┘    └──────────────┘  │
│         Loopback (lo) interface         │
└──────────────────────────────────────────┘
```

**MMDVM-SDR Config:**
```cpp
#define UDP_MODEM_ADDRESS  "127.0.0.1"  // MMDVMHost on localhost
#define UDP_MODEM_PORT     3335
#define UDP_LOCAL_ADDRESS  "127.0.0.1"  // Bind to localhost
#define UDP_LOCAL_PORT     3334
```

**MMDVMHost MMDVM.ini:**
```ini
[Modem]
Protocol=udp
ModemAddress=127.0.0.1
ModemPort=3334
LocalAddress=127.0.0.1
LocalPort=3335
```

**Startup:**
```bash
# Terminal 1: Start MMDVM-SDR
cd /path/to/mmdvm-sdr/build
./mmdvm

# Terminal 2: Start MMDVMHost
cd /path/to/MMDVMHost
./MMDVMHost MMDVM.ini
```

**Testing:**
```bash
# Monitor UDP traffic
sudo tcpdump -i lo -X udp port 3334 or udp port 3335

# You should see MMDVM protocol frames in hexdump
```

---

### Deployment Scenario 2: LAN (Different Machines)

**Use Case:**
- MMDVM-SDR on embedded device (Raspberry Pi, Pluto embedded CPU)
- MMDVMHost on server/desktop
- Local network only

**Network Diagram:**
```
┌──────────────────┐                 ┌──────────────────┐
│  Raspberry Pi    │                 │  Linux Server    │
│  MMDVM-SDR       │                 │  MMDVMHost       │
│                  │   Ethernet/     │                  │
│  192.168.1.100   │◄───────────────►│  192.168.1.10    │
│  Port: 3334      │    WiFi/LAN     │  Port: 3335      │
│                  │                 │                  │
│  ┌────────────┐  │                 │                  │
│  │ PlutoSDR   │  │                 │                  │
│  │ (USB/IP)   │  │                 │                  │
│  └────────────┘  │                 │                  │
└──────────────────┘                 └──────────────────┘
```

**MMDVM-SDR Config (on Pi):**
```cpp
#define UDP_MODEM_ADDRESS  "192.168.1.10"   // Server IP
#define UDP_MODEM_PORT     3335
#define UDP_LOCAL_ADDRESS  "192.168.1.100"  // Pi IP
#define UDP_LOCAL_PORT     3334
```

**MMDVMHost MMDVM.ini (on Server):**
```ini
[Modem]
Protocol=udp
ModemAddress=192.168.1.100    # Pi IP
ModemPort=3334
LocalAddress=192.168.1.10     # Server IP
LocalPort=3335
```

**Network Configuration (on Pi):**
```bash
# Set static IP (optional, but recommended)
sudo nano /etc/network/interfaces

# Add:
auto eth0
iface eth0 inet static
    address 192.168.1.100
    netmask 255.255.255.0
    gateway 192.168.1.1

# Or use DHCP reservation on router
```

**Firewall (on Server):**
```bash
# Allow UDP from Pi only
sudo ufw allow from 192.168.1.100 to any port 3335 proto udp
```

**Testing:**
```bash
# On Pi: Test connectivity
ping 192.168.1.10

# On Server: Monitor UDP
sudo tcpdump -i eth0 -X host 192.168.1.100 and udp

# On Pi: Start MMDVM-SDR
./mmdvm

# On Server: Start MMDVMHost
./MMDVMHost MMDVM.ini
```

---

### Deployment Scenario 3: Remote (VPN)

**Use Case:**
- MMDVM-SDR at remote site
- MMDVMHost on home server
- Encrypted connection over Internet

**Network Diagram:**
```
┌──────────────────┐                          ┌──────────────────┐
│  Remote Site     │                          │  Home Server     │
│                  │      Internet            │                  │
│  MMDVM-SDR       │◄────────────────────────►│  MMDVMHost       │
│                  │    (encrypted VPN)       │                  │
│  Public IP:      │                          │  Public IP:      │
│  203.0.113.50    │                          │  198.51.100.10   │
│                  │                          │                  │
│  VPN IP:         │                          │  VPN IP:         │
│  10.8.0.100      │                          │  10.8.0.1        │
│  Port: 3334      │                          │  Port: 3335      │
└──────────────────┘                          └──────────────────┘
```

**VPN Setup (Wireguard Example):**

**On Server (/etc/wireguard/wg0.conf):**
```ini
[Interface]
Address = 10.8.0.1/24
PrivateKey = <server_private_key>
ListenPort = 51820

[Peer]
# Remote MMDVM-SDR
PublicKey = <remote_public_key>
AllowedIPs = 10.8.0.100/32
```

**On Remote (/etc/wireguard/wg0.conf):**
```ini
[Interface]
Address = 10.8.0.100/24
PrivateKey = <remote_private_key>

[Peer]
# Home Server
PublicKey = <server_public_key>
Endpoint = 198.51.100.10:51820
AllowedIPs = 10.8.0.0/24
PersistentKeepalive = 25
```

**Start VPN:**
```bash
# On both machines
sudo wg-quick up wg0

# Verify
sudo wg show
ping 10.8.0.1  # From remote to server
ping 10.8.0.100  # From server to remote
```

**MMDVM-SDR Config (Remote):**
```cpp
#define UDP_MODEM_ADDRESS  "10.8.0.1"     // VPN address
#define UDP_MODEM_PORT     3335
#define UDP_LOCAL_ADDRESS  "10.8.0.100"   // VPN address
#define UDP_LOCAL_PORT     3334
```

**MMDVMHost MMDVM.ini (Server):**
```ini
[Modem]
Protocol=udp
ModemAddress=10.8.0.100    # Remote VPN IP
ModemPort=3334
LocalAddress=10.8.0.1      # Server VPN IP
LocalPort=3335
```

**Benefits:**
- Encrypted traffic (secure over Internet)
- Fixed VPN IPs (no DDNS needed)
- NAT traversal (works behind routers)

---

## Testing Strategy

### Unit Tests

**Test 1: UDP Socket Creation and Binding**

```cpp
// test_udpsocket.cpp
#include "UDPSocket.h"
#include <assert.h>

void test_socket_creation() {
    CUDPSocket socket("127.0.0.1", 3334);
    assert(socket.open() == true);
    socket.close();
    printf("PASS: Socket creation\n");
}

void test_socket_lookup() {
    sockaddr_storage addr;
    unsigned int addrLen;

    assert(CUDPSocket::lookup("127.0.0.1", 3334, addr, addrLen) == true);
    assert(CUDPSocket::lookup("localhost", 3334, addr, addrLen) == true);
    assert(CUDPSocket::lookup("invalid.invalid", 3334, addr, addrLen) == false);

    printf("PASS: Socket lookup\n");
}

void test_socket_match() {
    sockaddr_storage addr1, addr2;
    unsigned int addrLen;

    CUDPSocket::lookup("127.0.0.1", 3334, addr1, addrLen);
    CUDPSocket::lookup("127.0.0.1", 3334, addr2, addrLen);

    assert(CUDPSocket::match(addr1, addr2) == true);

    CUDPSocket::lookup("127.0.0.1", 3335, addr2, addrLen);
    assert(CUDPSocket::match(addr1, addr2) == false);

    printf("PASS: Socket matching\n");
}

int main() {
    test_socket_creation();
    test_socket_lookup();
    test_socket_match();

    printf("All UDP socket tests passed!\n");
    return 0;
}
```

**Test 2: UDP Modem Port I/O**

```cpp
// test_udpmodemport.cpp
#include "UDPModemPort.h"
#include <unistd.h>
#include <assert.h>

void test_loopback_communication() {
    // Create two UDP ports (simulate modem and host)
    CUDPModemPort modem("127.0.0.1", 3335, "127.0.0.1", 3334);
    CUDPModemPort host("127.0.0.1", 3334, "127.0.0.1", 3335);

    assert(modem.open() == true);
    assert(host.open() == true);

    // Send data from host to modem
    unsigned char txData[] = {0xE0, 0x04, 0x00, 0x00};  // GET_VERSION
    assert(host.write(txData, 4) == 4);

    usleep(10000);  // 10ms delay

    // Receive on modem
    unsigned char rxData[10];
    int len = modem.read(rxData, 10);
    assert(len == 4);
    assert(memcmp(rxData, txData, 4) == 0);

    modem.close();
    host.close();

    printf("PASS: Loopback communication\n");
}

void test_ring_buffer() {
    CUDPModemPort port("127.0.0.1", 3334, "127.0.0.1", 3335);
    port.open();

    // Send multiple packets quickly
    unsigned char data1[] = {0x01, 0x02, 0x03};
    unsigned char data2[] = {0x04, 0x05, 0x06};

    port.write(data1, 3);
    port.write(data2, 3);

    usleep(10000);

    // Read should return buffered data
    unsigned char rxData[10];
    int len = port.read(rxData, 10);
    assert(len >= 3);  // At least one packet buffered

    port.close();

    printf("PASS: Ring buffer\n");
}

int main() {
    test_loopback_communication();
    test_ring_buffer();

    printf("All UDP modem port tests passed!\n");
    return 0;
}
```

**Test 3: MMDVM Protocol Over UDP**

```cpp
// test_mmdvm_protocol.cpp
#include "UDPModemPort.h"
#include <assert.h>
#include <unistd.h>

void test_version_query() {
    CUDPModemPort modem("127.0.0.1", 3335, "127.0.0.1", 3334);
    CUDPModemPort host("127.0.0.1", 3334, "127.0.0.1", 3335);

    modem.open();
    host.open();

    // Host sends GET_VERSION
    unsigned char cmd[] = {0xE0, 0x03, 0x00};
    host.write(cmd, 3);

    usleep(10000);

    // Modem receives and should process
    unsigned char rxCmd[10];
    int len = modem.read(rxCmd, 10);
    assert(len == 3);
    assert(rxCmd[0] == 0xE0);  // Frame start
    assert(rxCmd[2] == 0x00);  // GET_VERSION command

    printf("PASS: MMDVM protocol transport\n");
}

int main() {
    test_version_query();
    printf("All MMDVM protocol tests passed!\n");
    return 0;
}
```

---

### Integration Tests

**Test 1: Localhost Integration**

```bash
#!/bin/bash
# test_localhost.sh

echo "Starting localhost integration test..."

# Kill any existing instances
killall mmdvm 2>/dev/null
killall MMDVMHost 2>/dev/null

# Start MMDVM-SDR in background
cd /path/to/mmdvm-sdr/build
./mmdvm &
MMDVM_PID=$!

# Wait for startup
sleep 2

# Start MMDVMHost
cd /path/to/MMDVMHost
./MMDVMHost MMDVM.ini &
HOST_PID=$!

# Monitor for 30 seconds
echo "Monitoring for 30 seconds..."
sleep 30

# Check if both are still running
if kill -0 $MMDVM_PID 2>/dev/null && kill -0 $HOST_PID 2>/dev/null; then
    echo "PASS: Both processes running"
else
    echo "FAIL: Process crashed"
    exit 1
fi

# Cleanup
kill $MMDVM_PID $HOST_PID

echo "Integration test complete!"
```

**Test 2: Network Capture Validation**

```bash
#!/bin/bash
# test_network_capture.sh

echo "Starting network capture test..."

# Start packet capture
sudo tcpdump -i lo -w /tmp/mmdvm_capture.pcap \
    'udp port 3334 or udp port 3335' &
TCPDUMP_PID=$!

# Run MMDVM-SDR and MMDVMHost for 60 seconds
./start_mmdvm.sh  # Your startup script

sleep 60

# Stop capture
sudo kill $TCPDUMP_PID

# Analyze capture
echo "Analyzing capture..."
sudo tcpdump -r /tmp/mmdvm_capture.pcap -X | head -100

# Check for MMDVM frame start (0xE0)
if sudo tcpdump -r /tmp/mmdvm_capture.pcap -X | grep -q "e0"; then
    echo "PASS: MMDVM protocol frames detected"
else
    echo "FAIL: No MMDVM frames found"
    exit 1
fi

echo "Network capture test complete!"
```

**Test 3: Error Recovery**

```bash
#!/bin/bash
# test_error_recovery.sh

echo "Testing error recovery..."

# Start MMDVM-SDR
./mmdvm &
MMDVM_PID=$!

sleep 2

# Start MMDVMHost
./MMDVMHost MMDVM.ini &
HOST_PID=$!

sleep 5

# Kill MMDVMHost (simulate crash)
echo "Simulating MMDVMHost crash..."
kill $HOST_PID

sleep 2

# Check if MMDVM-SDR is still running
if kill -0 $MMDVM_PID 2>/dev/null; then
    echo "PASS: MMDVM-SDR survived host crash"
else
    echo "FAIL: MMDVM-SDR crashed"
    exit 1
fi

# Restart MMDVMHost
./MMDVMHost MMDVM.ini &
HOST_PID=$!

sleep 5

# Check if reconnection successful
if kill -0 $HOST_PID 2>/dev/null; then
    echo "PASS: MMDVMHost reconnected"
else
    echo "FAIL: MMDVMHost failed to restart"
    exit 1
fi

# Cleanup
kill $MMDVM_PID $HOST_PID

echo "Error recovery test complete!"
```

---

### Performance Tests

**Test 1: Latency Measurement**

```python
#!/usr/bin/env python3
# test_latency.py

import socket
import time

def measure_latency():
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 4444))
    sock.settimeout(1.0)

    # MMDVM GET_STATUS frame
    status_cmd = bytes([0xE0, 0x03, 0x01])

    latencies = []

    for i in range(100):
        start = time.time()

        # Send to MMDVM-SDR
        sock.sendto(status_cmd, ('127.0.0.1', 3334))

        # Wait for response
        try:
            data, addr = sock.recvfrom(1024)
            end = time.time()

            latency = (end - start) * 1000  # Convert to ms
            latencies.append(latency)
        except socket.timeout:
            print(f"Timeout on iteration {i}")

        time.sleep(0.01)  # 10ms between tests

    # Statistics
    avg = sum(latencies) / len(latencies)
    min_lat = min(latencies)
    max_lat = max(latencies)

    print(f"Latency statistics (ms):")
    print(f"  Average: {avg:.2f}")
    print(f"  Min: {min_lat:.2f}")
    print(f"  Max: {max_lat:.2f}")

    if avg < 2.0:
        print("PASS: Latency within acceptable range")
    else:
        print("FAIL: Latency too high")

if __name__ == '__main__':
    measure_latency()
```

**Test 2: Throughput Measurement**

```python
#!/usr/bin/env python3
# test_throughput.py

import socket
import time

def measure_throughput():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 4445))

    # Simulate DMR traffic (50 frames/sec, 33 bytes each)
    dmr_frame = bytes([0xE0, 0x21, 0x1A] + [0x00]*30)

    frames_sent = 0
    start_time = time.time()

    # Send for 10 seconds
    while time.time() - start_time < 10.0:
        sock.sendto(dmr_frame, ('127.0.0.1', 3334))
        frames_sent += 1
        time.sleep(0.02)  # 50 fps

    end_time = time.time()
    duration = end_time - start_time

    # Calculate throughput
    bytes_sent = frames_sent * len(dmr_frame)
    throughput = bytes_sent / duration

    print(f"Throughput test:")
    print(f"  Frames sent: {frames_sent}")
    print(f"  Duration: {duration:.2f} sec")
    print(f"  Throughput: {throughput:.0f} bytes/sec ({throughput/1024:.2f} KB/sec)")

    expected = 50 * len(dmr_frame)  # 50 fps
    if throughput >= expected * 0.9:  # Allow 10% margin
        print("PASS: Throughput adequate")
    else:
        print("FAIL: Throughput too low")

if __name__ == '__main__':
    measure_throughput()
```

---

## Deployment Scenarios

### Scenario 1: Embedded Modem (Recommended)

**Hardware:**
- Raspberry Pi 4 or similar
- ADALM Pluto SDR
- Ethernet connection to network

**Software Stack:**
```
┌─────────────────────────────────────────┐
│ Raspberry Pi (192.168.1.100)            │
│                                         │
│ ┌─────────────────────────────────────┐ │
│ │ MMDVM-SDR (standalone mode)         │ │
│ │ - Direct PlutoSDR control (libiio) │ │
│ │ - FM modem + resampling             │ │
│ │ - UDP modem on port 3334            │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ ┌─────────────────────────────────────┐ │
│ │ PlutoSDR (USB attached)             │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
                 │ UDP
                 ▼
┌─────────────────────────────────────────┐
│ Linux Server (192.168.1.10)             │
│                                         │
│ ┌─────────────────────────────────────┐ │
│ │ MMDVMHost                           │ │
│ │ - UDP client to 192.168.1.100:3334 │ │
│ │ - Network gateways (DMR+, etc.)    │ │
│ │ - Web dashboard                     │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

**Benefits:**
- Dedicated modem hardware
- Network isolation possible
- Easy to monitor and debug
- Scales to multiple modems

**Deployment:**
```bash
# On Raspberry Pi
cd /home/pi/mmdvm-sdr
./build/mmdvm

# Autostart with systemd
sudo nano /etc/systemd/system/mmdvm-sdr.service
```

**mmdvm-sdr.service:**
```ini
[Unit]
Description=MMDVM-SDR Modem
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/mmdvm-sdr
ExecStart=/home/pi/mmdvm-sdr/build/mmdvm
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable mmdvm-sdr
sudo systemctl start mmdvm-sdr
sudo systemctl status mmdvm-sdr
```

---

### Scenario 2: All-in-One (Simple)

**Hardware:**
- Single Linux PC or Raspberry Pi
- USB SDR (PlutoSDR, LimeSDR, etc.)

**Software Stack:**
```
┌───────────────────────────────────────────────┐
│ Single Computer (localhost)                  │
│                                               │
│ ┌───────────────────────────────────────────┐ │
│ │ MMDVM-SDR                                 │ │
│ │ - Listens on 127.0.0.1:3334              │ │
│ │ - PlutoSDR via USB                        │ │
│ └─────────────┬─────────────────────────────┘ │
│               │ Loopback UDP                  │
│ ┌─────────────▼─────────────────────────────┐ │
│ │ MMDVMHost                                 │ │
│ │ - Connects to 127.0.0.1:3334             │ │
│ │ - Network connections                     │ │
│ └───────────────────────────────────────────┘ │
└───────────────────────────────────────────────┘
```

**Benefits:**
- Simplest deployment
- No network setup required
- Maximum security (localhost only)
- Good for testing and development

**Startup Script:**
```bash
#!/bin/bash
# start_mmdvm_all.sh

cd /opt/mmdvm-sdr/build
./mmdvm &
sleep 2

cd /opt/MMDVMHost
./MMDVMHost MMDVM.ini &

echo "MMDVM system started"
echo "Modem PID: $(pgrep mmdvm)"
echo "Host PID: $(pgrep MMDVMHost)"
```

---

### Scenario 3: Distributed (Advanced)

**Use Case:**
- Multiple remote modems
- Central MMDVMHost server
- VPN interconnection

**Architecture:**
```
┌──────────────┐        ┌──────────────┐
│ Remote 1     │        │ Remote 2     │
│ MMDVM-SDR    │        │ MMDVM-SDR    │
│ 10.8.0.100   │        │ 10.8.0.101   │
└──────┬───────┘        └──────┬───────┘
       │ VPN                   │ VPN
       │                       │
       └───────────┬───────────┘
                   │
                   ▼
         ┌──────────────────┐
         │ Central Server   │
         │ MMDVMHost Pool   │
         │ 10.8.0.1         │
         │                  │
         │ ┌──────────────┐ │
         │ │ MMDVMHost 1  │ │
         │ │ Port 3335    │ │
         │ └──────────────┘ │
         │ ┌──────────────┐ │
         │ │ MMDVMHost 2  │ │
         │ │ Port 3336    │ │
         │ └──────────────┘ │
         └──────────────────┘
```

**Configuration Matrix:**

| Modem | Local IP | Remote IP | Local Port | Remote Port |
|-------|----------|-----------|------------|-------------|
| Remote 1 | 10.8.0.100 | 10.8.0.1 | 3334 | 3335 |
| Remote 2 | 10.8.0.101 | 10.8.0.1 | 3334 | 3336 |

| Host | Local IP | Remote IP | Local Port | Remote Port |
|------|----------|-----------|------------|-------------|
| Host 1 | 10.8.0.1 | 10.8.0.100 | 3335 | 3334 |
| Host 2 | 10.8.0.1 | 10.8.0.101 | 3336 | 3334 |

**Benefits:**
- Centralized management
- Multiple simultaneous modems
- Geographic distribution
- Encrypted communication

---

## Troubleshooting Guide

### Common Issues

**1. "Failed to open UDP socket"**

**Symptoms:**
```
ERROR: Failed to open UDP socket
ERROR: bind: Address already in use
```

**Causes:**
- Port already in use by another process
- Insufficient permissions
- Invalid address

**Solutions:**
```bash
# Check if port is in use
sudo netstat -tulpn | grep 3334
sudo lsof -i :3334

# Kill process using port
sudo kill $(lsof -t -i:3334)

# Try different port
# Modify config to use 3340 instead of 3334

# Check permissions
# UDP ports <1024 require root, use ports >1024
```

---

**2. "No response from modem"**

**Symptoms:**
- MMDVMHost hangs on "Opening the modem..."
- No UDP traffic visible in tcpdump

**Causes:**
- MMDVM-SDR not started
- Wrong IP address/port
- Firewall blocking traffic
- Network routing issue

**Solutions:**
```bash
# 1. Verify MMDVM-SDR is running
ps aux | grep mmdvm
sudo netstat -tulpn | grep 3334  # Should show mmdvm process

# 2. Test network connectivity
ping 192.168.1.100  # Modem IP

# 3. Test UDP connectivity
nc -u 192.168.1.100 3334  # Should connect

# 4. Check firewall
sudo iptables -L -n | grep 3334
sudo ufw status

# 5. Verify configuration
# MMDVM-SDR config: LocalAddress=192.168.1.100, LocalPort=3334
# MMDVMHost MMDVM.ini: ModemAddress=192.168.1.100, ModemPort=3334

# 6. Monitor UDP traffic
sudo tcpdump -i any -vvv udp port 3334
# Should see packets when MMDVMHost tries to connect
```

---

**3. "Rejected UDP packet from unauthorized source"**

**Symptoms:**
```
WARNING: Rejected UDP packet from unauthorized source
```

**Causes:**
- Source address validation failing
- MMDVMHost using wrong source port
- NAT/firewall changing source address

**Solutions:**
```bash
# 1. Capture and inspect packets
sudo tcpdump -i any -vvv -X udp port 3334

# Look for source address in capture
# Example:
# 192.168.1.10.12345 > 192.168.1.100.3334
#               ^^^^^ Source port (ephemeral, OK)
# 192.168.1.10 <- Should match ModemAddress in MMDVM-SDR config

# 2. Check MMDVM-SDR expected address
# Config should have: UDP_MODEM_ADDRESS = "192.168.1.10"

# 3. Temporarily disable validation (debug only!)
# In UDPModemPort.cpp, comment out:
// if (!CUDPSocket::match(addr, m_modemAddr))
//     return 0;

# If packets now work, address mismatch confirmed
```

---

**4. "Buffer overflow, dropping data"**

**Symptoms:**
```
WARNING: UDP buffer overflow, dropping data
```

**Causes:**
- MMDVM-SDR processing too slow
- MMDVMHost sending too fast
- Ring buffer too small

**Solutions:**
```cpp
// Increase buffer size in UDPModemPort.cpp
#define BUFFER_SIZE 4000U  // Was 2000U

// Or optimize processing loop
void CSerialPort::process() {
    // Process multiple frames per call
    for (int i = 0; i < 10; i++) {
        if (!processOneFrame())
            break;
    }
}
```

---

**5. "High latency / Poor performance"**

**Symptoms:**
- Audio dropouts
- Delayed PTT response
- Network shows >10ms latency

**Causes:**
- WiFi interference
- Slow network
- CPU overload
- Swapping/low memory

**Solutions:**
```bash
# 1. Test network latency
ping -i 0.01 192.168.1.100  # 100 pps
# Should see <2ms average

# 2. Check CPU usage
top -p $(pgrep mmdvm)
# Should be <50% on modern CPU

# 3. Check memory
free -h
# Should have >100MB free

# 4. Switch to wired Ethernet
# WiFi has variable latency

# 5. Increase process priority
sudo nice -n -10 ./mmdvm  # Higher priority

# 6. Use localhost if possible
# Loopback is faster than network
```

---

### Debugging Tools

**1. tcpdump - Network Capture**

```bash
# Capture all MMDVM UDP traffic
sudo tcpdump -i any -w mmdvm.pcap 'udp port 3334 or udp port 3335'

# View capture
sudo tcpdump -r mmdvm.pcap -X

# Look for MMDVM frame start (0xE0)
sudo tcpdump -r mmdvm.pcap -X | grep -A 10 "e0"
```

**2. Wireshark - Detailed Analysis**

```bash
# Start Wireshark
sudo wireshark &

# Filter: udp.port == 3334 || udp.port == 3335

# Create custom MMDVM dissector (advanced)
# ~/.config/wireshark/plugins/mmdvm.lua
```

**3. netcat - Manual Testing**

```bash
# Listen on modem port (simulate MMDVM-SDR)
nc -u -l 3334

# Send to modem port (simulate MMDVMHost)
echo -ne '\xE0\x03\x00' | nc -u 192.168.1.100 3334

# Should see hex bytes on listener
```

**4. socat - Bidirectional Bridge**

```bash
# Bridge two UDP endpoints for testing
socat -v UDP-LISTEN:3334,fork UDP:192.168.1.100:3335

# -v shows all data (hex dump)
```

**5. strace - System Call Tracing**

```bash
# Trace MMDVM-SDR UDP calls
sudo strace -p $(pgrep mmdvm) -e trace=network,recvfrom,sendto

# Look for:
# recvfrom(socket, buffer, size, ...)  <- Reading UDP
# sendto(socket, buffer, size, ...)    <- Writing UDP
```

---

### Performance Monitoring

**1. Bandwidth Monitor**

```bash
#!/bin/bash
# monitor_bandwidth.sh

while true; do
    RX=$(sudo tcpdump -i any -c 100 'udp port 3334' 2>&1 | \
         grep -oP '\d+(?= bytes)' | awk '{s+=$1} END {print s}')

    echo "RX: $(($RX / 100)) bytes/sec"
    sleep 1
done
```

**2. Latency Monitor**

```python
#!/usr/bin/env python3
# monitor_latency.py

import socket
import time
import statistics

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', 4444))
sock.settimeout(0.5)

latencies = []

while True:
    try:
        start = time.time()
        sock.sendto(b'\xE0\x03\x01', ('192.168.1.100', 3334))
        data, addr = sock.recvfrom(1024)
        latency = (time.time() - start) * 1000

        latencies.append(latency)
        latencies = latencies[-100:]  # Keep last 100

        avg = statistics.mean(latencies)
        stddev = statistics.stdev(latencies) if len(latencies) > 1 else 0

        print(f"Latency: {latency:.2f}ms  Avg: {avg:.2f}ms  StdDev: {stddev:.2f}ms")

    except socket.timeout:
        print("Timeout!")

    time.sleep(0.25)
```

---

## Appendix: Complete Example Configuration

### MMDVM-SDR Configuration

**mmdvm.conf:**
```ini
[Modem]
# Transport: pty or udp
Transport=udp

# UDP Modem Configuration
UDP.ModemAddress=127.0.0.1
UDP.ModemPort=3335
UDP.LocalAddress=127.0.0.1
UDP.LocalPort=3334

[SDR]
Device=pluto
SampleRate=1000000
Frequency=435000000
TXGain=-20
RXGain=40

[Modes]
DMR=1
DStar=1
YSF=1
P25=1
NXDN=1
```

---

### MMDVMHost Configuration

**MMDVM.ini (Complete Example):**
```ini
[General]
Callsign=N0CALL
Id=1234567
Timeout=180
Duplex=1
ModeHang=10
RFModeHang=10
NetModeHang=3
Display=None
Daemon=0

[Info]
RXFrequency=435000000
TXFrequency=435000000
Power=1
Latitude=0.0
Longitude=0.0
Height=0
Location=Anywhere
Description=MMDVM-SDR UDP Test

[Log]
DisplayLevel=1
FileLevel=1
FilePath=.
FileRoot=MMDVMHost

[CW Id]
Enable=1
Time=10

[Modem]
# ***********************************
# UDP CONFIGURATION - CHANGE THESE!
# ***********************************
Protocol=udp
ModemAddress=127.0.0.1
ModemPort=3334
LocalAddress=127.0.0.1
LocalPort=3335

# Modem Parameters
TXInvert=1
RXInvert=1
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
Debug=1

[D-Star]
Enable=1
Module=C
SelfOnly=0
BlackList=
AckReply=1
AckTime=750
ErrorReply=1
RemoteGateway=0
ModeHang=10

[DMR]
Enable=1
Beacons=0
BeaconInterval=60
BeaconDuration=3
ColorCode=1
SelfOnly=0
EmbeddedLCOnly=0
DumpTAData=1
CallHang=3
TXHang=4
ModeHang=10

[System Fusion]
Enable=1
LowDeviation=0
RemoteGateway=0
SelfOnly=0
ModeHang=10

[P25]
Enable=1
NAC=293
SelfOnly=0
OverrideUIDCheck=0
RemoteGateway=0
ModeHang=10

[NXDN]
Enable=1
RAN=1
SelfOnly=0
RemoteGateway=0
ModeHang=10

[D-Star Network]
Enable=0
GatewayAddress=127.0.0.1
GatewayPort=20010
LocalPort=20011
ModeHang=3
Debug=0

[DMR Network]
Enable=1
Address=127.0.0.1
Port=62031
Jitter=360
Local=62032
Password=passw0rd
Slot1=1
Slot2=1
ModeHang=3
Debug=0

[YSF Network]
Enable=0
LocalAddress=127.0.0.1
LocalPort=3200
GatewayAddress=127.0.0.1
GatewayPort=4200
ModeHang=3
Debug=0

[P25 Network]
Enable=0
GatewayAddress=127.0.0.1
GatewayPort=42020
LocalPort=32010
ModeHang=3
Debug=0

[NXDN Network]
Enable=0
LocalAddress=127.0.0.1
LocalPort=33300
GatewayAddress=127.0.0.1
GatewayPort=33400
ModeHang=3
Debug=0
```

---

**End of UDP Integration Guide**

*For implementation details, see UDP_IMPLEMENTATION_PLAN.md*
