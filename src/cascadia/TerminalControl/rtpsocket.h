//-----------------------------------------------------------------------------
// Filename: RtpSocket.h
//
// Description: Minimal implementation of a UDP socket to receive
// RTP packets.
//
// Author(s):
// Aaron Clauson (aaron@sipsorcery.com)
//
// History:
// 26 May 2020	Aaron Clauson	  Created, Dublin, Ireland.
//
// License and Attributions:
// Licensed under the MIT license.
//-----------------------------------------------------------------------------

#ifndef RTPSOCKET_H
#define RTPSOCKET_H

#include "mjpeg.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <chrono>
#include <ctime>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

// Change to INADDR_ANY to listen on all local IPv4 interfaces.
#define LISTEN_ADDRESS INADDR_LOOPBACK

namespace mjpeg
{
    class RtpSocket
    {
    public:
        /**
    * Attempts to create a new instance of the RTP socket and bind to the loopback address
    * and specified port.
    * @param[in] listenPort: the UDP port to listen on.
    */
        RtpSocket(int listenPort);

        /**
    * Default destructor. Main tasks are to close the socket if it was created and
    * signal the receive thread to stop if it is running.
    */
        ~RtpSocket();

        /**
    * Sets a function to call whenever a full JPEG frame is ready.
    * @param[in] cb: the callback function pointer.
    */
        void SetFrameReadyCallback(std::function<void(std::vector<uint8_t>&)> cb);

        /**
    * Starts the receive thread to monitor the socket for incoming messages.
    */
        void Start();

        /**
    * Closes the socket and shutsdown the receive thread. Once closed the socket cannot be
    * restarted.
    */
        void Close();

    private:
        const int RECEIVE_TIMEOUT_MILLISECONDS = 70;
        const int RECEIVE_BUFFER_SIZE = 2048;

        int _listenPort;
        bool _closed{ false };
        SOCKET _rtpSocket{ INVALID_SOCKET };
        struct sockaddr_in _listenAddr;
        struct timeval _timeout;
        std::unique_ptr<std::thread> _receiveThread{ nullptr };
        std::function<void(std::vector<uint8_t>&)> _cb{ nullptr };

        void Receive();
    };
}

#endif // RTPSOCKET_H
