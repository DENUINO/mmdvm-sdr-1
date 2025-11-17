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
