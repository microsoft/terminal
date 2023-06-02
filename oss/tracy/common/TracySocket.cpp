#include <assert.h>
#include <inttypes.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "TracyAlloc.hpp"
#include "TracySocket.hpp"
#include "TracySystem.hpp"

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  ifdef _MSC_VER
#    pragma warning(disable:4244)
#    pragma warning(disable:4267)
#  endif
#  define poll WSAPoll
#else
#  include <arpa/inet.h>
#  include <sys/socket.h>
#  include <sys/param.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <poll.h>
#endif

#ifndef MSG_NOSIGNAL
#  define MSG_NOSIGNAL 0
#endif

namespace tracy
{

#ifdef _WIN32
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

#ifdef _WIN32
struct __wsinit
{
    __wsinit()
    {
        WSADATA wsaData;
        if( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 )
        {
            fprintf( stderr, "Cannot init winsock.\n" );
            exit( 1 );
        }
    }
};

void InitWinSock()
{
    static __wsinit init;
}
#endif


enum { BufSize = 128 * 1024 };

Socket::Socket()
    : m_buf( (char*)tracy_malloc( BufSize ) )
    , m_bufPtr( nullptr )
    , m_sock( -1 )
    , m_bufLeft( 0 )
    , m_ptr( nullptr )
{
#ifdef _WIN32
    InitWinSock();
#endif
}

Socket::Socket( int sock )
    : m_buf( (char*)tracy_malloc( BufSize ) )
    , m_bufPtr( nullptr )
    , m_sock( sock )
    , m_bufLeft( 0 )
    , m_ptr( nullptr )
{
}

Socket::~Socket()
{
    tracy_free( m_buf );
    if( m_sock.load( std::memory_order_relaxed ) != -1 )
    {
        Close();
    }
    if( m_ptr )
    {
        freeaddrinfo( m_res );
#ifdef _WIN32
        closesocket( m_connSock );
#else
        close( m_connSock );
#endif
    }
}

bool Socket::Connect( const char* addr, uint16_t port )
{
    assert( !IsValid() );

    if( m_ptr )
    {
        const auto c = connect( m_connSock, m_ptr->ai_addr, m_ptr->ai_addrlen );
        if( c == -1 )
        {
#if defined _WIN32
            const auto err = WSAGetLastError();
            if( err == WSAEALREADY || err == WSAEINPROGRESS ) return false;
            if( err != WSAEISCONN )
            {
                freeaddrinfo( m_res );
                closesocket( m_connSock );
                m_ptr = nullptr;
                return false;
            }
#else
            const auto err = errno;
            if( err == EALREADY || err == EINPROGRESS ) return false;
            if( err != EISCONN )
            {
                freeaddrinfo( m_res );
                close( m_connSock );
                m_ptr = nullptr;
                return false;
            }
#endif
        }

#if defined _WIN32
        u_long nonblocking = 0;
        ioctlsocket( m_connSock, FIONBIO, &nonblocking );
#else
        int flags = fcntl( m_connSock, F_GETFL, 0 );
        fcntl( m_connSock, F_SETFL, flags & ~O_NONBLOCK );
#endif
        m_sock.store( m_connSock, std::memory_order_relaxed );
        freeaddrinfo( m_res );
        m_ptr = nullptr;
        return true;
    }

    struct addrinfo hints;
    struct addrinfo *res, *ptr;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char portbuf[32];
    sprintf( portbuf, "%" PRIu16, port );

    if( getaddrinfo( addr, portbuf, &hints, &res ) != 0 ) return false;
    int sock = 0;
    for( ptr = res; ptr; ptr = ptr->ai_next )
    {
        if( ( sock = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol ) ) == -1 ) continue;
#if defined __APPLE__
        int val = 1;
        setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof( val ) );
#endif
#if defined _WIN32
        u_long nonblocking = 1;
        ioctlsocket( sock, FIONBIO, &nonblocking );
#else
        int flags = fcntl( sock, F_GETFL, 0 );
        fcntl( sock, F_SETFL, flags | O_NONBLOCK );
#endif
        if( connect( sock, ptr->ai_addr, ptr->ai_addrlen ) == 0 )
        {
            break;
        }
        else
        {
#if defined _WIN32
            const auto err = WSAGetLastError();
            if( err != WSAEWOULDBLOCK )
            {
                closesocket( sock );
                continue;
            }
#else
            if( errno != EINPROGRESS )
            {
                close( sock );
                continue;
            }
#endif
        }
        m_res = res;
        m_ptr = ptr;
        m_connSock = sock;
        return false;
    }
    freeaddrinfo( res );
    if( !ptr ) return false;

#if defined _WIN32
    u_long nonblocking = 0;
    ioctlsocket( sock, FIONBIO, &nonblocking );
#else
    int flags = fcntl( sock, F_GETFL, 0 );
    fcntl( sock, F_SETFL, flags & ~O_NONBLOCK );
#endif

    m_sock.store( sock, std::memory_order_relaxed );
    return true;
}

bool Socket::ConnectBlocking( const char* addr, uint16_t port )
{
    assert( !IsValid() );
    assert( !m_ptr );

    struct addrinfo hints;
    struct addrinfo *res, *ptr;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char portbuf[32];
    sprintf( portbuf, "%" PRIu16, port );

    if( getaddrinfo( addr, portbuf, &hints, &res ) != 0 ) return false;
    int sock = 0;
    for( ptr = res; ptr; ptr = ptr->ai_next )
    {
        if( ( sock = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol ) ) == -1 ) continue;
#if defined __APPLE__
        int val = 1;
        setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof( val ) );
#endif
        if( connect( sock, ptr->ai_addr, ptr->ai_addrlen ) == -1 )
        {
#ifdef _WIN32
            closesocket( sock );
#else
            close( sock );
#endif
            continue;
        }
        break;
    }
    freeaddrinfo( res );
    if( !ptr ) return false;

    m_sock.store( sock, std::memory_order_relaxed );
    return true;
}

void Socket::Close()
{
    const auto sock = m_sock.load( std::memory_order_relaxed );
    assert( sock != -1 );
#ifdef _WIN32
    closesocket( sock );
#else
    close( sock );
#endif
    m_sock.store( -1, std::memory_order_relaxed );
}

int Socket::Send( const void* _buf, int len )
{
    const auto sock = m_sock.load( std::memory_order_relaxed );
    auto buf = (const char*)_buf;
    assert( sock != -1 );
    auto start = buf;
    while( len > 0 )
    {
        auto ret = send( sock, buf, len, MSG_NOSIGNAL );
        if( ret == -1 ) return -1;
        len -= ret;
        buf += ret;
    }
    return int( buf - start );
}

int Socket::GetSendBufSize()
{
    const auto sock = m_sock.load( std::memory_order_relaxed );
    int bufSize;
#if defined _WIN32
    int sz = sizeof( bufSize );
    getsockopt( sock, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, &sz );
#else
    socklen_t sz = sizeof( bufSize );
    getsockopt( sock, SOL_SOCKET, SO_SNDBUF, &bufSize, &sz );
#endif
    return bufSize;
}

int Socket::RecvBuffered( void* buf, int len, int timeout )
{
    if( len <= m_bufLeft )
    {
        memcpy( buf, m_bufPtr, len );
        m_bufPtr += len;
        m_bufLeft -= len;
        return len;
    }

    if( m_bufLeft > 0 )
    {
        memcpy( buf, m_bufPtr, m_bufLeft );
        const auto ret = m_bufLeft;
        m_bufLeft = 0;
        return ret;
    }

    if( len >= BufSize ) return Recv( buf, len, timeout );

    m_bufLeft = Recv( m_buf, BufSize, timeout );
    if( m_bufLeft <= 0 ) return m_bufLeft;

    const auto sz = len < m_bufLeft ? len : m_bufLeft;
    memcpy( buf, m_buf, sz );
    m_bufPtr = m_buf + sz;
    m_bufLeft -= sz;
    return sz;
}

int Socket::Recv( void* _buf, int len, int timeout )
{
    const auto sock = m_sock.load( std::memory_order_relaxed );
    auto buf = (char*)_buf;

    struct pollfd fd;
    fd.fd = (socket_t)sock;
    fd.events = POLLIN;

    if( poll( &fd, 1, timeout ) > 0 )
    {
        return recv( sock, buf, len, 0 );
    }
    else
    {
        return -1;
    }
}

int Socket::ReadUpTo( void* _buf, int len, int timeout )
{
    const auto sock = m_sock.load( std::memory_order_relaxed );
    auto buf = (char*)_buf;

    int rd = 0;
    while( len > 0 )
    {
        const auto res = recv( sock, buf, len, 0 );
        if( res == 0 ) break;
        if( res == -1 ) return -1;
        len -= res;
        rd += res;
        buf += res;
    }
    return rd;
}

bool Socket::Read( void* buf, int len, int timeout )
{
    auto cbuf = (char*)buf;
    while( len > 0 )
    {
        if( !ReadImpl( cbuf, len, timeout ) ) return false;
    }
    return true;
}

bool Socket::ReadImpl( char*& buf, int& len, int timeout )
{
    const auto sz = RecvBuffered( buf, len, timeout );
    switch( sz )
    {
    case 0:
        return false;
    case -1:
#ifdef _WIN32
    {
        auto err = WSAGetLastError();
        if( err == WSAECONNABORTED || err == WSAECONNRESET ) return false;
    }
#endif
    break;
    default:
        len -= sz;
        buf += sz;
        break;
    }
    return true;
}

bool Socket::ReadRaw( void* _buf, int len, int timeout )
{
    auto buf = (char*)_buf;
    while( len > 0 )
    {
        const auto sz = Recv( buf, len, timeout );
        if( sz <= 0 ) return false;
        len -= sz;
        buf += sz;
    }
    return true;
}

bool Socket::HasData()
{
    const auto sock = m_sock.load( std::memory_order_relaxed );
    if( m_bufLeft > 0 ) return true;

    struct pollfd fd;
    fd.fd = (socket_t)sock;
    fd.events = POLLIN;

    return poll( &fd, 1, 0 ) > 0;
}

bool Socket::IsValid() const
{
    return m_sock.load( std::memory_order_relaxed ) >= 0;
}


ListenSocket::ListenSocket()
    : m_sock( -1 )
{
#ifdef _WIN32
    InitWinSock();
#endif
}

ListenSocket::~ListenSocket()
{
    if( m_sock != -1 ) Close();
}

static int addrinfo_and_socket_for_family( uint16_t port, int ai_family, struct addrinfo** res )
{
    struct addrinfo hints;
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = ai_family;
    hints.ai_socktype = SOCK_STREAM;
#ifndef TRACY_ONLY_LOCALHOST
    const char* onlyLocalhost = GetEnvVar( "TRACY_ONLY_LOCALHOST" );
    if( !onlyLocalhost || onlyLocalhost[0] != '1' )
    {
        hints.ai_flags = AI_PASSIVE;
    }
#endif
    char portbuf[32];
    sprintf( portbuf, "%" PRIu16, port );
    if( getaddrinfo( nullptr, portbuf, &hints, res ) != 0 ) return -1;
    int sock = socket( (*res)->ai_family, (*res)->ai_socktype, (*res)->ai_protocol );
    if (sock == -1) freeaddrinfo( *res );
    return sock;
}

bool ListenSocket::Listen( uint16_t port, int backlog )
{
    assert( m_sock == -1 );

    struct addrinfo* res = nullptr;

#if !defined TRACY_ONLY_IPV4 && !defined TRACY_ONLY_LOCALHOST
    const char* onlyIPv4 = GetEnvVar( "TRACY_ONLY_IPV4" );
    if( !onlyIPv4 || onlyIPv4[0] != '1' )
    {
        m_sock = addrinfo_and_socket_for_family( port, AF_INET6, &res );
    }
#endif
    if (m_sock == -1)
    {
        // IPV6 protocol may not be available/is disabled. Try to create a socket
        // with the IPV4 protocol
        m_sock = addrinfo_and_socket_for_family( port, AF_INET, &res );
        if( m_sock == -1 ) return false;
    }
#if defined _WIN32
    unsigned long val = 0;
    setsockopt( m_sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&val, sizeof( val ) );
#elif defined BSD
    int val = 0;
    setsockopt( m_sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&val, sizeof( val ) );
    val = 1;
    setsockopt( m_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof( val ) );
#else
    int val = 1;
    setsockopt( m_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof( val ) );
#endif
    if( bind( m_sock, res->ai_addr, res->ai_addrlen ) == -1 ) { freeaddrinfo( res ); Close(); return false; }
    if( listen( m_sock, backlog ) == -1 ) { freeaddrinfo( res ); Close(); return false; }
    freeaddrinfo( res );
    return true;
}

Socket* ListenSocket::Accept()
{
    struct sockaddr_storage remote;
    socklen_t sz = sizeof( remote );

    struct pollfd fd;
    fd.fd = (socket_t)m_sock;
    fd.events = POLLIN;

    if( poll( &fd, 1, 10 ) > 0 )
    {
        int sock = accept( m_sock, (sockaddr*)&remote, &sz);
        if( sock == -1 ) return nullptr;

#if defined __APPLE__
        int val = 1;
        setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof( val ) );
#endif

        auto ptr = (Socket*)tracy_malloc( sizeof( Socket ) );
        new(ptr) Socket( sock );
        return ptr;
    }
    else
    {
        return nullptr;
    }
}

void ListenSocket::Close()
{
    assert( m_sock != -1 );
#ifdef _WIN32
    closesocket( m_sock );
#else
    close( m_sock );
#endif
    m_sock = -1;
}

UdpBroadcast::UdpBroadcast()
    : m_sock( -1 )
{
#ifdef _WIN32
    InitWinSock();
#endif
}

UdpBroadcast::~UdpBroadcast()
{
    if( m_sock != -1 ) Close();
}

bool UdpBroadcast::Open( const char* addr, uint16_t port )
{
    assert( m_sock == -1 );

    struct addrinfo hints;
    struct addrinfo *res, *ptr;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    char portbuf[32];
    sprintf( portbuf, "%" PRIu16, port );

    if( getaddrinfo( addr, portbuf, &hints, &res ) != 0 ) return false;
    int sock = 0;
    for( ptr = res; ptr; ptr = ptr->ai_next )
    {
        if( ( sock = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol ) ) == -1 ) continue;
#if defined __APPLE__
        int val = 1;
        setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof( val ) );
#endif
#if defined _WIN32
        unsigned long broadcast = 1;
        if( setsockopt( sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof( broadcast ) ) == -1 )
#else
        int broadcast = 1;
        if( setsockopt( sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof( broadcast ) ) == -1 )
#endif
        {
#ifdef _WIN32
            closesocket( sock );
#else
            close( sock );
#endif
            continue;
        }
        break;
    }
    freeaddrinfo( res );
    if( !ptr ) return false;

    m_sock = sock;
    inet_pton( AF_INET, addr, &m_addr );
    return true;
}

void UdpBroadcast::Close()
{
    assert( m_sock != -1 );
#ifdef _WIN32
    closesocket( m_sock );
#else
    close( m_sock );
#endif
    m_sock = -1;
}

int UdpBroadcast::Send( uint16_t port, const void* data, int len )
{
    assert( m_sock != -1 );
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons( port );
    addr.sin_addr.s_addr = m_addr;
    return sendto( m_sock, (const char*)data, len, MSG_NOSIGNAL, (sockaddr*)&addr, sizeof( addr ) );
}

IpAddress::IpAddress()
    : m_number( 0 )
{
    *m_text = '\0';
}

IpAddress::~IpAddress()
{
}

void IpAddress::Set( const struct sockaddr& addr )
{
#if defined _WIN32 && ( !defined NTDDI_WIN10 || NTDDI_VERSION < NTDDI_WIN10 )
    struct sockaddr_in tmp;
    memcpy( &tmp, &addr, sizeof( tmp ) );
    auto ai = &tmp;
#else
    auto ai = (const struct sockaddr_in*)&addr;
#endif
    inet_ntop( AF_INET, &ai->sin_addr, m_text, 17 );
    m_number = ai->sin_addr.s_addr;
}

UdpListen::UdpListen()
    : m_sock( -1 )
{
#ifdef _WIN32
    InitWinSock();
#endif
}

UdpListen::~UdpListen()
{
    if( m_sock != -1 ) Close();
}

bool UdpListen::Listen( uint16_t port )
{
    assert( m_sock == -1 );

    int sock;
    if( ( sock = socket( AF_INET, SOCK_DGRAM, 0 ) ) == -1 ) return false;

#if defined __APPLE__
    int val = 1;
    setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof( val ) );
#endif
#if defined _WIN32
    unsigned long reuse = 1;
    setsockopt( m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof( reuse ) );
#else
    int reuse = 1;
    setsockopt( m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
#endif
#if defined _WIN32
    unsigned long broadcast = 1;
    if( setsockopt( sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof( broadcast ) ) == -1 )
#else
    int broadcast = 1;
    if( setsockopt( sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof( broadcast ) ) == -1 )
#endif
    {
#ifdef _WIN32
        closesocket( sock );
#else
        close( sock );
#endif
        return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons( port );
    addr.sin_addr.s_addr = INADDR_ANY;

    if( bind( sock, (sockaddr*)&addr, sizeof( addr ) ) == -1 )
    {
#ifdef _WIN32
        closesocket( sock );
#else
        close( sock );
#endif
        return false;
    }

    m_sock = sock;
    return true;
}

void UdpListen::Close()
{
    assert( m_sock != -1 );
#ifdef _WIN32
    closesocket( m_sock );
#else
    close( m_sock );
#endif
    m_sock = -1;
}

const char* UdpListen::Read( size_t& len, IpAddress& addr, int timeout )
{
    static char buf[2048];

    struct pollfd fd;
    fd.fd = (socket_t)m_sock;
    fd.events = POLLIN;
    if( poll( &fd, 1, timeout ) <= 0 ) return nullptr;

    sockaddr sa;
    socklen_t salen = sizeof( struct sockaddr );
    len = (size_t)recvfrom( m_sock, buf, 2048, 0, &sa, &salen );
    addr.Set( sa );

    return buf;
}

}
