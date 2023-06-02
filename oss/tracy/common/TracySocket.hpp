#ifndef __TRACYSOCKET_HPP__
#define __TRACYSOCKET_HPP__

#include <atomic>
#include <stddef.h>
#include <stdint.h>

struct addrinfo;
struct sockaddr;

namespace tracy
{

#ifdef _WIN32
void InitWinSock();
#endif

class Socket
{
public:
    Socket();
    Socket( int sock );
    ~Socket();

    bool Connect( const char* addr, uint16_t port );
    bool ConnectBlocking( const char* addr, uint16_t port );
    void Close();

    int Send( const void* buf, int len );
    int GetSendBufSize();

    int ReadUpTo( void* buf, int len, int timeout );
    bool Read( void* buf, int len, int timeout );

    template<typename ShouldExit>
    bool Read( void* buf, int len, int timeout, ShouldExit exitCb )
    {
        auto cbuf = (char*)buf;
        while( len > 0 )
        {
            if( exitCb() ) return false;
            if( !ReadImpl( cbuf, len, timeout ) ) return false;
        }
        return true;
    }

    bool ReadRaw( void* buf, int len, int timeout );
    bool HasData();
    bool IsValid() const;

    Socket( const Socket& ) = delete;
    Socket( Socket&& ) = delete;
    Socket& operator=( const Socket& ) = delete;
    Socket& operator=( Socket&& ) = delete;

private:
    int RecvBuffered( void* buf, int len, int timeout );
    int Recv( void* buf, int len, int timeout );

    bool ReadImpl( char*& buf, int& len, int timeout );

    char* m_buf;
    char* m_bufPtr;
    std::atomic<int> m_sock;
    int m_bufLeft;

    struct addrinfo *m_res;
    struct addrinfo *m_ptr;
    int m_connSock;
};

class ListenSocket
{
public:
    ListenSocket();
    ~ListenSocket();

    bool Listen( uint16_t port, int backlog );
    Socket* Accept();
    void Close();

    ListenSocket( const ListenSocket& ) = delete;
    ListenSocket( ListenSocket&& ) = delete;
    ListenSocket& operator=( const ListenSocket& ) = delete;
    ListenSocket& operator=( ListenSocket&& ) = delete;

private:
    int m_sock;
};

class UdpBroadcast
{
public:
    UdpBroadcast();
    ~UdpBroadcast();

    bool Open( const char* addr, uint16_t port );
    void Close();

    int Send( uint16_t port, const void* data, int len );

    UdpBroadcast( const UdpBroadcast& ) = delete;
    UdpBroadcast( UdpBroadcast&& ) = delete;
    UdpBroadcast& operator=( const UdpBroadcast& ) = delete;
    UdpBroadcast& operator=( UdpBroadcast&& ) = delete;

private:
    int m_sock;
    uint32_t m_addr;
};

class IpAddress
{
public:
    IpAddress();
    ~IpAddress();

    void Set( const struct sockaddr& addr );

    uint32_t GetNumber() const { return m_number; }
    const char* GetText() const { return m_text; }

    IpAddress( const IpAddress& ) = delete;
    IpAddress( IpAddress&& ) = delete;
    IpAddress& operator=( const IpAddress& ) = delete;
    IpAddress& operator=( IpAddress&& ) = delete;

private:
    uint32_t m_number;
    char m_text[17];
};

class UdpListen
{
public:
    UdpListen();
    ~UdpListen();

    bool Listen( uint16_t port );
    void Close();

    const char* Read( size_t& len, IpAddress& addr, int timeout );

    UdpListen( const UdpListen& ) = delete;
    UdpListen( UdpListen&& ) = delete;
    UdpListen& operator=( const UdpListen& ) = delete;
    UdpListen& operator=( UdpListen&& ) = delete;

private:
    int m_sock;
};

}

#endif
