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
