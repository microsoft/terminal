#ifdef _MSC_VER
#  pragma warning(disable:4996)
#endif
#if defined _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <malloc.h>
#  include "TracyUwp.hpp"
#else
#  include <pthread.h>
#  include <string.h>
#  include <unistd.h>
#endif

#ifdef __linux__
#  ifdef __ANDROID__
#    include <sys/types.h>
#  else
#    include <sys/syscall.h>
#  endif
#  include <fcntl.h>
#elif defined __FreeBSD__
#  include <sys/thr.h>
#elif defined __NetBSD__ || defined __DragonFly__
#  include <sys/lwp.h>
#endif

#ifdef __MINGW32__
#  define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "TracySystem.hpp"

#if defined _WIN32
extern "C" typedef HRESULT (WINAPI *t_SetThreadDescription)( HANDLE, PCWSTR );
extern "C" typedef HRESULT (WINAPI *t_GetThreadDescription)( HANDLE, PWSTR* );
#endif

#ifdef TRACY_ENABLE
#  include <atomic>
#  include "TracyAlloc.hpp"
#endif

namespace tracy
{

namespace detail
{

TRACY_API uint32_t GetThreadHandleImpl()
{
#if defined _WIN32
    static_assert( sizeof( decltype( GetCurrentThreadId() ) ) <= sizeof( uint32_t ), "Thread handle too big to fit in protocol" );
    return uint32_t( GetCurrentThreadId() );
#elif defined __APPLE__
    uint64_t id;
    pthread_threadid_np( pthread_self(), &id );
    return uint32_t( id );
#elif defined __ANDROID__
    return (uint32_t)gettid();
#elif defined __linux__
    return (uint32_t)syscall( SYS_gettid );
#elif defined __FreeBSD__
    long id;
    thr_self( &id );
    return id;
#elif defined __NetBSD__
    return _lwp_self();
#elif defined __DragonFly__
    return lwp_gettid();
#elif defined __OpenBSD__
    return getthrid();
#elif defined __EMSCRIPTEN__
    // Not supported, but let it compile.
    return 0;
#else
    // To add support for a platform, retrieve and return the kernel thread identifier here.
    //
    // Note that pthread_t (as for example returned by pthread_self()) is *not* a kernel
    // thread identifier. It is a pointer to a library-allocated data structure instead.
    // Such pointers will be reused heavily, making the pthread_t non-unique. Additionally
    // a 64-bit pointer cannot be reliably truncated to 32 bits.
    #error "Unsupported platform!"
#endif

}

}

#ifdef TRACY_ENABLE
struct ThreadNameData
{
    uint32_t id;
    const char* name;
    ThreadNameData* next;
};
std::atomic<ThreadNameData*>& GetThreadNameData();
#endif

#ifdef _MSC_VER
#  pragma pack( push, 8 )
struct THREADNAME_INFO
{
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
};
#  pragma pack( pop )

void ThreadNameMsvcMagic( const THREADNAME_INFO& info )
{
    __try
    {
        RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
}
#endif

TRACY_API void SetThreadName( const char* name )
{
#if defined _WIN32
#  ifdef TRACY_UWP
    static auto _SetThreadDescription = &::SetThreadDescription;
#  else
    static auto _SetThreadDescription = (t_SetThreadDescription)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "SetThreadDescription" );
#  endif
    if( _SetThreadDescription )
    {
        wchar_t buf[256];
        mbstowcs( buf, name, 256 );
        _SetThreadDescription( GetCurrentThread(), buf );
    }
    else
    {
#  if defined _MSC_VER
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name;
        info.dwThreadID = GetCurrentThreadId();
        info.dwFlags = 0;
        ThreadNameMsvcMagic( info );
#  endif
    }
#elif defined _GNU_SOURCE && !defined __EMSCRIPTEN__
    {
        const auto sz = strlen( name );
        if( sz <= 15 )
        {
#if defined __APPLE__
            pthread_setname_np( name );
#else
            pthread_setname_np( pthread_self(), name );
#endif
        }
        else
        {
            char buf[16];
            memcpy( buf, name, 15 );
            buf[15] = '\0';
#if defined __APPLE__
            pthread_setname_np( buf );
#else
            pthread_setname_np( pthread_self(), buf );
#endif
        }
    }
#endif
#ifdef TRACY_ENABLE
    {
        const auto sz = strlen( name );
        char* buf = (char*)tracy_malloc( sz+1 );
        memcpy( buf, name, sz );
        buf[sz] = '\0';
        auto data = (ThreadNameData*)tracy_malloc_fast( sizeof( ThreadNameData ) );
        data->id = detail::GetThreadHandleImpl();
        data->name = buf;
        data->next = GetThreadNameData().load( std::memory_order_relaxed );
        while( !GetThreadNameData().compare_exchange_weak( data->next, data, std::memory_order_release, std::memory_order_relaxed ) ) {}
    }
#endif
}

TRACY_API const char* GetThreadName( uint32_t id )
{
    static char buf[256];
#ifdef TRACY_ENABLE
    auto ptr = GetThreadNameData().load( std::memory_order_relaxed );
    while( ptr )
    {
        if( ptr->id == id )
        {
            return ptr->name;
        }
        ptr = ptr->next;
    }
#endif

#if defined _WIN32
# ifdef TRACY_UWP
   static auto _GetThreadDescription = &::GetThreadDescription;
# else
   static auto _GetThreadDescription = (t_GetThreadDescription)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "GetThreadDescription" );
# endif
  if( _GetThreadDescription )
  {
      auto hnd = OpenThread( THREAD_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)id );
      if( hnd != 0 )
      {
          PWSTR tmp;
          _GetThreadDescription( hnd, &tmp );
          auto ret = wcstombs( buf, tmp, 256 );
          CloseHandle( hnd );
          if( ret != 0 )
          {
              return buf;
          }
      }
  }
#elif defined __linux__
  int cs, fd;
  char path[32];
  snprintf( path, sizeof( path ), "/proc/self/task/%d/comm", id );
  sprintf( buf, "%" PRIu32, id );
# ifndef __ANDROID__
   pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs );
# endif
  if ( ( fd = open( path, O_RDONLY ) ) > 0) {
      int len = read( fd, buf, 255 );
      if( len > 0 )
      {
          buf[len] = 0;
          if( len > 1 && buf[len-1] == '\n' )
          {
              buf[len-1] = 0;
          }
      }
      close( fd );
  }
# ifndef __ANDROID__
   pthread_setcancelstate( cs, 0 );
# endif
  return buf;
#endif

  sprintf( buf, "%" PRIu32, id );
  return buf;
}

TRACY_API const char* GetEnvVar( const char* name )
{
#if defined _WIN32
    // unfortunately getenv() on Windows is just fundamentally broken.  It caches the entire
    // environment block once on startup, then never refreshes it again.  If any environment
    // strings are added or modified after startup of the CRT, those changes will not be
    // seen by getenv().  This removes the possibility of an app using this SDK from
    // programmatically setting any of the behaviour controlling envvars here.
    //
    // To work around this, we'll instead go directly to the Win32 environment strings APIs
    // to get the current value.
    static char buffer[1024];
    DWORD const kBufferSize = DWORD(sizeof(buffer) / sizeof(buffer[0]));
    DWORD count = GetEnvironmentVariableA(name, buffer, kBufferSize);

    if( count == 0 )
        return nullptr;

    if( count >= kBufferSize )
    {
        char* buf = reinterpret_cast<char*>(_alloca(count + 1));
        count = GetEnvironmentVariableA(name, buf, count + 1);
        memcpy(buffer, buf, kBufferSize);
        buffer[kBufferSize - 1] = 0;
    }

    return buffer;
#else
    return getenv(name);
#endif
}

}

#ifdef __cplusplus
extern "C" {
#endif

TRACY_API void ___tracy_set_thread_name( const char* name ) { tracy::SetThreadName( name ); }

#ifdef __cplusplus
}
#endif
