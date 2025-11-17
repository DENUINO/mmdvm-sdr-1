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
