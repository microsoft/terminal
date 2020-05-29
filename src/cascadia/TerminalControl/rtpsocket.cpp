// Licensed under the MIT license.

#include "pch.h"
#include "rtpsocket.h"

#include <fstream>
#include <iterator>

namespace mjpeg
{
    RtpSocket::RtpSocket(int listenPort) :
        _listenPort(listenPort), _listenAddr(), _timeout()
    { }

    RtpSocket::~RtpSocket()
    {
        Close();
    }

    void RtpSocket::Start()
    {
        if (_closed)
        {
            std::cerr << "RTP socket cannot restart a closed socket." << std::endl;
            return;
        }

        WSADATA wsaData;

        // Initialize Winsock
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0)
        {
            std::cerr << "RTP socket WSAStartup failed with result " << iResult << "." << std::endl;
            return;
        }

        _rtpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_rtpSocket == INVALID_SOCKET)
        {
            std::cerr << "RTP socket creation failed with error " << WSAGetLastError() << std::endl;
            WSACleanup();
            return;
        }

        _listenAddr.sin_family = AF_INET;
        _listenAddr.sin_addr.s_addr = htonl(LISTEN_ADDRESS);
        _listenAddr.sin_port = htons((short)_listenPort);
        
        // Bind the socket.
        iResult = bind(_rtpSocket, (SOCKADDR*)&_listenAddr, sizeof(_listenAddr));
        if (iResult == SOCKET_ERROR)
        {
            std::cerr << "RTP socket bind failed with error " << WSAGetLastError() << std::endl;
            closesocket(_rtpSocket);
            WSACleanup();
            return;
        }

        _timeout.tv_sec = 0;
        _timeout.tv_usec = RECEIVE_TIMEOUT_MILLISECONDS * 1000;

        _receiveThread = std::make_unique<std::thread>(std::thread(&RtpSocket::Receive, this));
    }

    void RtpSocket::SetFrameReadyCallback(std::function<void(std::vector<uint8_t>&)> cb)
    {
        _cb = cb;
    }

    void RtpSocket::Receive()
    {
        std::vector<uint8_t> recvBuffer(RECEIVE_BUFFER_SIZE);

        struct sockaddr_in senderAddr;
        int senderAddrSize = sizeof(senderAddr);
        struct fd_set fds;
        std::vector<uint8_t> frame;

        while (!_closed)
        {
            FD_ZERO(&fds);
            FD_SET(_rtpSocket, &fds);

            // Return value:
            // -1: error occurred
            // 0: timed out
            // > 0: data ready to be read
            int selectResult = select(0, &fds, 0, 0, &_timeout);

            if (selectResult < 0)
            {
                std::cerr << "RTP socket select failed with error " << WSAGetLastError() << std::endl;
                break;
            }
            else if (selectResult > 0)
            {
                int bytesRead = recvfrom(_rtpSocket,
                                         (char*)recvBuffer.data(),
                                         (int)recvBuffer.size(),
                                         0,
                                         (SOCKADDR*)&senderAddr,
                                         &senderAddrSize);

                if (bytesRead == SOCKET_ERROR)
                {
                    int lastError = WSAGetLastError();
                    std::cerr << "RTP socket receive failed with error " << lastError << "." << std::endl;
                }
                else if (bytesRead > RtpHeader::RTP_MINIMUM_HEADER_LENGTH)
                {
                    RtpHeader rtpHeader;
                    int rtpHdrLen = rtpHeader.Deserialise(recvBuffer, 0);
                    if (rtpHdrLen <= 0)
                    {
                        // There was a problem parsing the RTP header. this packet will be ignored.
                        // If the packet was part of the stream it most likely means the current frame will be
                        // corrupted but if it's a transient error the next frame can recover.
                    }
                    else
                    {
                        JpegRtpHeader jpegHeader;
                        int jpegHdrLen = jpegHeader.Deserialise(recvBuffer, RtpHeader::RTP_MINIMUM_HEADER_LENGTH);
                        if (jpegHdrLen <= 0)
                        {
                            // There was a problem parsing the JPEG header. this packet will be ignored.
                            // If the packet was part of the stream it most likely means the current frame will be
                            // corrupted but if it's a transient error the next frame can recover.
                        }
                        else
                        {
                            if (jpegHeader.Offset == 0)
                            {
                                frame.clear();

                                // Add the JFIF header at the top of the frame.
                                mjpeg::Jfif jfif;
                                jfif.jpeg_create_header(frame, jpegHeader.Type, jpegHeader.Width, jpegHeader.Height, jpegHeader.QTable.data(), 1, 0);
                            }

                            int hdrLen = rtpHdrLen + jpegHdrLen;
                            if (bytesRead > hdrLen)
                            {
                                std::copy(recvBuffer.begin() + hdrLen, recvBuffer.begin() + bytesRead, std::back_inserter(frame));
                            }

                            if (rtpHeader.MarkerBit == 1)
                            {
                                // Write the JFIF end of data tag.
                                mjpeg::Jfif::jpeg_put_marker(frame, mjpeg::Jfif::JpegMarker::EOI);

                                if (_cb != nullptr)
                                {
                                    _cb(frame);
                                }

                                frame.clear();
                            }
                        }
                    }
                }
            }
        }

        closesocket(_rtpSocket);
        WSACleanup();
    }

    void RtpSocket::Close()
    {
        _closed = true;

        if (_receiveThread != nullptr)
        {
            _receiveThread->join();
        }
    }
}
