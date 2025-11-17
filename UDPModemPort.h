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
