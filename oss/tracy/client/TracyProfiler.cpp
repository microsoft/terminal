#ifdef TRACY_ENABLE

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <windows.h>
#  include <tlhelp32.h>
#  include <inttypes.h>
#  include <intrin.h>
#  include "../common/TracyUwp.hpp"
#else
#  include <sys/time.h>
#  include <sys/param.h>
#endif

#ifdef _GNU_SOURCE
#  include <errno.h>
#endif

#ifdef __linux__
#  include <dirent.h>
#  include <pthread.h>
#  include <sys/types.h>
#  include <sys/syscall.h>
#endif

#if defined __APPLE__ || defined BSD
#  include <sys/types.h>
#  include <sys/sysctl.h>
#endif

#if defined __APPLE__
#  include "TargetConditionals.h"
#  include <mach-o/dyld.h>
#endif

#ifdef __ANDROID__
#  include <sys/mman.h>
#  include <sys/system_properties.h>
#  include <stdio.h>
#  include <stdint.h>
#  include <algorithm>
#  include <vector>
#endif

#include <algorithm>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <limits>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <thread>

#include "../common/TracyAlign.hpp"
#include "../common/TracyAlloc.hpp"
#include "../common/TracySocket.hpp"
#include "../common/TracySystem.hpp"
#include "../common/TracyYield.hpp"
#include "../common/tracy_lz4.hpp"
#include "tracy_rpmalloc.hpp"
#include "TracyCallstack.hpp"
#include "TracyDebug.hpp"
#include "TracyDxt1.hpp"
#include "TracyScoped.hpp"
#include "TracyProfiler.hpp"
#include "TracyThread.hpp"
#include "TracyArmCpuTable.hpp"
#include "TracySysTrace.hpp"
#include "../tracy/TracyC.h"

#ifdef TRACY_PORT
#  ifndef TRACY_DATA_PORT
#    define TRACY_DATA_PORT TRACY_PORT
#  endif
#  ifndef TRACY_BROADCAST_PORT
#    define TRACY_BROADCAST_PORT TRACY_PORT
#  endif
#endif

#ifdef __APPLE__
#  define TRACY_DELAYED_INIT
#else
#  ifdef __GNUC__
#    define init_order( val ) __attribute__ ((init_priority(val)))
#  else
#    define init_order(x)
#  endif
#endif

#if defined _WIN32
#  include <lmcons.h>
extern "C" typedef LONG (WINAPI *t_RtlGetVersion)( PRTL_OSVERSIONINFOW );
extern "C" typedef BOOL (WINAPI *t_GetLogicalProcessorInformationEx)( LOGICAL_PROCESSOR_RELATIONSHIP, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD );
#else
#  include <unistd.h>
#  include <limits.h>
#endif
#if defined __linux__
#  include <sys/sysinfo.h>
#  include <sys/utsname.h>
#endif

#if !defined _WIN32 && ( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
#  include "TracyCpuid.hpp"
#endif

#if !( ( defined _WIN32 && _WIN32_WINNT >= _WIN32_WINNT_VISTA ) || defined __linux__ )
#  include <mutex>
#endif

namespace tracy
{

#ifdef __ANDROID__
// Implementation helpers of EnsureReadable(address).
// This is so far only needed on Android, where it is common for libraries to be mapped
// with only executable, not readable, permissions. Typical example (line from /proc/self/maps):
/*
746b63b000-746b6dc000 --xp 00042000 07:48 35                             /apex/com.android.runtime/lib64/bionic/libc.so
*/
// See https://github.com/wolfpld/tracy/issues/125 .
// To work around this, we parse /proc/self/maps and we use mprotect to set read permissions
// on any mappings that contain symbols addresses hit by HandleSymbolCodeQuery.

namespace {
// Holds some information about a single memory mapping.
struct MappingInfo {
    // Start of address range. Inclusive.
    uintptr_t start_address;
    // End of address range. Exclusive, so the mapping is the half-open interval
    // [start, end) and its length in bytes is `end - start`. As in /proc/self/maps.
    uintptr_t end_address;
    // Read/Write/Executable permissions.
    bool perm_r, perm_w, perm_x;
};
}  // anonymous namespace

   // Internal implementation helper for LookUpMapping(address).
   //
   // Parses /proc/self/maps returning a vector<MappingInfo>.
   // /proc/self/maps is assumed to be sorted by ascending address, so the resulting
   // vector is sorted by ascending address too.
static std::vector<MappingInfo> ParseMappings()
{
    std::vector<MappingInfo> result;
    FILE* file = fopen( "/proc/self/maps", "r" );
    if( !file ) return result;
    char line[1024];
    while( fgets( line, sizeof( line ), file ) )
    {
        uintptr_t start_addr;
        uintptr_t end_addr;
        if( sscanf( line, "%lx-%lx", &start_addr, &end_addr ) != 2 ) continue;
        char* first_space = strchr( line, ' ' );
        if( !first_space ) continue;
        char* perm = first_space + 1;
        char* second_space = strchr( perm, ' ' );
        if( !second_space || second_space - perm != 4 ) continue;
        result.emplace_back();
        auto& mapping = result.back();
        mapping.start_address = start_addr;
        mapping.end_address = end_addr;
        mapping.perm_r = perm[0] == 'r';
        mapping.perm_w = perm[1] == 'w';
        mapping.perm_x = perm[2] == 'x';
    }
    fclose( file );
    return result;
}

// Internal implementation helper for LookUpMapping(address).
//
// Takes as input an `address` and a known vector `mappings`, assumed to be
// sorted by increasing addresses, as /proc/self/maps seems to be.
// Returns a pointer to the MappingInfo describing the mapping that this
// address belongs to, or nullptr if the address isn't in `mappings`.
static MappingInfo* LookUpMapping(std::vector<MappingInfo>& mappings, uintptr_t address)
{
    // Comparison function for std::lower_bound. Returns true if all addresses in `m1`
    // are lower than `addr`.
    auto Compare = []( const MappingInfo& m1, uintptr_t addr ) {
        // '<=' because the address ranges are half-open intervals, [start, end).
        return m1.end_address <= addr;
    };
    auto iter = std::lower_bound( mappings.begin(), mappings.end(), address, Compare );
    if( iter == mappings.end() || iter->start_address > address) {
        return nullptr;
    }
    return &*iter;
}

// Internal implementation helper for EnsureReadable(address).
//
// Takes as input an `address` and returns a pointer to a MappingInfo
// describing the mapping that this address belongs to, or nullptr if
// the address isn't in any known mapping.
//
// This function is stateful and not reentrant (assumes to be called from
// only one thread). It holds a vector of mappings parsed from /proc/self/maps.
//
// Attempts to react to mappings changes by re-parsing /proc/self/maps.
static MappingInfo* LookUpMapping(uintptr_t address)
{
    // Static state managed by this function. Not constant, we mutate that state as
    // we turn some mappings readable. Initially parsed once here, updated as needed below.
    static std::vector<MappingInfo> s_mappings = ParseMappings();
    MappingInfo* mapping = LookUpMapping( s_mappings, address );
    if( mapping ) return mapping;

    // This address isn't in any known mapping. Try parsing again, maybe
    // mappings changed.
    s_mappings = ParseMappings();
    return LookUpMapping( s_mappings, address );
}

// Internal implementation helper for EnsureReadable(address).
//
// Attempts to make the specified `mapping` readable if it isn't already.
// Returns true if and only if the mapping is readable.
static bool EnsureReadable( MappingInfo& mapping )
{
    if( mapping.perm_r )
    {
        // The mapping is already readable.
        return true;
    }
    int prot = PROT_READ;
    if( mapping.perm_w ) prot |= PROT_WRITE;
    if( mapping.perm_x ) prot |= PROT_EXEC;
    if( mprotect( reinterpret_cast<void*>( mapping.start_address ),
        mapping.end_address - mapping.start_address, prot ) == -1 )
    {
        // Failed to make the mapping readable. Shouldn't happen, hasn't
        // been observed yet. If it happened in practice, we should consider
        // adding a bool to MappingInfo to track this to avoid retrying mprotect
        // everytime on such mappings.
        return false;
    }
    // The mapping is now readable. Update `mapping` so the next call will be fast.
    mapping.perm_r = true;
    return true;
}

// Attempts to set the read permission on the entire mapping containing the
// specified address. Returns true if and only if the mapping is now readable.
static bool EnsureReadable( uintptr_t address )
{
    MappingInfo* mapping = LookUpMapping(address);
    return mapping && EnsureReadable( *mapping );
}

#endif  // defined __ANDROID__

#ifndef TRACY_DELAYED_INIT

struct InitTimeWrapper
{
    int64_t val;
};

struct ProducerWrapper
{
    tracy::moodycamel::ConcurrentQueue<QueueItem>::ExplicitProducer* ptr;
};

struct ThreadHandleWrapper
{
    uint32_t val;
};
#endif


#if defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64
static inline void CpuId( uint32_t* regs, uint32_t leaf )
{
    memset(regs, 0, sizeof(uint32_t) * 4);
#if defined _WIN32
    __cpuidex( (int*)regs, leaf, 0 );
#else
    __get_cpuid( leaf, regs, regs+1, regs+2, regs+3 );
#endif
}

static void InitFailure( const char* msg )
{
#if defined _WIN32
    bool hasConsole = false;
    bool reopen = false;
    const auto attached = AttachConsole( ATTACH_PARENT_PROCESS );
    if( attached )
    {
        hasConsole = true;
        reopen = true;
    }
    else
    {
        const auto err = GetLastError();
        if( err == ERROR_ACCESS_DENIED )
        {
            hasConsole = true;
        }
    }
    if( hasConsole )
    {
        fprintf( stderr, "Tracy Profiler initialization failure: %s\n", msg );
        if( reopen )
        {
            freopen( "CONOUT$", "w", stderr );
            fprintf( stderr, "Tracy Profiler initialization failure: %s\n", msg );
        }
    }
    else
    {
#  ifndef TRACY_UWP
        MessageBoxA( nullptr, msg, "Tracy Profiler initialization failure", MB_ICONSTOP );
#  endif
    }
#else
    fprintf( stderr, "Tracy Profiler initialization failure: %s\n", msg );
#endif
    exit( 1 );
}

static bool CheckHardwareSupportsInvariantTSC()
{
    const char* noCheck = GetEnvVar( "TRACY_NO_INVARIANT_CHECK" );
    if( noCheck && noCheck[0] == '1' ) return true;

    uint32_t regs[4];
    CpuId( regs, 1 );
    if( !( regs[3] & ( 1 << 4 ) ) )
    {
#if !defined TRACY_TIMER_QPC && !defined TRACY_TIMER_FALLBACK
        InitFailure( "CPU doesn't support RDTSC instruction." );
#else
        return false;
#endif
    }
    CpuId( regs, 0x80000007 );
    if( regs[3] & ( 1 << 8 ) ) return true;

    return false;
}

#if defined TRACY_TIMER_FALLBACK && defined TRACY_HW_TIMER
bool HardwareSupportsInvariantTSC()
{
    static bool cachedResult = CheckHardwareSupportsInvariantTSC();
    return cachedResult;
}
#endif

static int64_t SetupHwTimer()
{
#if !defined TRACY_TIMER_QPC && !defined TRACY_TIMER_FALLBACK
    if( !CheckHardwareSupportsInvariantTSC() )
    {
#if defined _WIN32
        InitFailure( "CPU doesn't support invariant TSC.\nDefine TRACY_NO_INVARIANT_CHECK=1 to ignore this error, *if you know what you are doing*.\nAlternatively you may rebuild the application with the TRACY_TIMER_QPC or TRACY_TIMER_FALLBACK define to use lower resolution timer." );
#else
        InitFailure( "CPU doesn't support invariant TSC.\nDefine TRACY_NO_INVARIANT_CHECK=1 to ignore this error, *if you know what you are doing*.\nAlternatively you may rebuild the application with the TRACY_TIMER_FALLBACK define to use lower resolution timer." );
#endif
    }
#endif

    return Profiler::GetTime();
}
#else
static int64_t SetupHwTimer()
{
    return Profiler::GetTime();
}
#endif

static const char* GetProcessName()
{
    const char* processName = "unknown";
#ifdef _WIN32
    static char buf[_MAX_PATH];
    GetModuleFileNameA( nullptr, buf, _MAX_PATH );
    const char* ptr = buf;
    while( *ptr != '\0' ) ptr++;
    while( ptr > buf && *ptr != '\\' && *ptr != '/' ) ptr--;
    if( ptr > buf ) ptr++;
    processName = ptr;
#elif defined __ANDROID__
#  if __ANDROID_API__ >= 21
    auto buf = getprogname();
    if( buf ) processName = buf;
#  endif
#elif defined __linux__ && defined _GNU_SOURCE
    if( program_invocation_short_name ) processName = program_invocation_short_name;
#elif defined __APPLE__ || defined BSD
    auto buf = getprogname();
    if( buf ) processName = buf;
#endif
    return processName;
}

static const char* GetProcessExecutablePath()
{
#ifdef _WIN32
    static char buf[_MAX_PATH];
    GetModuleFileNameA( nullptr, buf, _MAX_PATH );
    return buf;
#elif defined __ANDROID__
    return nullptr;
#elif defined __linux__ && defined _GNU_SOURCE
    return program_invocation_name;
#elif defined __APPLE__
    static char buf[1024];
    uint32_t size = 1024;
    _NSGetExecutablePath( buf, &size );
    return buf;
#elif defined __DragonFly__
    static char buf[1024];
    readlink( "/proc/curproc/file", buf, 1024 );
    return buf;
#elif defined __FreeBSD__
    static char buf[1024];
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    size_t cb = 1024;
    sysctl( mib, 4, buf, &cb, nullptr, 0 );
    return buf;
#elif defined __NetBSD__
    static char buf[1024];
    readlink( "/proc/curproc/exe", buf, 1024 );
    return buf;
#else
    return nullptr;
#endif
}

#if defined __linux__ && defined __ARM_ARCH
static uint32_t GetHex( char*& ptr, int skip )
{
    uint32_t ret;
    ptr += skip;
    char* end;
    if( ptr[0] == '0' && ptr[1] == 'x' )
    {
        ptr += 2;
        ret = strtol( ptr, &end, 16 );
    }
    else
    {
        ret = strtol( ptr, &end, 10 );
    }
    ptr = end;
    return ret;
}
#endif

static const char* GetHostInfo()
{
    static char buf[1024];
    auto ptr = buf;
#if defined _WIN32
#  ifdef TRACY_UWP
    auto GetVersion = &::GetVersionEx;
#  else
    auto GetVersion = (t_RtlGetVersion)GetProcAddress( GetModuleHandleA( "ntdll.dll" ), "RtlGetVersion" );
#  endif
    if( !GetVersion )
    {
#  ifdef __MINGW32__
        ptr += sprintf( ptr, "OS: Windows (MingW)\n" );
#  else
        ptr += sprintf( ptr, "OS: Windows\n" );
#  endif
    }
    else
    {
        RTL_OSVERSIONINFOW ver = { sizeof( RTL_OSVERSIONINFOW ) };
        GetVersion( &ver );

#  ifdef __MINGW32__
        ptr += sprintf( ptr, "OS: Windows %i.%i.%i (MingW)\n", (int)ver.dwMajorVersion, (int)ver.dwMinorVersion, (int)ver.dwBuildNumber );
#  else
        ptr += sprintf( ptr, "OS: Windows %i.%i.%i\n", ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber );
#  endif
    }
#elif defined __linux__
    struct utsname utsName;
    uname( &utsName );
#  if defined __ANDROID__
    ptr += sprintf( ptr, "OS: Linux %s (Android)\n", utsName.release );
#  else
    ptr += sprintf( ptr, "OS: Linux %s\n", utsName.release );
#  endif
#elif defined __APPLE__
#  if TARGET_OS_IPHONE == 1
    ptr += sprintf( ptr, "OS: Darwin (iOS)\n" );
#  elif TARGET_OS_MAC == 1
    ptr += sprintf( ptr, "OS: Darwin (OSX)\n" );
#  else
    ptr += sprintf( ptr, "OS: Darwin (unknown)\n" );
#  endif
#elif defined __DragonFly__
    ptr += sprintf( ptr, "OS: BSD (DragonFly)\n" );
#elif defined __FreeBSD__
    ptr += sprintf( ptr, "OS: BSD (FreeBSD)\n" );
#elif defined __NetBSD__
    ptr += sprintf( ptr, "OS: BSD (NetBSD)\n" );
#elif defined __OpenBSD__
    ptr += sprintf( ptr, "OS: BSD (OpenBSD)\n" );
#else
    ptr += sprintf( ptr, "OS: unknown\n" );
#endif

#if defined _MSC_VER
#  if defined __clang__
    ptr += sprintf( ptr, "Compiler: MSVC clang-cl %i.%i.%i\n", __clang_major__, __clang_minor__, __clang_patchlevel__ );
#  else
    ptr += sprintf( ptr, "Compiler: MSVC %i\n", _MSC_VER );
#  endif
#elif defined __clang__
    ptr += sprintf( ptr, "Compiler: clang %i.%i.%i\n", __clang_major__, __clang_minor__, __clang_patchlevel__ );
#elif defined __GNUC__
    ptr += sprintf( ptr, "Compiler: gcc %i.%i.%i\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ );
#else
    ptr += sprintf( ptr, "Compiler: unknown\n" );
#endif

#if defined _WIN32
    InitWinSock();

    char hostname[512];
    gethostname( hostname, 512 );

#  ifdef TRACY_UWP
    const char* user = "";
#  else
    DWORD userSz = UNLEN+1;
    char user[UNLEN+1];
    GetUserNameA( user, &userSz );
#  endif

    ptr += sprintf( ptr, "User: %s@%s\n", user, hostname );
#else
    char hostname[_POSIX_HOST_NAME_MAX]{};
    char user[_POSIX_LOGIN_NAME_MAX]{};

    gethostname( hostname, _POSIX_HOST_NAME_MAX );
#  if defined __ANDROID__
    const auto login = getlogin();
    if( login )
    {
        strcpy( user, login );
    }
    else
    {
        memcpy( user, "(?)", 4 );
    }
#  else
    getlogin_r( user, _POSIX_LOGIN_NAME_MAX );
#  endif

    ptr += sprintf( ptr, "User: %s@%s\n", user, hostname );
#endif

#if defined __i386 || defined _M_IX86
    ptr += sprintf( ptr, "Arch: x86\n" );
#elif defined __x86_64__ || defined _M_X64
    ptr += sprintf( ptr, "Arch: x64\n" );
#elif defined __aarch64__
    ptr += sprintf( ptr, "Arch: ARM64\n" );
#elif defined __ARM_ARCH
    ptr += sprintf( ptr, "Arch: ARM\n" );
#else
    ptr += sprintf( ptr, "Arch: unknown\n" );
#endif

#if defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64
    uint32_t regs[4];
    char cpuModel[4*4*3+1] = {};
    auto modelPtr = cpuModel;
    for( uint32_t i=0x80000002; i<0x80000005; ++i )
    {
        CpuId( regs, i );
        memcpy( modelPtr, regs, sizeof( regs ) ); modelPtr += sizeof( regs );
    }

    ptr += sprintf( ptr, "CPU: %s\n", cpuModel );
#elif defined __linux__ && defined __ARM_ARCH
    bool cpuFound = false;
    FILE* fcpuinfo = fopen( "/proc/cpuinfo", "rb" );
    if( fcpuinfo )
    {
        enum { BufSize = 4*1024 };
        char buf[BufSize];
        const auto sz = fread( buf, 1, BufSize, fcpuinfo );
        fclose( fcpuinfo );
        const auto end = buf + sz;
        auto cptr = buf;

        uint32_t impl = 0;
        uint32_t var = 0;
        uint32_t part = 0;
        uint32_t rev = 0;

        while( end - cptr > 20 )
        {
            while( end - cptr > 20 && memcmp( cptr, "CPU ", 4 ) != 0 )
            {
                cptr += 4;
                while( end - cptr > 20 && *cptr != '\n' ) cptr++;
                cptr++;
            }
            if( end - cptr <= 20 ) break;
            cptr += 4;
            if( memcmp( cptr, "implementer\t: ", 14 ) == 0 )
            {
                if( impl != 0 ) break;
                impl = GetHex( cptr, 14 );
            }
            else if( memcmp( cptr, "variant\t: ", 10 ) == 0 ) var = GetHex( cptr, 10 );
            else if( memcmp( cptr, "part\t: ", 7 ) == 0 ) part = GetHex( cptr, 7 );
            else if( memcmp( cptr, "revision\t: ", 11 ) == 0 ) rev = GetHex( cptr, 11 );
            while( *cptr != '\n' && *cptr != '\0' ) cptr++;
            cptr++;
        }

        if( impl != 0 || var != 0 || part != 0 || rev != 0 )
        {
            cpuFound = true;
            ptr += sprintf( ptr, "CPU: %s%s r%ip%i\n", DecodeArmImplementer( impl ), DecodeArmPart( impl, part ), var, rev );
        }
    }
    if( !cpuFound )
    {
        ptr += sprintf( ptr, "CPU: unknown\n" );
    }
#elif defined __APPLE__ && TARGET_OS_IPHONE == 1
    {
        size_t sz;
        sysctlbyname( "hw.machine", nullptr, &sz, nullptr, 0 );
        auto str = (char*)tracy_malloc( sz );
        sysctlbyname( "hw.machine", str, &sz, nullptr, 0 );
        ptr += sprintf( ptr, "Device: %s\n", DecodeIosDevice( str ) );
        tracy_free( str );
    }
#else
    ptr += sprintf( ptr, "CPU: unknown\n" );
#endif
#ifdef __ANDROID__
    char deviceModel[PROP_VALUE_MAX+1];
    char deviceManufacturer[PROP_VALUE_MAX+1];
    __system_property_get( "ro.product.model", deviceModel );
    __system_property_get( "ro.product.manufacturer", deviceManufacturer );
    ptr += sprintf( ptr, "Device: %s %s\n", deviceManufacturer, deviceModel );
#endif

    ptr += sprintf( ptr, "CPU cores: %i\n", std::thread::hardware_concurrency() );

#if defined _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof( statex );
    GlobalMemoryStatusEx( &statex );
#  ifdef _MSC_VER
    ptr += sprintf( ptr, "RAM: %I64u MB\n", statex.ullTotalPhys / 1024 / 1024 );
#  else
    ptr += sprintf( ptr, "RAM: %llu MB\n", statex.ullTotalPhys / 1024 / 1024 );
#  endif
#elif defined __linux__
    struct sysinfo sysInfo;
    sysinfo( &sysInfo );
    ptr += sprintf( ptr, "RAM: %lu MB\n", sysInfo.totalram / 1024 / 1024 );
#elif defined __APPLE__
    size_t memSize;
    size_t sz = sizeof( memSize );
    sysctlbyname( "hw.memsize", &memSize, &sz, nullptr, 0 );
    ptr += sprintf( ptr, "RAM: %zu MB\n", memSize / 1024 / 1024 );
#elif defined BSD
    size_t memSize;
    size_t sz = sizeof( memSize );
    sysctlbyname( "hw.physmem", &memSize, &sz, nullptr, 0 );
    ptr += sprintf( ptr, "RAM: %zu MB\n", memSize / 1024 / 1024 );
#else
    ptr += sprintf( ptr, "RAM: unknown\n" );
#endif

    return buf;
}

static uint64_t GetPid()
{
#if defined _WIN32
    return uint64_t( GetCurrentProcessId() );
#else
    return uint64_t( getpid() );
#endif
}

void Profiler::AckServerQuery()
{
    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::AckServerQueryNoop );
    NeedDataSize( QueueDataSize[(int)QueueType::AckServerQueryNoop] );
    AppendDataUnsafe( &item, QueueDataSize[(int)QueueType::AckServerQueryNoop] );
}

void Profiler::AckSymbolCodeNotAvailable()
{
    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::AckSymbolCodeNotAvailable );
    NeedDataSize( QueueDataSize[(int)QueueType::AckSymbolCodeNotAvailable] );
    AppendDataUnsafe( &item, QueueDataSize[(int)QueueType::AckSymbolCodeNotAvailable] );
}

static BroadcastMessage& GetBroadcastMessage( const char* procname, size_t pnsz, int& len, int port )
{
    static BroadcastMessage msg;

    msg.broadcastVersion = BroadcastVersion;
    msg.protocolVersion = ProtocolVersion;
    msg.listenPort = port;
    msg.pid = GetPid();

    memcpy( msg.programName, procname, pnsz );
    memset( msg.programName + pnsz, 0, WelcomeMessageProgramNameSize - pnsz );

    len = int( offsetof( BroadcastMessage, programName ) + pnsz + 1 );
    return msg;
}

#if defined _WIN32 && !defined TRACY_UWP && !defined TRACY_NO_CRASH_HANDLER
static DWORD s_profilerThreadId = 0;
static DWORD s_symbolThreadId = 0;
static char s_crashText[1024];

LONG WINAPI CrashFilter( PEXCEPTION_POINTERS pExp )
{
    if( !GetProfiler().IsConnected() ) return EXCEPTION_CONTINUE_SEARCH;

    const unsigned ec = pExp->ExceptionRecord->ExceptionCode;
    auto msgPtr = s_crashText;
    switch( ec )
    {
    case EXCEPTION_ACCESS_VIOLATION:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_ACCESS_VIOLATION (0x%x). ", ec );
        switch( pExp->ExceptionRecord->ExceptionInformation[0] )
        {
        case 0:
            msgPtr += sprintf( msgPtr, "Read violation at address 0x%" PRIxPTR ".", pExp->ExceptionRecord->ExceptionInformation[1] );
            break;
        case 1:
            msgPtr += sprintf( msgPtr, "Write violation at address 0x%" PRIxPTR ".", pExp->ExceptionRecord->ExceptionInformation[1] );
            break;
        case 8:
            msgPtr += sprintf( msgPtr, "DEP violation at address 0x%" PRIxPTR ".", pExp->ExceptionRecord->ExceptionInformation[1] );
            break;
        default:
            break;
        }
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_ARRAY_BOUNDS_EXCEEDED (0x%x). ", ec );
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_DATATYPE_MISALIGNMENT (0x%x). ", ec );
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_FLT_DIVIDE_BY_ZERO (0x%x). ", ec );
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_ILLEGAL_INSTRUCTION (0x%x). ", ec );
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_IN_PAGE_ERROR (0x%x). ", ec );
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_INT_DIVIDE_BY_ZERO (0x%x). ", ec );
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_PRIV_INSTRUCTION (0x%x). ", ec );
        break;
    case EXCEPTION_STACK_OVERFLOW:
        msgPtr += sprintf( msgPtr, "Exception EXCEPTION_STACK_OVERFLOW (0x%x). ", ec );
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }

    {
        GetProfiler().SendCallstack( 60, "KiUserExceptionDispatcher" );

        TracyQueuePrepare( QueueType::CrashReport );
        item->crashReport.time = Profiler::GetTime();
        item->crashReport.text = (uint64_t)s_crashText;
        TracyQueueCommit( crashReportThread );
    }

    HANDLE h = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
    if( h == INVALID_HANDLE_VALUE ) return EXCEPTION_CONTINUE_SEARCH;

    THREADENTRY32 te = { sizeof( te ) };
    if( !Thread32First( h, &te ) )
    {
        CloseHandle( h );
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const auto pid = GetCurrentProcessId();
    const auto tid = GetCurrentThreadId();

    do
    {
        if( te.th32OwnerProcessID == pid && te.th32ThreadID != tid && te.th32ThreadID != s_profilerThreadId && te.th32ThreadID != s_symbolThreadId )
        {
            HANDLE th = OpenThread( THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID );
            if( th != INVALID_HANDLE_VALUE )
            {
                SuspendThread( th );
                CloseHandle( th );
            }
        }
    }
    while( Thread32Next( h, &te ) );
    CloseHandle( h );

    {
        TracyLfqPrepare( QueueType::Crash );
        TracyLfqCommit;
    }

    std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    GetProfiler().RequestShutdown();
    while( !GetProfiler().HasShutdownFinished() ) { std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) ); };

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

static Profiler* s_instance = nullptr;
static Thread* s_thread;
#ifndef TRACY_NO_FRAME_IMAGE
static Thread* s_compressThread;
#endif
#ifdef TRACY_HAS_CALLSTACK
static Thread* s_symbolThread;
std::atomic<bool> s_symbolThreadGone { false };
#endif
#ifdef TRACY_HAS_SYSTEM_TRACING
static Thread* s_sysTraceThread = nullptr;
#endif

#if defined __linux__ && !defined TRACY_NO_CRASH_HANDLER
#  ifndef TRACY_CRASH_SIGNAL
#    define TRACY_CRASH_SIGNAL SIGPWR
#  endif

static long s_profilerTid = 0;
static long s_symbolTid = 0;
static char s_crashText[1024];
static std::atomic<bool> s_alreadyCrashed( false );

static void ThreadFreezer( int /*signal*/ )
{
    for(;;) sleep( 1000 );
}

static inline void HexPrint( char*& ptr, uint64_t val )
{
    if( val == 0 )
    {
        *ptr++ = '0';
        return;
    }

    static const char HexTable[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    char buf[16];
    auto bptr = buf;

    do
    {
        *bptr++ = HexTable[val%16];
        val /= 16;
    }
    while( val > 0 );

    do
    {
        *ptr++ = *--bptr;
    }
    while( bptr != buf );
}

static void CrashHandler( int signal, siginfo_t* info, void* /*ucontext*/ )
{
    bool expected = false;
    if( !s_alreadyCrashed.compare_exchange_strong( expected, true ) ) ThreadFreezer( signal );

    struct sigaction act = {};
    act.sa_handler = SIG_DFL;
    sigaction( SIGABRT, &act, nullptr );

    auto msgPtr = s_crashText;
    switch( signal )
    {
    case SIGILL:
        strcpy( msgPtr, "Illegal Instruction.\n" );
        while( *msgPtr ) msgPtr++;
        switch( info->si_code )
        {
        case ILL_ILLOPC:
            strcpy( msgPtr, "Illegal opcode.\n" );
            break;
        case ILL_ILLOPN:
            strcpy( msgPtr, "Illegal operand.\n" );
            break;
        case ILL_ILLADR:
            strcpy( msgPtr, "Illegal addressing mode.\n" );
            break;
        case ILL_ILLTRP:
            strcpy( msgPtr, "Illegal trap.\n" );
            break;
        case ILL_PRVOPC:
            strcpy( msgPtr, "Privileged opcode.\n" );
            break;
        case ILL_PRVREG:
            strcpy( msgPtr, "Privileged register.\n" );
            break;
        case ILL_COPROC:
            strcpy( msgPtr, "Coprocessor error.\n" );
            break;
        case ILL_BADSTK:
            strcpy( msgPtr, "Internal stack error.\n" );
            break;
        default:
            break;
        }
        break;
    case SIGFPE:
        strcpy( msgPtr, "Floating-point exception.\n" );
        while( *msgPtr ) msgPtr++;
        switch( info->si_code )
        {
        case FPE_INTDIV:
            strcpy( msgPtr, "Integer divide by zero.\n" );
            break;
        case FPE_INTOVF:
            strcpy( msgPtr, "Integer overflow.\n" );
            break;
        case FPE_FLTDIV:
            strcpy( msgPtr, "Floating-point divide by zero.\n" );
            break;
        case FPE_FLTOVF:
            strcpy( msgPtr, "Floating-point overflow.\n" );
            break;
        case FPE_FLTUND:
            strcpy( msgPtr, "Floating-point underflow.\n" );
            break;
        case FPE_FLTRES:
            strcpy( msgPtr, "Floating-point inexact result.\n" );
            break;
        case FPE_FLTINV:
            strcpy( msgPtr, "Floating-point invalid operation.\n" );
            break;
        case FPE_FLTSUB:
            strcpy( msgPtr, "Subscript out of range.\n" );
            break;
        default:
            break;
        }
        break;
    case SIGSEGV:
        strcpy( msgPtr, "Invalid memory reference.\n" );
        while( *msgPtr ) msgPtr++;
        switch( info->si_code )
        {
        case SEGV_MAPERR:
            strcpy( msgPtr, "Address not mapped to object.\n" );
            break;
        case SEGV_ACCERR:
            strcpy( msgPtr, "Invalid permissions for mapped object.\n" );
            break;
#  ifdef SEGV_BNDERR
        case SEGV_BNDERR:
            strcpy( msgPtr, "Failed address bound checks.\n" );
            break;
#  endif
#  ifdef SEGV_PKUERR
        case SEGV_PKUERR:
            strcpy( msgPtr, "Access was denied by memory protection keys.\n" );
            break;
#  endif
        default:
            break;
        }
        break;
    case SIGPIPE:
        strcpy( msgPtr, "Broken pipe.\n" );
        while( *msgPtr ) msgPtr++;
        break;
    case SIGBUS:
        strcpy( msgPtr, "Bus error.\n" );
        while( *msgPtr ) msgPtr++;
        switch( info->si_code )
        {
        case BUS_ADRALN:
            strcpy( msgPtr, "Invalid address alignment.\n" );
            break;
        case BUS_ADRERR:
            strcpy( msgPtr, "Nonexistent physical address.\n" );
            break;
        case BUS_OBJERR:
            strcpy( msgPtr, "Object-specific hardware error.\n" );
            break;
#  ifdef BUS_MCEERR_AR
        case BUS_MCEERR_AR:
            strcpy( msgPtr, "Hardware memory error consumed on a machine check; action required.\n" );
            break;
#  endif
#  ifdef BUS_MCEERR_AO
        case BUS_MCEERR_AO:
            strcpy( msgPtr, "Hardware memory error detected in process but not consumed; action optional.\n" );
            break;
#  endif
        default:
            break;
        }
        break;
    case SIGABRT:
        strcpy( msgPtr, "Abort signal from abort().\n" );
        break;
    default:
        abort();
    }
    while( *msgPtr ) msgPtr++;

    if( signal != SIGPIPE )
    {
        strcpy( msgPtr, "Fault address: 0x" );
        while( *msgPtr ) msgPtr++;
        HexPrint( msgPtr, uint64_t( info->si_addr ) );
        *msgPtr++ = '\n';
    }

    {
        GetProfiler().SendCallstack( 60, "__kernel_rt_sigreturn" );

        TracyQueuePrepare( QueueType::CrashReport );
        item->crashReport.time = Profiler::GetTime();
        item->crashReport.text = (uint64_t)s_crashText;
        TracyQueueCommit( crashReportThread );
    }

    DIR* dp = opendir( "/proc/self/task" );
    if( !dp ) abort();

    const auto selfTid = syscall( SYS_gettid );

    struct dirent* ep;
    while( ( ep = readdir( dp ) ) != nullptr )
    {
        if( ep->d_name[0] == '.' ) continue;
        int tid = atoi( ep->d_name );
        if( tid != selfTid && tid != s_profilerTid && tid != s_symbolTid )
        {
            syscall( SYS_tkill, tid, TRACY_CRASH_SIGNAL );
        }
    }
    closedir( dp );

    if( selfTid == s_symbolTid ) s_symbolThreadGone.store( true, std::memory_order_release );

    TracyLfqPrepare( QueueType::Crash );
    TracyLfqCommit;

    std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    GetProfiler().RequestShutdown();
    while( !GetProfiler().HasShutdownFinished() ) { std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) ); };

    abort();
}
#endif


enum { QueuePrealloc = 256 * 1024 };

TRACY_API int64_t GetFrequencyQpc()
{
#if defined _WIN32
    LARGE_INTEGER t;
    QueryPerformanceFrequency( &t );
    return t.QuadPart;
#else
    return 0;
#endif
}

#ifdef TRACY_DELAYED_INIT
struct ThreadNameData;
TRACY_API moodycamel::ConcurrentQueue<QueueItem>& GetQueue();

struct ProfilerData
{
    int64_t initTime = SetupHwTimer();
    moodycamel::ConcurrentQueue<QueueItem> queue;
    Profiler profiler;
    std::atomic<uint32_t> lockCounter { 0 };
    std::atomic<uint8_t> gpuCtxCounter { 0 };
    std::atomic<ThreadNameData*> threadNameData { nullptr };
};

struct ProducerWrapper
{
    ProducerWrapper( ProfilerData& data ) : detail( data.queue ), ptr( data.queue.get_explicit_producer( detail ) ) {}
    moodycamel::ProducerToken detail;
    tracy::moodycamel::ConcurrentQueue<QueueItem>::ExplicitProducer* ptr;
};

struct ProfilerThreadData
{
    ProfilerThreadData( ProfilerData& data ) : token( data ), gpuCtx( { nullptr } ) {}
    ProducerWrapper token;
    GpuCtxWrapper gpuCtx;
#  ifdef TRACY_ON_DEMAND
    LuaZoneState luaZoneState;
#  endif
};

std::atomic<int> RpInitDone { 0 };
std::atomic<int> RpInitLock { 0 };
thread_local bool RpThreadInitDone = false;
thread_local bool RpThreadShutdown = false;

#  ifdef TRACY_MANUAL_LIFETIME
ProfilerData* s_profilerData = nullptr;
static ProfilerThreadData& GetProfilerThreadData();
TRACY_API void StartupProfiler()
{
    s_profilerData = (ProfilerData*)tracy_malloc( sizeof( ProfilerData ) );
    new (s_profilerData) ProfilerData();
    s_profilerData->profiler.SpawnWorkerThreads();
    GetProfilerThreadData().token = ProducerWrapper( *s_profilerData );
}
static ProfilerData& GetProfilerData()
{
    assert( s_profilerData );
    return *s_profilerData;
}
TRACY_API void ShutdownProfiler()
{
    s_profilerData->~ProfilerData();
    tracy_free( s_profilerData );
    s_profilerData = nullptr;
    rpmalloc_finalize();
    RpThreadInitDone = false;
    RpInitDone.store( 0, std::memory_order_release );
}
#  else
static std::atomic<int> profilerDataLock { 0 };
static std::atomic<ProfilerData*> profilerData { nullptr };

static ProfilerData& GetProfilerData()
{
    auto ptr = profilerData.load( std::memory_order_acquire );
    if( !ptr )
    {
        int expected = 0;
        while( !profilerDataLock.compare_exchange_weak( expected, 1, std::memory_order_release, std::memory_order_relaxed ) ) { expected = 0; YieldThread(); }
        ptr = profilerData.load( std::memory_order_acquire );
        if( !ptr )
        {
            ptr = (ProfilerData*)tracy_malloc( sizeof( ProfilerData ) );
            new (ptr) ProfilerData();
            profilerData.store( ptr, std::memory_order_release );
        }
        profilerDataLock.store( 0, std::memory_order_release );
    }
    return *ptr;
}
#  endif

// GCC prior to 8.4 had a bug with function-inline thread_local variables. Versions of glibc beginning with
// 2.18 may attempt to work around this issue, which manifests as a crash while running static destructors
// if this function is compiled into a shared object. Unfortunately, centos7 ships with glibc 2.17. If running
// on old GCC, use the old-fashioned way as a workaround
// See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85400
#if !defined(__clang__) && defined(__GNUC__) && ((__GNUC__ < 8) || ((__GNUC__ == 8) && (__GNUC_MINOR__ < 4)))
struct ProfilerThreadDataKey
{
public:
    ProfilerThreadDataKey()
    {
        int val = pthread_key_create(&m_key, sDestructor);
        static_cast<void>(val); // unused
        assert(val == 0);
    }
    ~ProfilerThreadDataKey()
    {
        int val = pthread_key_delete(m_key);
        static_cast<void>(val); // unused
        assert(val == 0);
    }
    ProfilerThreadData& get()
    {
        void* p = pthread_getspecific(m_key);
        if (!p)
        {
            p = (ProfilerThreadData*)tracy_malloc( sizeof( ProfilerThreadData ) );
            new (p) ProfilerThreadData(GetProfilerData());
            pthread_setspecific(m_key, p);
        }
        return *static_cast<ProfilerThreadData*>(p);
    }
private:
    pthread_key_t m_key;

    static void sDestructor(void* p)
    {
        ((ProfilerThreadData*)p)->~ProfilerThreadData();
        tracy_free(p);
    }
};

static ProfilerThreadData& GetProfilerThreadData()
{
    static ProfilerThreadDataKey key;
    return key.get();
}
#else
static ProfilerThreadData& GetProfilerThreadData()
{
    thread_local ProfilerThreadData data( GetProfilerData() );
    return data;
}
#endif

TRACY_API moodycamel::ConcurrentQueue<QueueItem>::ExplicitProducer* GetToken() { return GetProfilerThreadData().token.ptr; }
TRACY_API Profiler& GetProfiler() { return GetProfilerData().profiler; }
TRACY_API moodycamel::ConcurrentQueue<QueueItem>& GetQueue() { return GetProfilerData().queue; }
TRACY_API int64_t GetInitTime() { return GetProfilerData().initTime; }
TRACY_API std::atomic<uint32_t>& GetLockCounter() { return GetProfilerData().lockCounter; }
TRACY_API std::atomic<uint8_t>& GetGpuCtxCounter() { return GetProfilerData().gpuCtxCounter; }
TRACY_API GpuCtxWrapper& GetGpuCtx() { return GetProfilerThreadData().gpuCtx; }
TRACY_API uint32_t GetThreadHandle() { return detail::GetThreadHandleImpl(); }
std::atomic<ThreadNameData*>& GetThreadNameData() { return GetProfilerData().threadNameData; }

#  ifdef TRACY_ON_DEMAND
TRACY_API LuaZoneState& GetLuaZoneState() { return GetProfilerThreadData().luaZoneState; }
#  endif

#  ifndef TRACY_MANUAL_LIFETIME
namespace
{
    const auto& __profiler_init = GetProfiler();
}
#  endif

#else

// MSVC static initialization order solution. gcc/clang uses init_order() to avoid all this.

// 1a. But s_queue is needed for initialization of variables in point 2.
extern moodycamel::ConcurrentQueue<QueueItem> s_queue;

// 2. If these variables would be in the .CRT$XCB section, they would be initialized only in main thread.
thread_local moodycamel::ProducerToken init_order(107) s_token_detail( s_queue );
thread_local ProducerWrapper init_order(108) s_token { s_queue.get_explicit_producer( s_token_detail ) };
thread_local ThreadHandleWrapper init_order(104) s_threadHandle { detail::GetThreadHandleImpl() };

#  ifdef _MSC_VER
// 1. Initialize these static variables before all other variables.
#    pragma warning( disable : 4075 )
#    pragma init_seg( ".CRT$XCB" )
#  endif

static InitTimeWrapper init_order(101) s_initTime { SetupHwTimer() };
std::atomic<int> init_order(102) RpInitDone( 0 );
std::atomic<int> init_order(102) RpInitLock( 0 );
thread_local bool RpThreadInitDone = false;
thread_local bool RpThreadShutdown = false;
moodycamel::ConcurrentQueue<QueueItem> init_order(103) s_queue( QueuePrealloc );
std::atomic<uint32_t> init_order(104) s_lockCounter( 0 );
std::atomic<uint8_t> init_order(104) s_gpuCtxCounter( 0 );

thread_local GpuCtxWrapper init_order(104) s_gpuCtx { nullptr };

struct ThreadNameData;
static std::atomic<ThreadNameData*> init_order(104) s_threadNameDataInstance( nullptr );
std::atomic<ThreadNameData*>& s_threadNameData = s_threadNameDataInstance;

#  ifdef TRACY_ON_DEMAND
thread_local LuaZoneState init_order(104) s_luaZoneState { 0, false };
#  endif

static Profiler init_order(105) s_profiler;

TRACY_API moodycamel::ConcurrentQueue<QueueItem>::ExplicitProducer* GetToken() { return s_token.ptr; }
TRACY_API Profiler& GetProfiler() { return s_profiler; }
TRACY_API moodycamel::ConcurrentQueue<QueueItem>& GetQueue() { return s_queue; }
TRACY_API int64_t GetInitTime() { return s_initTime.val; }
TRACY_API std::atomic<uint32_t>& GetLockCounter() { return s_lockCounter; }
TRACY_API std::atomic<uint8_t>& GetGpuCtxCounter() { return s_gpuCtxCounter; }
TRACY_API GpuCtxWrapper& GetGpuCtx() { return s_gpuCtx; }
TRACY_API uint32_t GetThreadHandle() { return s_threadHandle.val; }

std::atomic<ThreadNameData*>& GetThreadNameData() { return s_threadNameData; }

#  ifdef TRACY_ON_DEMAND
TRACY_API LuaZoneState& GetLuaZoneState() { return s_luaZoneState; }
#  endif
#endif

TRACY_API bool ProfilerAvailable() { return s_instance != nullptr; }
TRACY_API bool ProfilerAllocatorAvailable() { return !RpThreadShutdown; }

Profiler::Profiler()
    : m_timeBegin( 0 )
    , m_mainThread( detail::GetThreadHandleImpl() )
    , m_epoch( std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now().time_since_epoch() ).count() )
    , m_shutdown( false )
    , m_shutdownManual( false )
    , m_shutdownFinished( false )
    , m_sock( nullptr )
    , m_broadcast( nullptr )
    , m_noExit( false )
    , m_userPort( 0 )
    , m_zoneId( 1 )
    , m_samplingPeriod( 0 )
    , m_stream( LZ4_createStream() )
    , m_buffer( (char*)tracy_malloc( TargetFrameSize*3 ) )
    , m_bufferOffset( 0 )
    , m_bufferStart( 0 )
    , m_lz4Buf( (char*)tracy_malloc( LZ4Size + sizeof( lz4sz_t ) ) )
    , m_serialQueue( 1024*1024 )
    , m_serialDequeue( 1024*1024 )
#ifndef TRACY_NO_FRAME_IMAGE
    , m_fiQueue( 16 )
    , m_fiDequeue( 16 )
#endif
    , m_symbolQueue( 8*1024 )
    , m_frameCount( 0 )
    , m_isConnected( false )
#ifdef TRACY_ON_DEMAND
    , m_connectionId( 0 )
    , m_deferredQueue( 64*1024 )
#endif
    , m_paramCallback( nullptr )
    , m_sourceCallback( nullptr )
    , m_queryImage( nullptr )
    , m_queryData( nullptr )
    , m_crashHandlerInstalled( false )
{
    assert( !s_instance );
    s_instance = this;

#ifndef TRACY_DELAYED_INIT
#  ifdef _MSC_VER
    // 3. But these variables need to be initialized in main thread within the .CRT$XCB section. Do it here.
    s_token_detail = moodycamel::ProducerToken( s_queue );
    s_token = ProducerWrapper { s_queue.get_explicit_producer( s_token_detail ) };
    s_threadHandle = ThreadHandleWrapper { m_mainThread };
#  endif
#endif

    CalibrateTimer();
    CalibrateDelay();
    ReportTopology();

#ifndef TRACY_NO_EXIT
    const char* noExitEnv = GetEnvVar( "TRACY_NO_EXIT" );
    if( noExitEnv && noExitEnv[0] == '1' )
    {
        m_noExit = true;
    }
#endif

    const char* userPort = GetEnvVar( "TRACY_PORT" );
    if( userPort )
    {
        m_userPort = atoi( userPort );
    }

#if !defined(TRACY_DELAYED_INIT) || !defined(TRACY_MANUAL_LIFETIME)
    SpawnWorkerThreads();
#endif
}

void Profiler::SpawnWorkerThreads()
{
#ifdef TRACY_HAS_SYSTEM_TRACING
    if( SysTraceStart( m_samplingPeriod ) )
    {
        s_sysTraceThread = (Thread*)tracy_malloc( sizeof( Thread ) );
        new(s_sysTraceThread) Thread( SysTraceWorker, nullptr );
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }
#endif

    s_thread = (Thread*)tracy_malloc( sizeof( Thread ) );
    new(s_thread) Thread( LaunchWorker, this );

#ifndef TRACY_NO_FRAME_IMAGE
    s_compressThread = (Thread*)tracy_malloc( sizeof( Thread ) );
    new(s_compressThread) Thread( LaunchCompressWorker, this );
#endif

#ifdef TRACY_HAS_CALLSTACK
    s_symbolThread = (Thread*)tracy_malloc( sizeof( Thread ) );
    new(s_symbolThread) Thread( LaunchSymbolWorker, this );
#endif

#if defined _WIN32 && !defined TRACY_UWP && !defined TRACY_NO_CRASH_HANDLER
    s_profilerThreadId = GetThreadId( s_thread->Handle() );
#  ifdef TRACY_HAS_CALLSTACK
    s_symbolThreadId = GetThreadId( s_symbolThread->Handle() );
#  endif
    m_exceptionHandler = AddVectoredExceptionHandler( 1, CrashFilter );
#endif

#if defined __linux__ && !defined TRACY_NO_CRASH_HANDLER
    struct sigaction threadFreezer = {};
    threadFreezer.sa_handler = ThreadFreezer;
    sigaction( TRACY_CRASH_SIGNAL, &threadFreezer, &m_prevSignal.pwr );

    struct sigaction crashHandler = {};
    crashHandler.sa_sigaction = CrashHandler;
    crashHandler.sa_flags = SA_SIGINFO;
    sigaction( SIGILL, &crashHandler, &m_prevSignal.ill );
    sigaction( SIGFPE, &crashHandler, &m_prevSignal.fpe );
    sigaction( SIGSEGV, &crashHandler, &m_prevSignal.segv );
    sigaction( SIGPIPE, &crashHandler, &m_prevSignal.pipe );
    sigaction( SIGBUS, &crashHandler, &m_prevSignal.bus );
    sigaction( SIGABRT, &crashHandler, &m_prevSignal.abrt );
#endif

#ifndef TRACY_NO_CRASH_HANDLER
    m_crashHandlerInstalled = true;
#endif

#ifdef TRACY_HAS_CALLSTACK
    InitCallstackCritical();
#endif

    m_timeBegin.store( GetTime(), std::memory_order_relaxed );
}

Profiler::~Profiler()
{
    m_shutdown.store( true, std::memory_order_relaxed );

#if defined _WIN32 && !defined TRACY_UWP
    if( m_crashHandlerInstalled ) RemoveVectoredExceptionHandler( m_exceptionHandler );
#endif

#if defined __linux__ && !defined TRACY_NO_CRASH_HANDLER
    if( m_crashHandlerInstalled )
    {
        sigaction( TRACY_CRASH_SIGNAL, &m_prevSignal.pwr, nullptr );
        sigaction( SIGILL, &m_prevSignal.ill, nullptr );
        sigaction( SIGFPE, &m_prevSignal.fpe, nullptr );
        sigaction( SIGSEGV, &m_prevSignal.segv, nullptr );
        sigaction( SIGPIPE, &m_prevSignal.pipe, nullptr );
        sigaction( SIGBUS, &m_prevSignal.bus, nullptr );
        sigaction( SIGABRT, &m_prevSignal.abrt, nullptr );
    }
#endif

#ifdef TRACY_HAS_SYSTEM_TRACING
    if( s_sysTraceThread )
    {
        SysTraceStop();
        s_sysTraceThread->~Thread();
        tracy_free( s_sysTraceThread );
    }
#endif

#ifdef TRACY_HAS_CALLSTACK
    s_symbolThread->~Thread();
    tracy_free( s_symbolThread );
#endif

#ifndef TRACY_NO_FRAME_IMAGE
    s_compressThread->~Thread();
    tracy_free( s_compressThread );
#endif

    s_thread->~Thread();
    tracy_free( s_thread );

#ifdef TRACY_HAS_CALLSTACK
    EndCallstack();
#endif

    tracy_free( m_lz4Buf );
    tracy_free( m_buffer );
    LZ4_freeStream( (LZ4_stream_t*)m_stream );

    if( m_sock )
    {
        m_sock->~Socket();
        tracy_free( m_sock );
    }

    if( m_broadcast )
    {
        m_broadcast->~UdpBroadcast();
        tracy_free( m_broadcast );
    }

    assert( s_instance );
    s_instance = nullptr;
}

bool Profiler::ShouldExit()
{
    return s_instance->m_shutdown.load( std::memory_order_relaxed );
}

void Profiler::Worker()
{
#if defined __linux__ && !defined TRACY_NO_CRASH_HANDLER
    s_profilerTid = syscall( SYS_gettid );
#endif

    ThreadExitHandler threadExitHandler;

    SetThreadName( "Tracy Profiler" );

#ifdef TRACY_DATA_PORT
    const bool dataPortSearch = false;
    auto dataPort = m_userPort != 0 ? m_userPort : TRACY_DATA_PORT;
#else
    const bool dataPortSearch = m_userPort == 0;
    auto dataPort = m_userPort != 0 ? m_userPort : 8086;
#endif
#ifdef TRACY_BROADCAST_PORT
    const auto broadcastPort = TRACY_BROADCAST_PORT;
#else
    const auto broadcastPort = 8086;
#endif

    while( m_timeBegin.load( std::memory_order_relaxed ) == 0 ) std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

#ifdef TRACY_USE_RPMALLOC
    rpmalloc_thread_initialize();
#endif

    m_exectime = 0;
    const auto execname = GetProcessExecutablePath();
    if( execname )
    {
        struct stat st;
        if( stat( execname, &st ) == 0 )
        {
            m_exectime = (uint64_t)st.st_mtime;
        }
    }

    const auto procname = GetProcessName();
    const auto pnsz = std::min<size_t>( strlen( procname ), WelcomeMessageProgramNameSize - 1 );

    const auto hostinfo = GetHostInfo();
    const auto hisz = std::min<size_t>( strlen( hostinfo ), WelcomeMessageHostInfoSize - 1 );

    const uint64_t pid = GetPid();

    uint8_t flags = 0;

#ifdef TRACY_ON_DEMAND
    flags |= WelcomeFlag::OnDemand;
#endif
#ifdef __APPLE__
    flags |= WelcomeFlag::IsApple;
#endif
#ifndef TRACY_NO_CODE_TRANSFER
    flags |= WelcomeFlag::CodeTransfer;
#endif
#ifdef _WIN32
    flags |= WelcomeFlag::CombineSamples;
#  ifndef TRACY_NO_CONTEXT_SWITCH
    flags |= WelcomeFlag::IdentifySamples;
#  endif
#endif

#if defined __i386 || defined _M_IX86
    uint8_t cpuArch = CpuArchX86;
#elif defined __x86_64__ || defined _M_X64
    uint8_t cpuArch = CpuArchX64;
#elif defined __aarch64__
    uint8_t cpuArch = CpuArchArm64;
#elif defined __ARM_ARCH
    uint8_t cpuArch = CpuArchArm32;
#else
    uint8_t cpuArch = CpuArchUnknown;
#endif

#if defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64
    uint32_t regs[4];
    char manufacturer[12];
    CpuId( regs, 0 );
    memcpy( manufacturer, regs+1, 4 );
    memcpy( manufacturer+4, regs+3, 4 );
    memcpy( manufacturer+8, regs+2, 4 );

    CpuId( regs, 1 );
    uint32_t cpuId = ( regs[0] & 0xFFF ) | ( ( regs[0] & 0xFFF0000 ) >> 4 );
#else
    const char manufacturer[12] = {};
    uint32_t cpuId = 0;
#endif

    WelcomeMessage welcome;
    MemWrite( &welcome.timerMul, m_timerMul );
    MemWrite( &welcome.initBegin, GetInitTime() );
    MemWrite( &welcome.initEnd, m_timeBegin.load( std::memory_order_relaxed ) );
    MemWrite( &welcome.delay, m_delay );
    MemWrite( &welcome.resolution, m_resolution );
    MemWrite( &welcome.epoch, m_epoch );
    MemWrite( &welcome.exectime, m_exectime );
    MemWrite( &welcome.pid, pid );
    MemWrite( &welcome.samplingPeriod, m_samplingPeriod );
    MemWrite( &welcome.flags, flags );
    MemWrite( &welcome.cpuArch, cpuArch );
    memcpy( welcome.cpuManufacturer, manufacturer, 12 );
    MemWrite( &welcome.cpuId, cpuId );
    memcpy( welcome.programName, procname, pnsz );
    memset( welcome.programName + pnsz, 0, WelcomeMessageProgramNameSize - pnsz );
    memcpy( welcome.hostInfo, hostinfo, hisz );
    memset( welcome.hostInfo + hisz, 0, WelcomeMessageHostInfoSize - hisz );

    moodycamel::ConsumerToken token( GetQueue() );

    ListenSocket listen;
    bool isListening = false;
    if( !dataPortSearch )
    {
        isListening = listen.Listen( dataPort, 4 );
    }
    else
    {
        for( uint32_t i=0; i<20; i++ )
        {
            if( listen.Listen( dataPort+i, 4 ) )
            {
                dataPort += i;
                isListening = true;
                break;
            }
        }
    }
    if( !isListening )
    {
        for(;;)
        {
            if( ShouldExit() )
            {
                m_shutdownFinished.store( true, std::memory_order_relaxed );
                return;
            }

            ClearQueues( token );
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        }
    }

#ifndef TRACY_NO_BROADCAST
    m_broadcast = (UdpBroadcast*)tracy_malloc( sizeof( UdpBroadcast ) );
    new(m_broadcast) UdpBroadcast();
#  ifdef TRACY_ONLY_LOCALHOST
    const char* addr = "127.255.255.255";
#  else
    const char* addr = "255.255.255.255";
#  endif
    if( !m_broadcast->Open( addr, broadcastPort ) )
    {
        m_broadcast->~UdpBroadcast();
        tracy_free( m_broadcast );
        m_broadcast = nullptr;
    }
#endif

    int broadcastLen = 0;
    auto& broadcastMsg = GetBroadcastMessage( procname, pnsz, broadcastLen, dataPort );
    uint64_t lastBroadcast = 0;

    // Connections loop.
    // Each iteration of the loop handles whole connection. Multiple iterations will only
    // happen in the on-demand mode or when handshake fails.
    for(;;)
    {
        // Wait for incoming connection
        for(;;)
        {
#ifndef TRACY_NO_EXIT
            if( !m_noExit && ShouldExit() )
            {
                if( m_broadcast )
                {
                    broadcastMsg.activeTime = -1;
                    m_broadcast->Send( broadcastPort, &broadcastMsg, broadcastLen );
                }
                m_shutdownFinished.store( true, std::memory_order_relaxed );
                return;
            }
#endif
            m_sock = listen.Accept();
            if( m_sock ) break;
#ifndef TRACY_ON_DEMAND
            ProcessSysTime();
#endif

            if( m_broadcast )
            {
                const auto t = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                if( t - lastBroadcast > 3000000000 )  // 3s
                {
                    lastBroadcast = t;
                    const auto ts = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
                    broadcastMsg.activeTime = int32_t( ts - m_epoch );
                    assert( broadcastMsg.activeTime >= 0 );
                    m_broadcast->Send( broadcastPort, &broadcastMsg, broadcastLen );
                }
            }
        }

        if( m_broadcast )
        {
            lastBroadcast = 0;
            broadcastMsg.activeTime = -1;
            m_broadcast->Send( broadcastPort, &broadcastMsg, broadcastLen );
        }

        // Handshake
        {
            char shibboleth[HandshakeShibbolethSize];
            auto res = m_sock->ReadRaw( shibboleth, HandshakeShibbolethSize, 2000 );
            if( !res || memcmp( shibboleth, HandshakeShibboleth, HandshakeShibbolethSize ) != 0 )
            {
                m_sock->~Socket();
                tracy_free( m_sock );
                m_sock = nullptr;
                continue;
            }

            uint32_t protocolVersion;
            res = m_sock->ReadRaw( &protocolVersion, sizeof( protocolVersion ), 2000 );
            if( !res )
            {
                m_sock->~Socket();
                tracy_free( m_sock );
                m_sock = nullptr;
                continue;
            }

            if( protocolVersion != ProtocolVersion )
            {
                HandshakeStatus status = HandshakeProtocolMismatch;
                m_sock->Send( &status, sizeof( status ) );
                m_sock->~Socket();
                tracy_free( m_sock );
                m_sock = nullptr;
                continue;
            }
        }

#ifdef TRACY_ON_DEMAND
        const auto currentTime = GetTime();
        ClearQueues( token );
        m_connectionId.fetch_add( 1, std::memory_order_release );
#endif
        m_isConnected.store( true, std::memory_order_release );

        HandshakeStatus handshake = HandshakeWelcome;
        m_sock->Send( &handshake, sizeof( handshake ) );

        LZ4_resetStream( (LZ4_stream_t*)m_stream );
        m_sock->Send( &welcome, sizeof( welcome ) );

        m_threadCtx = 0;
        m_refTimeSerial = 0;
        m_refTimeCtx = 0;
        m_refTimeGpu = 0;

#ifdef TRACY_ON_DEMAND
        OnDemandPayloadMessage onDemand;
        onDemand.frames = m_frameCount.load( std::memory_order_relaxed );
        onDemand.currentTime = currentTime;

        m_sock->Send( &onDemand, sizeof( onDemand ) );

        m_deferredLock.lock();
        for( auto& item : m_deferredQueue )
        {
            uint64_t ptr;
            uint16_t size;
            const auto idx = MemRead<uint8_t>( &item.hdr.idx );
            switch( (QueueType)idx )
            {
            case QueueType::MessageAppInfo:
                ptr = MemRead<uint64_t>( &item.messageFat.text );
                size = MemRead<uint16_t>( &item.messageFat.size );
                SendSingleString( (const char*)ptr, size );
                break;
            case QueueType::LockName:
                ptr = MemRead<uint64_t>( &item.lockNameFat.name );
                size = MemRead<uint16_t>( &item.lockNameFat.size );
                SendSingleString( (const char*)ptr, size );
                break;
            case QueueType::GpuContextName:
                ptr = MemRead<uint64_t>( &item.gpuContextNameFat.ptr );
                size = MemRead<uint16_t>( &item.gpuContextNameFat.size );
                SendSingleString( (const char*)ptr, size );
                break;
            default:
                break;
            }
            AppendData( &item, QueueDataSize[idx] );
        }
        m_deferredLock.unlock();
#endif

        // Main communications loop
        int keepAlive = 0;
        for(;;)
        {
            ProcessSysTime();
            const auto status = Dequeue( token );
            const auto serialStatus = DequeueSerial();
            if( status == DequeueStatus::ConnectionLost || serialStatus == DequeueStatus::ConnectionLost )
            {
                break;
            }
            else if( status == DequeueStatus::QueueEmpty && serialStatus == DequeueStatus::QueueEmpty )
            {
                if( ShouldExit() ) break;
                if( m_bufferOffset != m_bufferStart )
                {
                    if( !CommitData() ) break;
                }
                if( keepAlive == 500 )
                {
                    QueueItem ka;
                    ka.hdr.type = QueueType::KeepAlive;
                    AppendData( &ka, QueueDataSize[ka.hdr.idx] );
                    if( !CommitData() ) break;

                    keepAlive = 0;
                }
                else if( !m_sock->HasData() )
                {
                    keepAlive++;
                    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
                }
            }
            else
            {
                keepAlive = 0;
            }

            bool connActive = true;
            while( m_sock->HasData() )
            {
                connActive = HandleServerQuery();
                if( !connActive ) break;
            }
            if( !connActive ) break;
        }
        if( ShouldExit() ) break;

        m_isConnected.store( false, std::memory_order_release );
#ifdef TRACY_ON_DEMAND
        m_bufferOffset = 0;
        m_bufferStart = 0;
#endif

        m_sock->~Socket();
        tracy_free( m_sock );
        m_sock = nullptr;

#ifndef TRACY_ON_DEMAND
        // Client is no longer available here. Accept incoming connections, but reject handshake.
        for(;;)
        {
            if( ShouldExit() )
            {
                m_shutdownFinished.store( true, std::memory_order_relaxed );
                return;
            }

            ClearQueues( token );

            m_sock = listen.Accept();
            if( m_sock )
            {
                char shibboleth[HandshakeShibbolethSize];
                auto res = m_sock->ReadRaw( shibboleth, HandshakeShibbolethSize, 1000 );
                if( !res || memcmp( shibboleth, HandshakeShibboleth, HandshakeShibbolethSize ) != 0 )
                {
                    m_sock->~Socket();
                    tracy_free( m_sock );
                    m_sock = nullptr;
                    continue;
                }

                uint32_t protocolVersion;
                res = m_sock->ReadRaw( &protocolVersion, sizeof( protocolVersion ), 1000 );
                if( !res )
                {
                    m_sock->~Socket();
                    tracy_free( m_sock );
                    m_sock = nullptr;
                    continue;
                }

                HandshakeStatus status = HandshakeNotAvailable;
                m_sock->Send( &status, sizeof( status ) );
                m_sock->~Socket();
                tracy_free( m_sock );
            }
        }
#endif
    }
    // End of connections loop

    // Wait for symbols thread to terminate. Symbol resolution will continue in this thread.
#ifdef TRACY_HAS_CALLSTACK
    while( s_symbolThreadGone.load() == false ) { YieldThread(); }
#endif

    // Client is exiting. Send items remaining in queues.
    for(;;)
    {
        const auto status = Dequeue( token );
        const auto serialStatus = DequeueSerial();
        if( status == DequeueStatus::ConnectionLost || serialStatus == DequeueStatus::ConnectionLost )
        {
            m_shutdownFinished.store( true, std::memory_order_relaxed );
            return;
        }
        else if( status == DequeueStatus::QueueEmpty && serialStatus == DequeueStatus::QueueEmpty )
        {
            if( m_bufferOffset != m_bufferStart ) CommitData();
            break;
        }

        while( m_sock->HasData() )
        {
            if( !HandleServerQuery() )
            {
                m_shutdownFinished.store( true, std::memory_order_relaxed );
                return;
            }
        }

#ifdef TRACY_HAS_CALLSTACK
        for(;;)
        {
            auto si = m_symbolQueue.front();
            if( !si ) break;
            HandleSymbolQueueItem( *si );
            m_symbolQueue.pop();
        }
#endif
    }

    // Send client termination notice to the server
    QueueItem terminate;
    MemWrite( &terminate.hdr.type, QueueType::Terminate );
    if( !SendData( (const char*)&terminate, 1 ) )
    {
        m_shutdownFinished.store( true, std::memory_order_relaxed );
        return;
    }
    // Handle remaining server queries
    for(;;)
    {
        while( m_sock->HasData() )
        {
            if( !HandleServerQuery() )
            {
                m_shutdownFinished.store( true, std::memory_order_relaxed );
                return;
            }
        }
#ifdef TRACY_HAS_CALLSTACK
        for(;;)
        {
            auto si = m_symbolQueue.front();
            if( !si ) break;
            HandleSymbolQueueItem( *si );
            m_symbolQueue.pop();
        }
#endif
        const auto status = Dequeue( token );
        const auto serialStatus = DequeueSerial();
        if( status == DequeueStatus::ConnectionLost || serialStatus == DequeueStatus::ConnectionLost )
        {
            m_shutdownFinished.store( true, std::memory_order_relaxed );
            return;
        }
        if( m_bufferOffset != m_bufferStart )
        {
            if( !CommitData() )
            {
                m_shutdownFinished.store( true, std::memory_order_relaxed );
                return;
            }
        }
    }
}

#ifndef TRACY_NO_FRAME_IMAGE
void Profiler::CompressWorker()
{
    ThreadExitHandler threadExitHandler;
    SetThreadName( "Tracy DXT1" );
    while( m_timeBegin.load( std::memory_order_relaxed ) == 0 ) std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

#ifdef TRACY_USE_RPMALLOC
    rpmalloc_thread_initialize();
#endif

    for(;;)
    {
        const auto shouldExit = ShouldExit();

        {
            bool lockHeld = true;
            while( !m_fiLock.try_lock() )
            {
                if( m_shutdownManual.load( std::memory_order_relaxed ) )
                {
                    lockHeld = false;
                    break;
                }
            }
            if( !m_fiQueue.empty() ) m_fiQueue.swap( m_fiDequeue );
            if( lockHeld )
            {
                m_fiLock.unlock();
            }
        }

        const auto sz = m_fiDequeue.size();
        if( sz > 0 )
        {
            auto fi = m_fiDequeue.data();
            auto end = fi + sz;
            while( fi != end )
            {
                const auto w = fi->w;
                const auto h = fi->h;
                const auto csz = size_t( w * h / 2 );
                auto etc1buf = (char*)tracy_malloc( csz );
                CompressImageDxt1( (const char*)fi->image, etc1buf, w, h );
                tracy_free( fi->image );

                TracyLfqPrepare( QueueType::FrameImage );
                MemWrite( &item->frameImageFat.image, (uint64_t)etc1buf );
                MemWrite( &item->frameImageFat.frame, fi->frame );
                MemWrite( &item->frameImageFat.w, w );
                MemWrite( &item->frameImageFat.h, h );
                uint8_t flip = fi->flip;
                MemWrite( &item->frameImageFat.flip, flip );
                TracyLfqCommit;

                fi++;
            }
            m_fiDequeue.clear();
        }
        else
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
        }

        if( shouldExit )
        {
            return;
        }
    }
}
#endif

static void FreeAssociatedMemory( const QueueItem& item )
{
    if( item.hdr.idx >= (int)QueueType::Terminate ) return;

    uint64_t ptr;
    switch( item.hdr.type )
    {
    case QueueType::ZoneText:
    case QueueType::ZoneName:
        ptr = MemRead<uint64_t>( &item.zoneTextFat.text );
        tracy_free( (void*)ptr );
        break;
    case QueueType::MessageColor:
    case QueueType::MessageColorCallstack:
        ptr = MemRead<uint64_t>( &item.messageColorFat.text );
        tracy_free( (void*)ptr );
        break;
    case QueueType::Message:
    case QueueType::MessageCallstack:
#ifndef TRACY_ON_DEMAND
    case QueueType::MessageAppInfo:
#endif
        ptr = MemRead<uint64_t>( &item.messageFat.text );
        tracy_free( (void*)ptr );
        break;
    case QueueType::ZoneBeginAllocSrcLoc:
    case QueueType::ZoneBeginAllocSrcLocCallstack:
        ptr = MemRead<uint64_t>( &item.zoneBegin.srcloc );
        tracy_free( (void*)ptr );
        break;
    case QueueType::GpuZoneBeginAllocSrcLoc:
    case QueueType::GpuZoneBeginAllocSrcLocCallstack:
    case QueueType::GpuZoneBeginAllocSrcLocSerial:
    case QueueType::GpuZoneBeginAllocSrcLocCallstackSerial:
        ptr = MemRead<uint64_t>( &item.gpuZoneBegin.srcloc );
        tracy_free( (void*)ptr );
        break;
    case QueueType::CallstackSerial:
    case QueueType::Callstack:
        ptr = MemRead<uint64_t>( &item.callstackFat.ptr );
        tracy_free( (void*)ptr );
        break;
    case QueueType::CallstackAlloc:
        ptr = MemRead<uint64_t>( &item.callstackAllocFat.nativePtr );
        tracy_free( (void*)ptr );
        ptr = MemRead<uint64_t>( &item.callstackAllocFat.ptr );
        tracy_free( (void*)ptr );
        break;
    case QueueType::CallstackSample:
    case QueueType::CallstackSampleContextSwitch:
        ptr = MemRead<uint64_t>( &item.callstackSampleFat.ptr );
        tracy_free( (void*)ptr );
        break;
    case QueueType::FrameImage:
        ptr = MemRead<uint64_t>( &item.frameImageFat.image );
        tracy_free( (void*)ptr );
        break;
#ifdef TRACY_HAS_CALLSTACK
    case QueueType::CallstackFrameSize:
    {
        InitRpmalloc();
        auto size = MemRead<uint8_t>( &item.callstackFrameSizeFat.size );
        auto data = (const CallstackEntry*)MemRead<uint64_t>( &item.callstackFrameSizeFat.data );
        for( uint8_t i=0; i<size; i++ )
        {
            const auto& frame = data[i];
            tracy_free_fast( (void*)frame.name );
            tracy_free_fast( (void*)frame.file );
        }
        tracy_free_fast( (void*)data );
        break;
    }
    case QueueType::SymbolInformation:
    {
        uint8_t needFree = MemRead<uint8_t>( &item.symbolInformationFat.needFree );
        if( needFree )
        {
            ptr = MemRead<uint64_t>( &item.symbolInformationFat.fileString );
            tracy_free( (void*)ptr );
        }
        break;
    }
    case QueueType::SymbolCodeMetadata:
        ptr = MemRead<uint64_t>( &item.symbolCodeMetadata.ptr );
        tracy_free( (void*)ptr );
        break;
#endif
#ifndef TRACY_ON_DEMAND
    case QueueType::LockName:
        ptr = MemRead<uint64_t>( &item.lockNameFat.name );
        tracy_free( (void*)ptr );
        break;
    case QueueType::GpuContextName:
        ptr = MemRead<uint64_t>( &item.gpuContextNameFat.ptr );
        tracy_free( (void*)ptr );
        break;
#endif
#ifdef TRACY_ON_DEMAND
    case QueueType::MessageAppInfo:
    case QueueType::GpuContextName:
        // Don't free memory associated with deferred messages.
        break;
#endif
#ifdef TRACY_HAS_SYSTEM_TRACING
    case QueueType::ExternalNameMetadata:
        ptr = MemRead<uint64_t>( &item.externalNameMetadata.name );
        tracy_free( (void*)ptr );
        ptr = MemRead<uint64_t>( &item.externalNameMetadata.threadName );
        tracy_free_fast( (void*)ptr );
        break;
#endif
    case QueueType::SourceCodeMetadata:
        ptr = MemRead<uint64_t>( &item.sourceCodeMetadata.ptr );
        tracy_free( (void*)ptr );
        break;
    default:
        break;
    }
}

void Profiler::ClearQueues( moodycamel::ConsumerToken& token )
{
    for(;;)
    {
        const auto sz = GetQueue().try_dequeue_bulk_single( token, [](const uint64_t&){}, []( QueueItem* item, size_t sz ) { assert( sz > 0 ); while( sz-- > 0 ) FreeAssociatedMemory( *item++ ); } );
        if( sz == 0 ) break;
    }

    ClearSerial();
}

void Profiler::ClearSerial()
{
    bool lockHeld = true;
    while( !m_serialLock.try_lock() )
    {
        if( m_shutdownManual.load( std::memory_order_relaxed ) )
        {
            lockHeld = false;
            break;
        }
    }
    for( auto& v : m_serialQueue ) FreeAssociatedMemory( v );
    m_serialQueue.clear();
    if( lockHeld )
    {
        m_serialLock.unlock();
    }

    for( auto& v : m_serialDequeue ) FreeAssociatedMemory( v );
    m_serialDequeue.clear();
}

Profiler::DequeueStatus Profiler::Dequeue( moodycamel::ConsumerToken& token )
{
    bool connectionLost = false;
    const auto sz = GetQueue().try_dequeue_bulk_single( token,
        [this, &connectionLost] ( const uint32_t& threadId )
        {
            if( ThreadCtxCheck( threadId ) == ThreadCtxStatus::ConnectionLost ) connectionLost = true;
        },
        [this, &connectionLost] ( QueueItem* item, size_t sz )
        {
            if( connectionLost ) return;
            InitRpmalloc();
            assert( sz > 0 );
            int64_t refThread = m_refTimeThread;
            int64_t refCtx = m_refTimeCtx;
            int64_t refGpu = m_refTimeGpu;
            while( sz-- > 0 )
            {
                uint64_t ptr;
                uint16_t size;
                auto idx = MemRead<uint8_t>( &item->hdr.idx );
                if( idx < (int)QueueType::Terminate )
                {
                    switch( (QueueType)idx )
                    {
                    case QueueType::ZoneText:
                    case QueueType::ZoneName:
                        ptr = MemRead<uint64_t>( &item->zoneTextFat.text );
                        size = MemRead<uint16_t>( &item->zoneTextFat.size );
                        SendSingleString( (const char*)ptr, size );
                        tracy_free_fast( (void*)ptr );
                        break;
                    case QueueType::Message:
                    case QueueType::MessageCallstack:
                        ptr = MemRead<uint64_t>( &item->messageFat.text );
                        size = MemRead<uint16_t>( &item->messageFat.size );
                        SendSingleString( (const char*)ptr, size );
                        tracy_free_fast( (void*)ptr );
                        break;
                    case QueueType::MessageColor:
                    case QueueType::MessageColorCallstack:
                        ptr = MemRead<uint64_t>( &item->messageColorFat.text );
                        size = MemRead<uint16_t>( &item->messageColorFat.size );
                        SendSingleString( (const char*)ptr, size );
                        tracy_free_fast( (void*)ptr );
                        break;
                    case QueueType::MessageAppInfo:
                        ptr = MemRead<uint64_t>( &item->messageFat.text );
                        size = MemRead<uint16_t>( &item->messageFat.size );
                        SendSingleString( (const char*)ptr, size );
#ifndef TRACY_ON_DEMAND
                        tracy_free_fast( (void*)ptr );
#endif
                        break;
                    case QueueType::ZoneBeginAllocSrcLoc:
                    case QueueType::ZoneBeginAllocSrcLocCallstack:
                    {
                        int64_t t = MemRead<int64_t>( &item->zoneBegin.time );
                        int64_t dt = t - refThread;
                        refThread = t;
                        MemWrite( &item->zoneBegin.time, dt );
                        ptr = MemRead<uint64_t>( &item->zoneBegin.srcloc );
                        SendSourceLocationPayload( ptr );
                        tracy_free_fast( (void*)ptr );
                        break;
                    }
                    case QueueType::Callstack:
                        ptr = MemRead<uint64_t>( &item->callstackFat.ptr );
                        SendCallstackPayload( ptr );
                        tracy_free_fast( (void*)ptr );
                        break;
                    case QueueType::CallstackAlloc:
                        ptr = MemRead<uint64_t>( &item->callstackAllocFat.nativePtr );
                        if( ptr != 0 )
                        {
                            CutCallstack( (void*)ptr, "lua_pcall" );
                            SendCallstackPayload( ptr );
                            tracy_free_fast( (void*)ptr );
                        }
                        ptr = MemRead<uint64_t>( &item->callstackAllocFat.ptr );
                        SendCallstackAlloc( ptr );
                        tracy_free_fast( (void*)ptr );
                        break;
                    case QueueType::CallstackSample:
                    case QueueType::CallstackSampleContextSwitch:
                    {
                        ptr = MemRead<uint64_t>( &item->callstackSampleFat.ptr );
                        SendCallstackPayload64( ptr );
                        tracy_free_fast( (void*)ptr );
                        int64_t t = MemRead<int64_t>( &item->callstackSampleFat.time );
                        int64_t dt = t - refCtx;
                        refCtx = t;
                        MemWrite( &item->callstackSampleFat.time, dt );
                        break;
                    }
                    case QueueType::FrameImage:
                    {
                        ptr = MemRead<uint64_t>( &item->frameImageFat.image );
                        const auto w = MemRead<uint16_t>( &item->frameImageFat.w );
                        const auto h = MemRead<uint16_t>( &item->frameImageFat.h );
                        const auto csz = size_t( w * h / 2 );
                        SendLongString( ptr, (const char*)ptr, csz, QueueType::FrameImageData );
                        tracy_free_fast( (void*)ptr );
                        break;
                    }
                    case QueueType::ZoneBegin:
                    case QueueType::ZoneBeginCallstack:
                    {
                        int64_t t = MemRead<int64_t>( &item->zoneBegin.time );
                        int64_t dt = t - refThread;
                        refThread = t;
                        MemWrite( &item->zoneBegin.time, dt );
                        break;
                    }
                    case QueueType::ZoneEnd:
                    {
                        int64_t t = MemRead<int64_t>( &item->zoneEnd.time );
                        int64_t dt = t - refThread;
                        refThread = t;
                        MemWrite( &item->zoneEnd.time, dt );
                        break;
                    }
                    case QueueType::GpuZoneBegin:
                    case QueueType::GpuZoneBeginCallstack:
                    {
                        int64_t t = MemRead<int64_t>( &item->gpuZoneBegin.cpuTime );
                        int64_t dt = t - refThread;
                        refThread = t;
                        MemWrite( &item->gpuZoneBegin.cpuTime, dt );
                        break;
                    }
                    case QueueType::GpuZoneBeginAllocSrcLoc:
                    case QueueType::GpuZoneBeginAllocSrcLocCallstack:
                    {
                        int64_t t = MemRead<int64_t>( &item->gpuZoneBegin.cpuTime );
                        int64_t dt = t - refThread;
                        refThread = t;
                        MemWrite( &item->gpuZoneBegin.cpuTime, dt );
                        ptr = MemRead<uint64_t>( &item->gpuZoneBegin.srcloc );
                        SendSourceLocationPayload( ptr );
                        tracy_free_fast( (void*)ptr );
                        break;
                    }
                    case QueueType::GpuZoneEnd:
                    {
                        int64_t t = MemRead<int64_t>( &item->gpuZoneEnd.cpuTime );
                        int64_t dt = t - refThread;
                        refThread = t;
                        MemWrite( &item->gpuZoneEnd.cpuTime, dt );
                        break;
                    }
                    case QueueType::GpuContextName:
                        ptr = MemRead<uint64_t>( &item->gpuContextNameFat.ptr );
                        size = MemRead<uint16_t>( &item->gpuContextNameFat.size );
                        SendSingleString( (const char*)ptr, size );
#ifndef TRACY_ON_DEMAND
                        tracy_free_fast( (void*)ptr );
#endif
                        break;
                    case QueueType::PlotDataInt:
                    case QueueType::PlotDataFloat:
                    case QueueType::PlotDataDouble:
                    {
                        int64_t t = MemRead<int64_t>( &item->plotDataInt.time );
                        int64_t dt = t - refThread;
                        refThread = t;
                        MemWrite( &item->plotDataInt.time, dt );
                        break;
                    }
                    case QueueType::ContextSwitch:
                    {
                        int64_t t = MemRead<int64_t>( &item->contextSwitch.time );
                        int64_t dt = t - refCtx;
                        refCtx = t;
                        MemWrite( &item->contextSwitch.time, dt );
                        break;
                    }
                    case QueueType::ThreadWakeup:
                    {
                        int64_t t = MemRead<int64_t>( &item->threadWakeup.time );
                        int64_t dt = t - refCtx;
                        refCtx = t;
                        MemWrite( &item->threadWakeup.time, dt );
                        break;
                    }
                    case QueueType::GpuTime:
                    {
                        int64_t t = MemRead<int64_t>( &item->gpuTime.gpuTime );
                        int64_t dt = t - refGpu;
                        refGpu = t;
                        MemWrite( &item->gpuTime.gpuTime, dt );
                        break;
                    }
#ifdef TRACY_HAS_CALLSTACK
                    case QueueType::CallstackFrameSize:
                    {
                        auto data = (const CallstackEntry*)MemRead<uint64_t>( &item->callstackFrameSizeFat.data );
                        auto datasz = MemRead<uint8_t>( &item->callstackFrameSizeFat.size );
                        auto imageName = (const char*)MemRead<uint64_t>( &item->callstackFrameSizeFat.imageName );
                        SendSingleString( imageName );
                        AppendData( item++, QueueDataSize[idx] );

                        for( uint8_t i=0; i<datasz; i++ )
                        {
                            const auto& frame = data[i];

                            SendSingleString( frame.name );
                            SendSecondString( frame.file );

                            QueueItem item;
                            MemWrite( &item.hdr.type, QueueType::CallstackFrame );
                            MemWrite( &item.callstackFrame.line, frame.line );
                            MemWrite( &item.callstackFrame.symAddr, frame.symAddr );
                            MemWrite( &item.callstackFrame.symLen, frame.symLen );

                            AppendData( &item, QueueDataSize[(int)QueueType::CallstackFrame] );

                            tracy_free_fast( (void*)frame.name );
                            tracy_free_fast( (void*)frame.file );
                        }
                        tracy_free_fast( (void*)data );
                        continue;
                    }
                    case QueueType::SymbolInformation:
                    {
                        auto fileString = (const char*)MemRead<uint64_t>( &item->symbolInformationFat.fileString );
                        auto needFree = MemRead<uint8_t>( &item->symbolInformationFat.needFree );
                        SendSingleString( fileString );
                        if( needFree ) tracy_free_fast( (void*)fileString );
                        break;
                    }
                    case QueueType::SymbolCodeMetadata:
                    {
                        auto symbol = MemRead<uint64_t>( &item->symbolCodeMetadata.symbol );
                        auto ptr = (const char*)MemRead<uint64_t>( &item->symbolCodeMetadata.ptr );
                        auto size = MemRead<uint32_t>( &item->symbolCodeMetadata.size );
                        SendLongString( symbol, ptr, size, QueueType::SymbolCode );
                        tracy_free_fast( (void*)ptr );
                        ++item;
                        continue;
                    }
#endif
#ifdef TRACY_HAS_SYSTEM_TRACING
                    case QueueType::ExternalNameMetadata:
                    {
                        auto thread = MemRead<uint64_t>( &item->externalNameMetadata.thread );
                        auto name = (const char*)MemRead<uint64_t>( &item->externalNameMetadata.name );
                        auto threadName = (const char*)MemRead<uint64_t>( &item->externalNameMetadata.threadName );
                        SendString( thread, threadName, QueueType::ExternalThreadName );
                        SendString( thread, name, QueueType::ExternalName );
                        tracy_free_fast( (void*)threadName );
                        tracy_free_fast( (void*)name );
                        ++item;
                        continue;
                    }
#endif
                    case QueueType::SourceCodeMetadata:
                    {
                        auto ptr = (const char*)MemRead<uint64_t>( &item->sourceCodeMetadata.ptr );
                        auto size = MemRead<uint32_t>( &item->sourceCodeMetadata.size );
                        auto id = MemRead<uint32_t>( &item->sourceCodeMetadata.id );
                        SendLongString( (uint64_t)id, ptr, size, QueueType::SourceCode );
                        tracy_free_fast( (void*)ptr );
                        ++item;
                        continue;
                    }
                    default:
                        assert( false );
                        break;
                    }
                }
                if( !AppendData( item++, QueueDataSize[idx] ) )
                {
                    connectionLost = true;
                    m_refTimeThread = refThread;
                    m_refTimeCtx = refCtx;
                    m_refTimeGpu = refGpu;
                    return;
                }
            }
            m_refTimeThread = refThread;
            m_refTimeCtx = refCtx;
            m_refTimeGpu = refGpu;
        }
    );
    if( connectionLost ) return DequeueStatus::ConnectionLost;
    return sz > 0 ? DequeueStatus::DataDequeued : DequeueStatus::QueueEmpty;
}

Profiler::DequeueStatus Profiler::DequeueContextSwitches( tracy::moodycamel::ConsumerToken& token, int64_t& timeStop )
{
    const auto sz = GetQueue().try_dequeue_bulk_single( token, [] ( const uint64_t& ) {},
        [this, &timeStop] ( QueueItem* item, size_t sz )
        {
            assert( sz > 0 );
            int64_t refCtx = m_refTimeCtx;
            while( sz-- > 0 )
            {
                FreeAssociatedMemory( *item );
                if( timeStop < 0 ) return;
                const auto idx = MemRead<uint8_t>( &item->hdr.idx );
                if( idx == (uint8_t)QueueType::ContextSwitch )
                {
                    const auto csTime = MemRead<int64_t>( &item->contextSwitch.time );
                    if( csTime > timeStop )
                    {
                        timeStop = -1;
                        m_refTimeCtx = refCtx;
                        return;
                    }
                    int64_t dt = csTime - refCtx;
                    refCtx = csTime;
                    MemWrite( &item->contextSwitch.time, dt );
                    if( !AppendData( item, QueueDataSize[(int)QueueType::ContextSwitch] ) )
                    {
                        timeStop = -2;
                        m_refTimeCtx = refCtx;
                        return;
                    }
                }
                else if( idx == (uint8_t)QueueType::ThreadWakeup )
                {
                    const auto csTime = MemRead<int64_t>( &item->threadWakeup.time );
                    if( csTime > timeStop )
                    {
                        timeStop = -1;
                        m_refTimeCtx = refCtx;
                        return;
                    }
                    int64_t dt = csTime - refCtx;
                    refCtx = csTime;
                    MemWrite( &item->threadWakeup.time, dt );
                    if( !AppendData( item, QueueDataSize[(int)QueueType::ThreadWakeup] ) )
                    {
                        timeStop = -2;
                        m_refTimeCtx = refCtx;
                        return;
                    }
                }
                item++;
            }
            m_refTimeCtx = refCtx;
        }
    );

    if( timeStop == -2 ) return DequeueStatus::ConnectionLost;
    return ( timeStop == -1 || sz > 0 ) ? DequeueStatus::DataDequeued : DequeueStatus::QueueEmpty;
}

#define ThreadCtxCheckSerial( _name ) \
    uint32_t thread = MemRead<uint32_t>( &item->_name.thread ); \
    switch( ThreadCtxCheck( thread ) ) \
    { \
    case ThreadCtxStatus::Same: break; \
    case ThreadCtxStatus::Changed: assert( m_refTimeThread == 0 ); refThread = 0; break; \
    case ThreadCtxStatus::ConnectionLost: return DequeueStatus::ConnectionLost; \
    default: assert( false ); break; \
    }

Profiler::DequeueStatus Profiler::DequeueSerial()
{
    {
        bool lockHeld = true;
        while( !m_serialLock.try_lock() )
        {
            if( m_shutdownManual.load( std::memory_order_relaxed ) )
            {
                lockHeld = false;
                break;
            }
        }
        if( !m_serialQueue.empty() ) m_serialQueue.swap( m_serialDequeue );
        if( lockHeld )
        {
            m_serialLock.unlock();
        }
    }

    const auto sz = m_serialDequeue.size();
    if( sz > 0 )
    {
        InitRpmalloc();
        int64_t refSerial = m_refTimeSerial;
        int64_t refGpu = m_refTimeGpu;
#ifdef TRACY_FIBERS
        int64_t refThread = m_refTimeThread;
#endif
        auto item = m_serialDequeue.data();
        auto end = item + sz;
        while( item != end )
        {
            uint64_t ptr;
            auto idx = MemRead<uint8_t>( &item->hdr.idx );
            if( idx < (int)QueueType::Terminate )
            {
                switch( (QueueType)idx )
                {
                case QueueType::CallstackSerial:
                    ptr = MemRead<uint64_t>( &item->callstackFat.ptr );
                    SendCallstackPayload( ptr );
                    tracy_free_fast( (void*)ptr );
                    break;
                case QueueType::LockWait:
                case QueueType::LockSharedWait:
                {
                    int64_t t = MemRead<int64_t>( &item->lockWait.time );
                    int64_t dt = t - refSerial;
                    refSerial = t;
                    MemWrite( &item->lockWait.time, dt );
                    break;
                }
                case QueueType::LockObtain:
                case QueueType::LockSharedObtain:
                {
                    int64_t t = MemRead<int64_t>( &item->lockObtain.time );
                    int64_t dt = t - refSerial;
                    refSerial = t;
                    MemWrite( &item->lockObtain.time, dt );
                    break;
                }
                case QueueType::LockRelease:
                case QueueType::LockSharedRelease:
                {
                    int64_t t = MemRead<int64_t>( &item->lockRelease.time );
                    int64_t dt = t - refSerial;
                    refSerial = t;
                    MemWrite( &item->lockRelease.time, dt );
                    break;
                }
                case QueueType::LockName:
                {
                    ptr = MemRead<uint64_t>( &item->lockNameFat.name );
                    uint16_t size = MemRead<uint16_t>( &item->lockNameFat.size );
                    SendSingleString( (const char*)ptr, size );
#ifndef TRACY_ON_DEMAND
                    tracy_free_fast( (void*)ptr );
#endif
                    break;
                }
                case QueueType::MemAlloc:
                case QueueType::MemAllocNamed:
                case QueueType::MemAllocCallstack:
                case QueueType::MemAllocCallstackNamed:
                {
                    int64_t t = MemRead<int64_t>( &item->memAlloc.time );
                    int64_t dt = t - refSerial;
                    refSerial = t;
                    MemWrite( &item->memAlloc.time, dt );
                    break;
                }
                case QueueType::MemFree:
                case QueueType::MemFreeNamed:
                case QueueType::MemFreeCallstack:
                case QueueType::MemFreeCallstackNamed:
                {
                    int64_t t = MemRead<int64_t>( &item->memFree.time );
                    int64_t dt = t - refSerial;
                    refSerial = t;
                    MemWrite( &item->memFree.time, dt );
                    break;
                }
                case QueueType::GpuZoneBeginSerial:
                case QueueType::GpuZoneBeginCallstackSerial:
                {
                    int64_t t = MemRead<int64_t>( &item->gpuZoneBegin.cpuTime );
                    int64_t dt = t - refSerial;
                    refSerial = t;
                    MemWrite( &item->gpuZoneBegin.cpuTime, dt );
                    break;
                }
                case QueueType::GpuZoneBeginAllocSrcLocSerial:
                case QueueType::GpuZoneBeginAllocSrcLocCallstackSerial:
                {
                    int64_t t = MemRead<int64_t>( &item->gpuZoneBegin.cpuTime );
                    int64_t dt = t - refSerial;
                    refSerial = t;
                    MemWrite( &item->gpuZoneBegin.cpuTime, dt );
                    ptr = MemRead<uint64_t>( &item->gpuZoneBegin.srcloc );
                    SendSourceLocationPayload( ptr );
                    tracy_free_fast( (void*)ptr );
                    break;
                }
                case QueueType::GpuZoneEndSerial:
                {
                    int64_t t = MemRead<int64_t>( &item->gpuZoneEnd.cpuTime );
                    int64_t dt = t - refSerial;
                    refSerial = t;
                    MemWrite( &item->gpuZoneEnd.cpuTime, dt );
                    break;
                }
                case QueueType::GpuTime:
                {
                    int64_t t = MemRead<int64_t>( &item->gpuTime.gpuTime );
                    int64_t dt = t - refGpu;
                    refGpu = t;
                    MemWrite( &item->gpuTime.gpuTime, dt );
                    break;
                }
                case QueueType::GpuContextName:
                {
                    ptr = MemRead<uint64_t>( &item->gpuContextNameFat.ptr );
                    uint16_t size = MemRead<uint16_t>( &item->gpuContextNameFat.size );
                    SendSingleString( (const char*)ptr, size );
#ifndef TRACY_ON_DEMAND
                    tracy_free_fast( (void*)ptr );
#endif
                    break;
                }
#ifdef TRACY_FIBERS
                case QueueType::ZoneBegin:
                case QueueType::ZoneBeginCallstack:
                {
                    ThreadCtxCheckSerial( zoneBeginThread );
                    int64_t t = MemRead<int64_t>( &item->zoneBegin.time );
                    int64_t dt = t - refThread;
                    refThread = t;
                    MemWrite( &item->zoneBegin.time, dt );
                    break;
                }
                case QueueType::ZoneBeginAllocSrcLoc:
                case QueueType::ZoneBeginAllocSrcLocCallstack:
                {
                    ThreadCtxCheckSerial( zoneBeginThread );
                    int64_t t = MemRead<int64_t>( &item->zoneBegin.time );
                    int64_t dt = t - refThread;
                    refThread = t;
                    MemWrite( &item->zoneBegin.time, dt );
                    ptr = MemRead<uint64_t>( &item->zoneBegin.srcloc );
                    SendSourceLocationPayload( ptr );
                    tracy_free_fast( (void*)ptr );
                    break;
                }
                case QueueType::ZoneEnd:
                {
                    ThreadCtxCheckSerial( zoneEndThread );
                    int64_t t = MemRead<int64_t>( &item->zoneEnd.time );
                    int64_t dt = t - refThread;
                    refThread = t;
                    MemWrite( &item->zoneEnd.time, dt );
                    break;
                }
                case QueueType::ZoneText:
                case QueueType::ZoneName:
                {
                    ThreadCtxCheckSerial( zoneTextFatThread );
                    ptr = MemRead<uint64_t>( &item->zoneTextFat.text );
                    uint16_t size = MemRead<uint16_t>( &item->zoneTextFat.size );
                    SendSingleString( (const char*)ptr, size );
                    tracy_free_fast( (void*)ptr );
                    break;
                }
                case QueueType::Message:
                case QueueType::MessageCallstack:
                {
                    ThreadCtxCheckSerial( messageFatThread );
                    ptr = MemRead<uint64_t>( &item->messageFat.text );
                    uint16_t size = MemRead<uint16_t>( &item->messageFat.size );
                    SendSingleString( (const char*)ptr, size );
                    tracy_free_fast( (void*)ptr );
                    break;
                }
                case QueueType::MessageColor:
                case QueueType::MessageColorCallstack:
                {
                    ThreadCtxCheckSerial( messageColorFatThread );
                    ptr = MemRead<uint64_t>( &item->messageColorFat.text );
                    uint16_t size = MemRead<uint16_t>( &item->messageColorFat.size );
                    SendSingleString( (const char*)ptr, size );
                    tracy_free_fast( (void*)ptr );
                    break;
                }
                case QueueType::Callstack:
                {
                    ThreadCtxCheckSerial( callstackFatThread );
                    ptr = MemRead<uint64_t>( &item->callstackFat.ptr );
                    SendCallstackPayload( ptr );
                    tracy_free_fast( (void*)ptr );
                    break;
                }
                case QueueType::CallstackAlloc:
                {
                    ThreadCtxCheckSerial( callstackAllocFatThread );
                    ptr = MemRead<uint64_t>( &item->callstackAllocFat.nativePtr );
                    if( ptr != 0 )
                    {
                        CutCallstack( (void*)ptr, "lua_pcall" );
                        SendCallstackPayload( ptr );
                        tracy_free_fast( (void*)ptr );
                    }
                    ptr = MemRead<uint64_t>( &item->callstackAllocFat.ptr );
                    SendCallstackAlloc( ptr );
                    tracy_free_fast( (void*)ptr );
                    break;
                }
                case QueueType::FiberEnter:
                {
                    ThreadCtxCheckSerial( fiberEnter );
                    int64_t t = MemRead<int64_t>( &item->fiberEnter.time );
                    int64_t dt = t - refThread;
                    refThread = t;
                    MemWrite( &item->fiberEnter.time, dt );
                    break;
                }
                case QueueType::FiberLeave:
                {
                    ThreadCtxCheckSerial( fiberLeave );
                    int64_t t = MemRead<int64_t>( &item->fiberLeave.time );
                    int64_t dt = t - refThread;
                    refThread = t;
                    MemWrite( &item->fiberLeave.time, dt );
                    break;
                }
#endif
                default:
                    assert( false );
                    break;
                }
            }
#ifdef TRACY_FIBERS
            else
            {
                switch( (QueueType)idx )
                {
                case QueueType::ZoneColor:
                {
                    ThreadCtxCheckSerial( zoneColorThread );
                    break;
                }
                case QueueType::ZoneValue:
                {
                    ThreadCtxCheckSerial( zoneValueThread );
                    break;
                }
                case QueueType::ZoneValidation:
                {
                    ThreadCtxCheckSerial( zoneValidationThread );
                    break;
                }
                case QueueType::MessageLiteral:
                case QueueType::MessageLiteralCallstack:
                {
                    ThreadCtxCheckSerial( messageLiteralThread );
                    break;
                }
                case QueueType::MessageLiteralColor:
                case QueueType::MessageLiteralColorCallstack:
                {
                    ThreadCtxCheckSerial( messageColorLiteralThread );
                    break;
                }
                case QueueType::CrashReport:
                {
                    ThreadCtxCheckSerial( crashReportThread );
                    break;
                }
                default:
                    break;
                }
            }
#endif
            if( !AppendData( item, QueueDataSize[idx] ) ) return DequeueStatus::ConnectionLost;
            item++;
        }
        m_refTimeSerial = refSerial;
        m_refTimeGpu = refGpu;
#ifdef TRACY_FIBERS
        m_refTimeThread = refThread;
#endif
        m_serialDequeue.clear();
    }
    else
    {
        return DequeueStatus::QueueEmpty;
    }
    return DequeueStatus::DataDequeued;
}

Profiler::ThreadCtxStatus Profiler::ThreadCtxCheck( uint32_t threadId )
{
    if( m_threadCtx == threadId ) return ThreadCtxStatus::Same;
    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::ThreadContext );
    MemWrite( &item.threadCtx.thread, threadId );
    if( !AppendData( &item, QueueDataSize[(int)QueueType::ThreadContext] ) ) return ThreadCtxStatus::ConnectionLost;
    m_threadCtx = threadId;
    m_refTimeThread = 0;
    return ThreadCtxStatus::Changed;
}

bool Profiler::CommitData()
{
    bool ret = SendData( m_buffer + m_bufferStart, m_bufferOffset - m_bufferStart );
    if( m_bufferOffset > TargetFrameSize * 2 ) m_bufferOffset = 0;
    m_bufferStart = m_bufferOffset;
    return ret;
}

bool Profiler::SendData( const char* data, size_t len )
{
    const lz4sz_t lz4sz = LZ4_compress_fast_continue( (LZ4_stream_t*)m_stream, data, m_lz4Buf + sizeof( lz4sz_t ), (int)len, LZ4Size, 1 );
    memcpy( m_lz4Buf, &lz4sz, sizeof( lz4sz ) );
    return m_sock->Send( m_lz4Buf, lz4sz + sizeof( lz4sz_t ) ) != -1;
}

void Profiler::SendString( uint64_t str, const char* ptr, size_t len, QueueType type )
{
    assert( type == QueueType::StringData ||
            type == QueueType::ThreadName ||
            type == QueueType::PlotName ||
            type == QueueType::FrameName ||
            type == QueueType::ExternalName ||
            type == QueueType::ExternalThreadName ||
            type == QueueType::FiberName );

    QueueItem item;
    MemWrite( &item.hdr.type, type );
    MemWrite( &item.stringTransfer.ptr, str );

    assert( len <= std::numeric_limits<uint16_t>::max() );
    auto l16 = uint16_t( len );

    NeedDataSize( QueueDataSize[(int)type] + sizeof( l16 ) + l16 );

    AppendDataUnsafe( &item, QueueDataSize[(int)type] );
    AppendDataUnsafe( &l16, sizeof( l16 ) );
    AppendDataUnsafe( ptr, l16 );
}

void Profiler::SendSingleString( const char* ptr, size_t len )
{
    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::SingleStringData );

    assert( len <= std::numeric_limits<uint16_t>::max() );
    auto l16 = uint16_t( len );

    NeedDataSize( QueueDataSize[(int)QueueType::SingleStringData] + sizeof( l16 ) + l16 );

    AppendDataUnsafe( &item, QueueDataSize[(int)QueueType::SingleStringData] );
    AppendDataUnsafe( &l16, sizeof( l16 ) );
    AppendDataUnsafe( ptr, l16 );
}

void Profiler::SendSecondString( const char* ptr, size_t len )
{
    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::SecondStringData );

    assert( len <= std::numeric_limits<uint16_t>::max() );
    auto l16 = uint16_t( len );

    NeedDataSize( QueueDataSize[(int)QueueType::SecondStringData] + sizeof( l16 ) + l16 );

    AppendDataUnsafe( &item, QueueDataSize[(int)QueueType::SecondStringData] );
    AppendDataUnsafe( &l16, sizeof( l16 ) );
    AppendDataUnsafe( ptr, l16 );
}

void Profiler::SendLongString( uint64_t str, const char* ptr, size_t len, QueueType type )
{
    assert( type == QueueType::FrameImageData ||
            type == QueueType::SymbolCode ||
            type == QueueType::SourceCode );

    QueueItem item;
    MemWrite( &item.hdr.type, type );
    MemWrite( &item.stringTransfer.ptr, str );

    assert( len <= std::numeric_limits<uint32_t>::max() );
    assert( QueueDataSize[(int)type] + sizeof( uint32_t ) + len <= TargetFrameSize );
    auto l32 = uint32_t( len );

    NeedDataSize( QueueDataSize[(int)type] + sizeof( l32 ) + l32 );

    AppendDataUnsafe( &item, QueueDataSize[(int)type] );
    AppendDataUnsafe( &l32, sizeof( l32 ) );
    AppendDataUnsafe( ptr, l32 );
}

void Profiler::SendSourceLocation( uint64_t ptr )
{
    auto srcloc = (const SourceLocationData*)ptr;
    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::SourceLocation );
    MemWrite( &item.srcloc.name, (uint64_t)srcloc->name );
    MemWrite( &item.srcloc.file, (uint64_t)srcloc->file );
    MemWrite( &item.srcloc.function, (uint64_t)srcloc->function );
    MemWrite( &item.srcloc.line, srcloc->line );
    MemWrite( &item.srcloc.b, uint8_t( ( srcloc->color       ) & 0xFF ) );
    MemWrite( &item.srcloc.g, uint8_t( ( srcloc->color >> 8  ) & 0xFF ) );
    MemWrite( &item.srcloc.r, uint8_t( ( srcloc->color >> 16 ) & 0xFF ) );
    AppendData( &item, QueueDataSize[(int)QueueType::SourceLocation] );
}

void Profiler::SendSourceLocationPayload( uint64_t _ptr )
{
    auto ptr = (const char*)_ptr;

    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::SourceLocationPayload );
    MemWrite( &item.stringTransfer.ptr, _ptr );

    uint16_t len;
    memcpy( &len, ptr, sizeof( len ) );
    assert( len > 2 );
    len -= 2;
    ptr += 2;

    NeedDataSize( QueueDataSize[(int)QueueType::SourceLocationPayload] + sizeof( len ) + len );

    AppendDataUnsafe( &item, QueueDataSize[(int)QueueType::SourceLocationPayload] );
    AppendDataUnsafe( &len, sizeof( len ) );
    AppendDataUnsafe( ptr, len );
}

void Profiler::SendCallstackPayload( uint64_t _ptr )
{
    auto ptr = (uintptr_t*)_ptr;

    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::CallstackPayload );
    MemWrite( &item.stringTransfer.ptr, _ptr );

    const auto sz = *ptr++;
    const auto len = sz * sizeof( uint64_t );
    const auto l16 = uint16_t( len );

    NeedDataSize( QueueDataSize[(int)QueueType::CallstackPayload] + sizeof( l16 ) + l16 );

    AppendDataUnsafe( &item, QueueDataSize[(int)QueueType::CallstackPayload] );
    AppendDataUnsafe( &l16, sizeof( l16 ) );

    if( compile_time_condition<sizeof( uintptr_t ) == sizeof( uint64_t )>::value )
    {
        AppendDataUnsafe( ptr, sizeof( uint64_t ) * sz );
    }
    else
    {
        for( uintptr_t i=0; i<sz; i++ )
        {
            const auto val = uint64_t( *ptr++ );
            AppendDataUnsafe( &val, sizeof( uint64_t ) );
        }
    }
}

void Profiler::SendCallstackPayload64( uint64_t _ptr )
{
    auto ptr = (uint64_t*)_ptr;

    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::CallstackPayload );
    MemWrite( &item.stringTransfer.ptr, _ptr );

    const auto sz = *ptr++;
    const auto len = sz * sizeof( uint64_t );
    const auto l16 = uint16_t( len );

    NeedDataSize( QueueDataSize[(int)QueueType::CallstackPayload] + sizeof( l16 ) + l16 );

    AppendDataUnsafe( &item, QueueDataSize[(int)QueueType::CallstackPayload] );
    AppendDataUnsafe( &l16, sizeof( l16 ) );
    AppendDataUnsafe( ptr, sizeof( uint64_t ) * sz );
}

void Profiler::SendCallstackAlloc( uint64_t _ptr )
{
    auto ptr = (const char*)_ptr;

    QueueItem item;
    MemWrite( &item.hdr.type, QueueType::CallstackAllocPayload );
    MemWrite( &item.stringTransfer.ptr, _ptr );

    uint16_t len;
    memcpy( &len, ptr, 2 );
    ptr += 2;

    NeedDataSize( QueueDataSize[(int)QueueType::CallstackAllocPayload] + sizeof( len ) + len );

    AppendDataUnsafe( &item, QueueDataSize[(int)QueueType::CallstackAllocPayload] );
    AppendDataUnsafe( &len, sizeof( len ) );
    AppendDataUnsafe( ptr, len );
}

void Profiler::QueueCallstackFrame( uint64_t ptr )
{
#ifdef TRACY_HAS_CALLSTACK
    m_symbolQueue.emplace( SymbolQueueItem { SymbolQueueItemType::CallstackFrame, ptr } );
#else
    AckServerQuery();
#endif
}

void Profiler::QueueSymbolQuery( uint64_t symbol )
{
#ifdef TRACY_HAS_CALLSTACK
    // Special handling for kernel frames
    if( symbol >> 63 != 0 )
    {
        SendSingleString( "<kernel>" );
        QueueItem item;
        MemWrite( &item.hdr.type, QueueType::SymbolInformation );
        MemWrite( &item.symbolInformation.line, 0 );
        MemWrite( &item.symbolInformation.symAddr, symbol );
        AppendData( &item, QueueDataSize[(int)QueueType::SymbolInformation] );
    }
    else
    {
        m_symbolQueue.emplace( SymbolQueueItem { SymbolQueueItemType::SymbolQuery, symbol } );
    }
#else
    AckServerQuery();
#endif
}

void Profiler::QueueExternalName( uint64_t ptr )
{
#ifdef TRACY_HAS_SYSTEM_TRACING
    m_symbolQueue.emplace( SymbolQueueItem { SymbolQueueItemType::ExternalName, ptr } );
#endif
}

void Profiler::QueueKernelCode( uint64_t symbol, uint32_t size )
{
    assert( symbol >> 63 != 0 );
#ifdef TRACY_HAS_CALLSTACK
    m_symbolQueue.emplace( SymbolQueueItem { SymbolQueueItemType::KernelCode, symbol, size } );
#else
    AckSymbolCodeNotAvailable();
#endif
}

void Profiler::QueueSourceCodeQuery( uint32_t id )
{
    assert( m_exectime != 0 );
    assert( m_queryData );
    m_symbolQueue.emplace( SymbolQueueItem { SymbolQueueItemType::SourceCode, uint64_t( m_queryData ), uint64_t( m_queryImage ), id } );
    m_queryData = nullptr;
    m_queryImage = nullptr;
}

#ifdef TRACY_HAS_CALLSTACK
void Profiler::HandleSymbolQueueItem( const SymbolQueueItem& si )
{
    switch( si.type )
    {
    case SymbolQueueItemType::CallstackFrame:
    {
        const auto frameData = DecodeCallstackPtr( si.ptr );
        auto data = tracy_malloc_fast( sizeof( CallstackEntry ) * frameData.size );
        memcpy( data, frameData.data, sizeof( CallstackEntry ) * frameData.size );
        TracyLfqPrepare( QueueType::CallstackFrameSize );
        MemWrite( &item->callstackFrameSizeFat.ptr, si.ptr );
        MemWrite( &item->callstackFrameSizeFat.size, frameData.size );
        MemWrite( &item->callstackFrameSizeFat.data, (uint64_t)data );
        MemWrite( &item->callstackFrameSizeFat.imageName, (uint64_t)frameData.imageName );
        TracyLfqCommit;
        break;
    }
    case SymbolQueueItemType::SymbolQuery:
    {
#ifdef __ANDROID__
        // On Android it's common for code to be in mappings that are only executable
        // but not readable.
        if( !EnsureReadable( si.ptr ) )
        {
            TracyLfqPrepare( QueueType::AckServerQueryNoop );
            TracyLfqCommit;
            break;
        }
#endif
        const auto sym = DecodeSymbolAddress( si.ptr );
        TracyLfqPrepare( QueueType::SymbolInformation );
        MemWrite( &item->symbolInformationFat.line, sym.line );
        MemWrite( &item->symbolInformationFat.symAddr, si.ptr );
        MemWrite( &item->symbolInformationFat.fileString, (uint64_t)sym.file );
        MemWrite( &item->symbolInformationFat.needFree, (uint8_t)sym.needFree );
        TracyLfqCommit;
        break;
    }
#ifdef TRACY_HAS_SYSTEM_TRACING
    case SymbolQueueItemType::ExternalName:
    {
        const char* threadName;
        const char* name;
        SysTraceGetExternalName( si.ptr, threadName, name );
        TracyLfqPrepare( QueueType::ExternalNameMetadata );
        MemWrite( &item->externalNameMetadata.thread, si.ptr );
        MemWrite( &item->externalNameMetadata.name, (uint64_t)name );
        MemWrite( &item->externalNameMetadata.threadName, (uint64_t)threadName );
        TracyLfqCommit;
        break;
    }
#endif
    case SymbolQueueItemType::KernelCode:
    {
#ifdef _WIN32
        auto mod = GetKernelModulePath( si.ptr );
        if( mod )
        {
            auto fn = DecodeCallstackPtrFast( si.ptr );
            if( *fn )
            {
                auto hnd = LoadLibraryExA( mod, nullptr, DONT_RESOLVE_DLL_REFERENCES );
                if( hnd )
                {
                    auto ptr = (const void*)GetProcAddress( hnd, fn );
                    if( ptr )
                    {
                        auto buf = (char*)tracy_malloc( si.extra );
                        memcpy( buf, ptr, si.extra );
                        FreeLibrary( hnd );
                        TracyLfqPrepare( QueueType::SymbolCodeMetadata );
                        MemWrite( &item->symbolCodeMetadata.symbol, si.ptr );
                        MemWrite( &item->symbolCodeMetadata.ptr, (uint64_t)buf );
                        MemWrite( &item->symbolCodeMetadata.size, (uint32_t)si.extra );
                        TracyLfqCommit;
                        break;
                    }
                    FreeLibrary( hnd );
                }
            }
        }
#endif
        TracyLfqPrepare( QueueType::AckSymbolCodeNotAvailable );
        TracyLfqCommit;
        break;
    }
    case SymbolQueueItemType::SourceCode:
        HandleSourceCodeQuery( (char*)si.ptr, (char*)si.extra, si.id );
        break;
    default:
        assert( false );
        break;
    }
}

void Profiler::SymbolWorker()
{
#if defined __linux__ && !defined TRACY_NO_CRASH_HANDLER
    s_symbolTid = syscall( SYS_gettid );
#endif

    ThreadExitHandler threadExitHandler;
    SetThreadName( "Tracy Symbol Worker" );
#ifdef TRACY_USE_RPMALLOC
    InitRpmalloc();
#endif
    InitCallstack();
    while( m_timeBegin.load( std::memory_order_relaxed ) == 0 ) std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

    for(;;)
    {
        const auto shouldExit = ShouldExit();
#ifdef TRACY_ON_DEMAND
        if( !IsConnected() )
        {
            if( shouldExit )
            {
                s_symbolThreadGone.store( true, std::memory_order_release );
                return;
            }
            while( m_symbolQueue.front() ) m_symbolQueue.pop();
            std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
            continue;
        }
#endif
        auto si = m_symbolQueue.front();
        if( si )
        {
            HandleSymbolQueueItem( *si );
            m_symbolQueue.pop();
        }
        else
        {
            if( shouldExit )
            {
                s_symbolThreadGone.store( true, std::memory_order_release );
                return;
            }
            std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
        }
    }
}
#endif

bool Profiler::HandleServerQuery()
{
    ServerQueryPacket payload;
    if( !m_sock->Read( &payload, sizeof( payload ), 10 ) ) return false;

    uint8_t type;
    uint64_t ptr;
    memcpy( &type, &payload.type, sizeof( payload.type ) );
    memcpy( &ptr, &payload.ptr, sizeof( payload.ptr ) );

    switch( type )
    {
    case ServerQueryString:
        SendString( ptr, (const char*)ptr, QueueType::StringData );
        break;
    case ServerQueryThreadString:
        if( ptr == m_mainThread )
        {
            SendString( ptr, "Main thread", 11, QueueType::ThreadName );
        }
        else
        {
            SendString( ptr, GetThreadName( ptr ), QueueType::ThreadName );
        }
        break;
    case ServerQuerySourceLocation:
        SendSourceLocation( ptr );
        break;
    case ServerQueryPlotName:
        SendString( ptr, (const char*)ptr, QueueType::PlotName );
        break;
    case ServerQueryTerminate:
        return false;
    case ServerQueryCallstackFrame:
        QueueCallstackFrame( ptr );
        break;
    case ServerQueryFrameName:
        SendString( ptr, (const char*)ptr, QueueType::FrameName );
        break;
    case ServerQueryDisconnect:
        HandleDisconnect();
        return false;
#ifdef TRACY_HAS_SYSTEM_TRACING
    case ServerQueryExternalName:
        QueueExternalName( ptr );
        break;
#endif
    case ServerQueryParameter:
        HandleParameter( ptr );
        break;
    case ServerQuerySymbol:
        QueueSymbolQuery( ptr );
        break;
#ifndef TRACY_NO_CODE_TRANSFER
    case ServerQuerySymbolCode:
        HandleSymbolCodeQuery( ptr, payload.extra );
        break;
#endif
    case ServerQuerySourceCode:
        QueueSourceCodeQuery( uint32_t( ptr ) );
        break;
    case ServerQueryDataTransfer:
        if( m_queryData )
        {
            assert( !m_queryImage );
            m_queryImage = m_queryData;
        }
        m_queryDataPtr = m_queryData = (char*)tracy_malloc( ptr + 11 );
        AckServerQuery();
        break;
    case ServerQueryDataTransferPart:
        memcpy( m_queryDataPtr, &ptr, 8 );
        memcpy( m_queryDataPtr+8, &payload.extra, 4 );
        m_queryDataPtr += 12;
        AckServerQuery();
        break;
#ifdef TRACY_FIBERS
    case ServerQueryFiberName:
        SendString( ptr, (const char*)ptr, QueueType::FiberName );
        break;
#endif
    default:
        assert( false );
        break;
    }

    return true;
}

void Profiler::HandleDisconnect()
{
    moodycamel::ConsumerToken token( GetQueue() );

#ifdef TRACY_HAS_SYSTEM_TRACING
    if( s_sysTraceThread )
    {
        auto timestamp = GetTime();
        for(;;)
        {
            const auto status = DequeueContextSwitches( token, timestamp );
            if( status == DequeueStatus::ConnectionLost )
            {
                return;
            }
            else if( status == DequeueStatus::QueueEmpty )
            {
                if( m_bufferOffset != m_bufferStart )
                {
                    if( !CommitData() ) return;
                }
            }
            if( timestamp < 0 )
            {
                if( m_bufferOffset != m_bufferStart )
                {
                    if( !CommitData() ) return;
                }
                break;
            }
            ClearSerial();
            if( m_sock->HasData() )
            {
                while( m_sock->HasData() )
                {
                    if( !HandleServerQuery() ) return;
                }
                if( m_bufferOffset != m_bufferStart )
                {
                    if( !CommitData() ) return;
                }
            }
            else
            {
                if( m_bufferOffset != m_bufferStart )
                {
                    if( !CommitData() ) return;
                }
                std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
            }
        }
    }
#endif

    QueueItem terminate;
    MemWrite( &terminate.hdr.type, QueueType::Terminate );
    if( !SendData( (const char*)&terminate, 1 ) ) return;
    for(;;)
    {
        ClearQueues( token );
        if( m_sock->HasData() )
        {
            while( m_sock->HasData() )
            {
                if( !HandleServerQuery() ) return;
            }
            if( m_bufferOffset != m_bufferStart )
            {
                if( !CommitData() ) return;
            }
        }
        else
        {
            if( m_bufferOffset != m_bufferStart )
            {
                if( !CommitData() ) return;
            }
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        }
    }
}

void Profiler::CalibrateTimer()
{
    m_timerMul = 1.;

#ifdef TRACY_HW_TIMER

#  if !defined TRACY_TIMER_QPC && defined TRACY_TIMER_FALLBACK
    const bool needCalibration = HardwareSupportsInvariantTSC();
#  else
    const bool needCalibration = true;
#  endif
    if( needCalibration )
    {
        std::atomic_signal_fence( std::memory_order_acq_rel );
        const auto t0 = std::chrono::high_resolution_clock::now();
        const auto r0 = GetTime();
        std::atomic_signal_fence( std::memory_order_acq_rel );
        std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
        std::atomic_signal_fence( std::memory_order_acq_rel );
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto r1 = GetTime();
        std::atomic_signal_fence( std::memory_order_acq_rel );

        const auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>( t1 - t0 ).count();
        const auto dr = r1 - r0;

        m_timerMul = double( dt ) / double( dr );
    }
#endif
}

void Profiler::CalibrateDelay()
{
    constexpr int Iterations = 50000;

    auto mindiff = std::numeric_limits<int64_t>::max();
    for( int i=0; i<Iterations * 10; i++ )
    {
        const auto t0i = GetTime();
        const auto t1i = GetTime();
        const auto dti = t1i - t0i;
        if( dti > 0 && dti < mindiff ) mindiff = dti;
    }
    m_resolution = mindiff;

#ifdef TRACY_DELAYED_INIT
    m_delay = m_resolution;
#else
    constexpr int Events = Iterations * 2;   // start + end
    static_assert( Events < QueuePrealloc, "Delay calibration loop will allocate memory in queue" );

    static const tracy::SourceLocationData __tracy_source_location { nullptr, TracyFunction,  TracyFile, (uint32_t)TracyLine, 0 };
    const auto t0 = GetTime();
    for( int i=0; i<Iterations; i++ )
    {
        {
            TracyLfqPrepare( QueueType::ZoneBegin );
            MemWrite( &item->zoneBegin.time, Profiler::GetTime() );
            MemWrite( &item->zoneBegin.srcloc, (uint64_t)&__tracy_source_location );
            TracyLfqCommit;
        }
        {
            TracyLfqPrepare( QueueType::ZoneEnd );
            MemWrite( &item->zoneEnd.time, GetTime() );
            TracyLfqCommit;
        }
    }
    const auto t1 = GetTime();
    const auto dt = t1 - t0;
    m_delay = dt / Events;

    moodycamel::ConsumerToken token( GetQueue() );
    int left = Events;
    while( left != 0 )
    {
        const auto sz = GetQueue().try_dequeue_bulk_single( token, [](const uint64_t&){}, [](QueueItem* item, size_t sz){} );
        assert( sz > 0 );
        left -= (int)sz;
    }
    assert( GetQueue().size_approx() == 0 );
#endif
}

void Profiler::ReportTopology()
{
#ifndef TRACY_DELAYED_INIT
    struct CpuData
    {
        uint32_t package;
        uint32_t core;
        uint32_t thread;
    };

#if defined _WIN32
#  ifdef TRACY_UWP
    t_GetLogicalProcessorInformationEx _GetLogicalProcessorInformationEx = &::GetLogicalProcessorInformationEx;
#  else
    t_GetLogicalProcessorInformationEx _GetLogicalProcessorInformationEx = (t_GetLogicalProcessorInformationEx)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "GetLogicalProcessorInformationEx" );
#  endif
    if( !_GetLogicalProcessorInformationEx ) return;

    DWORD psz = 0;
    _GetLogicalProcessorInformationEx( RelationProcessorPackage, nullptr, &psz );
    auto packageInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)tracy_malloc( psz );
    auto res = _GetLogicalProcessorInformationEx( RelationProcessorPackage, packageInfo, &psz );
    assert( res );

    DWORD csz = 0;
    _GetLogicalProcessorInformationEx( RelationProcessorCore, nullptr, &csz );
    auto coreInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)tracy_malloc( csz );
    res = _GetLogicalProcessorInformationEx( RelationProcessorCore, coreInfo, &csz );
    assert( res );

    SYSTEM_INFO sysinfo;
    GetSystemInfo( &sysinfo );
    const uint32_t numcpus = sysinfo.dwNumberOfProcessors;

    auto cpuData = (CpuData*)tracy_malloc( sizeof( CpuData ) * numcpus );
    for( uint32_t i=0; i<numcpus; i++ ) cpuData[i].thread = i;

    int idx = 0;
    auto ptr = packageInfo;
    while( (char*)ptr < ((char*)packageInfo) + psz )
    {
        assert( ptr->Relationship == RelationProcessorPackage );
        // FIXME account for GroupCount
        auto mask = ptr->Processor.GroupMask[0].Mask;
        int core = 0;
        while( mask != 0 )
        {
            if( mask & 1 ) cpuData[core].package = idx;
            core++;
            mask >>= 1;
        }
        ptr = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(((char*)ptr) + ptr->Size);
        idx++;
    }

    idx = 0;
    ptr = coreInfo;
    while( (char*)ptr < ((char*)coreInfo) + csz )
    {
        assert( ptr->Relationship == RelationProcessorCore );
        // FIXME account for GroupCount
        auto mask = ptr->Processor.GroupMask[0].Mask;
        int core = 0;
        while( mask != 0 )
        {
            if( mask & 1 ) cpuData[core].core = idx;
            core++;
            mask >>= 1;
        }
        ptr = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(((char*)ptr) + ptr->Size);
        idx++;
    }

    for( uint32_t i=0; i<numcpus; i++ )
    {
        auto& data = cpuData[i];

        TracyLfqPrepare( QueueType::CpuTopology );
        MemWrite( &item->cpuTopology.package, data.package );
        MemWrite( &item->cpuTopology.core, data.core );
        MemWrite( &item->cpuTopology.thread, data.thread );

#ifdef TRACY_ON_DEMAND
        DeferItem( *item );
#endif

        TracyLfqCommit;
    }

    tracy_free( cpuData );
    tracy_free( coreInfo );
    tracy_free( packageInfo );
#elif defined __linux__
    const int numcpus = std::thread::hardware_concurrency();
    auto cpuData = (CpuData*)tracy_malloc( sizeof( CpuData ) * numcpus );
    memset( cpuData, 0, sizeof( CpuData ) * numcpus );

    const char* basePath = "/sys/devices/system/cpu/cpu";
    for( int i=0; i<numcpus; i++ )
    {
        char path[1024];
        sprintf( path, "%s%i/topology/physical_package_id", basePath, i );
        char buf[1024];
        FILE* f = fopen( path, "rb" );
        if( !f )
        {
            tracy_free( cpuData );
            return;
        }
        auto read = fread( buf, 1, 1024, f );
        buf[read] = '\0';
        fclose( f );
        cpuData[i].package = uint32_t( atoi( buf ) );
        cpuData[i].thread = i;
        sprintf( path, "%s%i/topology/core_id", basePath, i );
        f = fopen( path, "rb" );
        read = fread( buf, 1, 1024, f );
        buf[read] = '\0';
        fclose( f );
        cpuData[i].core = uint32_t( atoi( buf ) );
    }

    for( int i=0; i<numcpus; i++ )
    {
        auto& data = cpuData[i];

        TracyLfqPrepare( QueueType::CpuTopology );
        MemWrite( &item->cpuTopology.package, data.package );
        MemWrite( &item->cpuTopology.core, data.core );
        MemWrite( &item->cpuTopology.thread, data.thread );

#ifdef TRACY_ON_DEMAND
        DeferItem( *item );
#endif

        TracyLfqCommit;
    }

    tracy_free( cpuData );
#endif
#endif
}

void Profiler::SendCallstack( int depth, const char* skipBefore )
{
#ifdef TRACY_HAS_CALLSTACK
    auto ptr = Callstack( depth );
    CutCallstack( ptr, skipBefore );

    TracyQueuePrepare( QueueType::Callstack );
    MemWrite( &item->callstackFat.ptr, (uint64_t)ptr );
    TracyQueueCommit( callstackFatThread );
#endif
}

void Profiler::CutCallstack( void* callstack, const char* skipBefore )
{
#ifdef TRACY_HAS_CALLSTACK
    auto data = (uintptr_t*)callstack;
    const auto sz = *data++;
    uintptr_t i;
    for( i=0; i<sz; i++ )
    {
        auto name = DecodeCallstackPtrFast( uint64_t( data[i] ) );
        const bool found = strcmp( name, skipBefore ) == 0;
        if( found )
        {
            i++;
            break;
        }
    }

    if( i != sz )
    {
        memmove( data, data + i, ( sz - i ) * sizeof( uintptr_t* ) );
        *--data = sz - i;
    }
#endif
}

#ifdef TRACY_HAS_SYSTIME
void Profiler::ProcessSysTime()
{
    if( m_shutdown.load( std::memory_order_relaxed ) ) return;
    auto t = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    if( t - m_sysTimeLast > 100000000 )    // 100 ms
    {
        auto sysTime = m_sysTime.Get();
        if( sysTime >= 0 )
        {
            m_sysTimeLast = t;

            TracyLfqPrepare( QueueType::SysTimeReport );
            MemWrite( &item->sysTime.time, GetTime() );
            MemWrite( &item->sysTime.sysTime, sysTime );
            TracyLfqCommit;
        }
    }
}
#endif

void Profiler::HandleParameter( uint64_t payload )
{
    assert( m_paramCallback );
    const auto idx = uint32_t( payload >> 32 );
    const auto val = int32_t( payload & 0xFFFFFFFF );
    m_paramCallback( m_paramCallbackData, idx, val );
    AckServerQuery();
}

void Profiler::HandleSymbolCodeQuery( uint64_t symbol, uint32_t size )
{
    if( symbol >> 63 != 0 )
    {
        QueueKernelCode( symbol, size );
    }
    else
    {
#ifdef __ANDROID__
        // On Android it's common for code to be in mappings that are only executable
        // but not readable.
        if( !EnsureReadable( symbol ) )
        {
            AckSymbolCodeNotAvailable();
            return;
        }
#endif
        SendLongString( symbol, (const char*)symbol, size, QueueType::SymbolCode );
    }
}

void Profiler::HandleSourceCodeQuery( char* data, char* image, uint32_t id )
{
    bool ok = false;
    struct stat st;
    if( stat( data, &st ) == 0 && (uint64_t)st.st_mtime < m_exectime )
    {
        if( st.st_size < ( TargetFrameSize - 16 ) )
        {
            FILE* f = fopen( data, "rb" );
            if( f )
            {
                auto ptr = (char*)tracy_malloc_fast( st.st_size );
                auto rd = fread( ptr, 1, st.st_size, f );
                fclose( f );
                if( rd == (size_t)st.st_size )
                {
                    TracyLfqPrepare( QueueType::SourceCodeMetadata );
                    MemWrite( &item->sourceCodeMetadata.ptr, (uint64_t)ptr );
                    MemWrite( &item->sourceCodeMetadata.size, (uint32_t)rd );
                    MemWrite( &item->sourceCodeMetadata.id, id );
                    TracyLfqCommit;
                    ok = true;
                }
            }
        }
    }

#ifdef TRACY_DEBUGINFOD
    else if( image && data[0] == '/' )
    {
        size_t size;
        auto buildid = GetBuildIdForImage( image, size );
        if( buildid )
        {
            auto d = debuginfod_find_source( GetDebuginfodClient(), buildid, size, data, nullptr );
            TracyDebug( "DebugInfo source query: %s, fn: %s, image: %s\n", d >= 0 ? " ok " : "fail", data, image );
            if( d >= 0 )
            {
                struct stat st;
                fstat( d, &st );
                if( st.st_size < ( TargetFrameSize - 16 ) )
                {
                    lseek( d, 0, SEEK_SET );
                    auto ptr = (char*)tracy_malloc_fast( st.st_size );
                    auto rd = read( d, ptr, st.st_size );
                    if( rd == (size_t)st.st_size )
                    {
                        TracyLfqPrepare( QueueType::SourceCodeMetadata );
                        MemWrite( &item->sourceCodeMetadata.ptr, (uint64_t)ptr );
                        MemWrite( &item->sourceCodeMetadata.size, (uint32_t)rd );
                        MemWrite( &item->sourceCodeMetadata.id, id );
                        TracyLfqCommit;
                        ok = true;
                    }
                }
                close( d );
            }
        }
    }
    else
    {
        TracyDebug( "DebugInfo invalid query fn: %s, image: %s\n", data, image );
    }
#endif

    if( !ok && m_sourceCallback )
    {
        size_t sz;
        char* ptr = m_sourceCallback( m_sourceCallbackData, data, sz );
        if( ptr )
        {
            if( sz < ( TargetFrameSize - 16 ) )
            {
                TracyLfqPrepare( QueueType::SourceCodeMetadata );
                MemWrite( &item->sourceCodeMetadata.ptr, (uint64_t)ptr );
                MemWrite( &item->sourceCodeMetadata.size, (uint32_t)sz );
                MemWrite( &item->sourceCodeMetadata.id, id );
                TracyLfqCommit;
                ok = true;
            }
        }
    }

    if( !ok )
    {
        TracyLfqPrepare( QueueType::AckSourceCodeNotAvailable );
        MemWrite( &item->sourceCodeNotAvailable, id );
        TracyLfqCommit;
    }

    tracy_free_fast( data );
    tracy_free_fast( image );
}

#if defined _WIN32 && defined TRACY_TIMER_QPC
int64_t Profiler::GetTimeQpc()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter( &t );
    return t.QuadPart;
}
#endif

}

#ifdef __cplusplus
extern "C" {
#endif

TRACY_API TracyCZoneCtx ___tracy_emit_zone_begin( const struct ___tracy_source_location_data* srcloc, int active )
{
    ___tracy_c_zone_context ctx;
#ifdef TRACY_ON_DEMAND
    ctx.active = active && tracy::GetProfiler().IsConnected();
#else
    ctx.active = active;
#endif
    if( !ctx.active ) return ctx;
    const auto id = tracy::GetProfiler().GetNextZoneId();
    ctx.id = id;

#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneBegin );
        tracy::MemWrite( &item->zoneBegin.time, tracy::Profiler::GetTime() );
        tracy::MemWrite( &item->zoneBegin.srcloc, (uint64_t)srcloc );
        TracyQueueCommitC( zoneBeginThread );
    }
    return ctx;
}

TRACY_API TracyCZoneCtx ___tracy_emit_zone_begin_callstack( const struct ___tracy_source_location_data* srcloc, int depth, int active )
{
    ___tracy_c_zone_context ctx;
#ifdef TRACY_ON_DEMAND
    ctx.active = active && tracy::GetProfiler().IsConnected();
#else
    ctx.active = active;
#endif
    if( !ctx.active ) return ctx;
    const auto id = tracy::GetProfiler().GetNextZoneId();
    ctx.id = id;

#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    tracy::GetProfiler().SendCallstack( depth );
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneBeginCallstack );
        tracy::MemWrite( &item->zoneBegin.time, tracy::Profiler::GetTime() );
        tracy::MemWrite( &item->zoneBegin.srcloc, (uint64_t)srcloc );
        TracyQueueCommitC( zoneBeginThread );
    }
    return ctx;
}

TRACY_API TracyCZoneCtx ___tracy_emit_zone_begin_alloc( uint64_t srcloc, int active )
{
    ___tracy_c_zone_context ctx;
#ifdef TRACY_ON_DEMAND
    ctx.active = active && tracy::GetProfiler().IsConnected();
#else
    ctx.active = active;
#endif
    if( !ctx.active )
    {
        tracy::tracy_free( (void*)srcloc );
        return ctx;
    }
    const auto id = tracy::GetProfiler().GetNextZoneId();
    ctx.id = id;

#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneBeginAllocSrcLoc );
        tracy::MemWrite( &item->zoneBegin.time, tracy::Profiler::GetTime() );
        tracy::MemWrite( &item->zoneBegin.srcloc, srcloc );
        TracyQueueCommitC( zoneBeginThread );
    }
    return ctx;
}

TRACY_API TracyCZoneCtx ___tracy_emit_zone_begin_alloc_callstack( uint64_t srcloc, int depth, int active )
{
    ___tracy_c_zone_context ctx;
#ifdef TRACY_ON_DEMAND
    ctx.active = active && tracy::GetProfiler().IsConnected();
#else
    ctx.active = active;
#endif
    if( !ctx.active )
    {
        tracy::tracy_free( (void*)srcloc );
        return ctx;
    }
    const auto id = tracy::GetProfiler().GetNextZoneId();
    ctx.id = id;

#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    tracy::GetProfiler().SendCallstack( depth );
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneBeginAllocSrcLocCallstack );
        tracy::MemWrite( &item->zoneBegin.time, tracy::Profiler::GetTime() );
        tracy::MemWrite( &item->zoneBegin.srcloc, srcloc );
        TracyQueueCommitC( zoneBeginThread );
    }
    return ctx;
}

TRACY_API void ___tracy_emit_zone_end( TracyCZoneCtx ctx )
{
    if( !ctx.active ) return;
#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, ctx.id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneEnd );
        tracy::MemWrite( &item->zoneEnd.time, tracy::Profiler::GetTime() );
        TracyQueueCommitC( zoneEndThread );
    }
}

TRACY_API void ___tracy_emit_zone_text( TracyCZoneCtx ctx, const char* txt, size_t size )
{
    assert( size < std::numeric_limits<uint16_t>::max() );
    if( !ctx.active ) return;
    auto ptr = (char*)tracy::tracy_malloc( size );
    memcpy( ptr, txt, size );
#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, ctx.id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneText );
        tracy::MemWrite( &item->zoneTextFat.text, (uint64_t)ptr );
        tracy::MemWrite( &item->zoneTextFat.size, (uint16_t)size );
        TracyQueueCommitC( zoneTextFatThread );
    }
}

TRACY_API void ___tracy_emit_zone_name( TracyCZoneCtx ctx, const char* txt, size_t size )
{
    assert( size < std::numeric_limits<uint16_t>::max() );
    if( !ctx.active ) return;
    auto ptr = (char*)tracy::tracy_malloc( size );
    memcpy( ptr, txt, size );
#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, ctx.id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneName );
        tracy::MemWrite( &item->zoneTextFat.text, (uint64_t)ptr );
        tracy::MemWrite( &item->zoneTextFat.size, (uint16_t)size );
        TracyQueueCommitC( zoneTextFatThread );
    }
}

TRACY_API void ___tracy_emit_zone_color( TracyCZoneCtx ctx, uint32_t color ) {
    if( !ctx.active ) return;
#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, ctx.id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneColor );
        tracy::MemWrite( &item->zoneColor.b, uint8_t( ( color       ) & 0xFF ) );
        tracy::MemWrite( &item->zoneColor.g, uint8_t( ( color >> 8  ) & 0xFF ) );
        tracy::MemWrite( &item->zoneColor.r, uint8_t( ( color >> 16 ) & 0xFF ) );
        TracyQueueCommitC( zoneColorThread );
    }
}

TRACY_API void ___tracy_emit_zone_value( TracyCZoneCtx ctx, uint64_t value )
{
    if( !ctx.active ) return;
#ifndef TRACY_NO_VERIFY
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValidation );
        tracy::MemWrite( &item->zoneValidation.id, ctx.id );
        TracyQueueCommitC( zoneValidationThread );
    }
#endif
    {
        TracyQueuePrepareC( tracy::QueueType::ZoneValue );
        tracy::MemWrite( &item->zoneValue.value, value );
        TracyQueueCommitC( zoneValueThread );
    }
}

TRACY_API void ___tracy_emit_memory_alloc( const void* ptr, size_t size, int secure ) { tracy::Profiler::MemAlloc( ptr, size, secure != 0 ); }
TRACY_API void ___tracy_emit_memory_alloc_callstack( const void* ptr, size_t size, int depth, int secure ) { tracy::Profiler::MemAllocCallstack( ptr, size, depth, secure != 0 ); }
TRACY_API void ___tracy_emit_memory_free( const void* ptr, int secure ) { tracy::Profiler::MemFree( ptr, secure != 0 ); }
TRACY_API void ___tracy_emit_memory_free_callstack( const void* ptr, int depth, int secure ) { tracy::Profiler::MemFreeCallstack( ptr, depth, secure != 0 ); }
TRACY_API void ___tracy_emit_memory_alloc_named( const void* ptr, size_t size, int secure, const char* name ) { tracy::Profiler::MemAllocNamed( ptr, size, secure != 0, name ); }
TRACY_API void ___tracy_emit_memory_alloc_callstack_named( const void* ptr, size_t size, int depth, int secure, const char* name ) { tracy::Profiler::MemAllocCallstackNamed( ptr, size, depth, secure != 0, name ); }
TRACY_API void ___tracy_emit_memory_free_named( const void* ptr, int secure, const char* name ) { tracy::Profiler::MemFreeNamed( ptr, secure != 0, name ); }
TRACY_API void ___tracy_emit_memory_free_callstack_named( const void* ptr, int depth, int secure, const char* name ) { tracy::Profiler::MemFreeCallstackNamed( ptr, depth, secure != 0, name ); }
TRACY_API void ___tracy_emit_frame_mark( const char* name ) { tracy::Profiler::SendFrameMark( name ); }
TRACY_API void ___tracy_emit_frame_mark_start( const char* name ) { tracy::Profiler::SendFrameMark( name, tracy::QueueType::FrameMarkMsgStart ); }
TRACY_API void ___tracy_emit_frame_mark_end( const char* name ) { tracy::Profiler::SendFrameMark( name, tracy::QueueType::FrameMarkMsgEnd ); }
TRACY_API void ___tracy_emit_frame_image( const void* image, uint16_t w, uint16_t h, uint8_t offset, int flip ) { tracy::Profiler::SendFrameImage( image, w, h, offset, flip ); }
TRACY_API void ___tracy_emit_plot( const char* name, double val ) { tracy::Profiler::PlotData( name, val ); }
TRACY_API void ___tracy_emit_plot_float( const char* name, float val ) { tracy::Profiler::PlotData( name, val ); }
TRACY_API void ___tracy_emit_plot_int( const char* name, int64_t val ) { tracy::Profiler::PlotData( name, val ); }
TRACY_API void ___tracy_emit_message( const char* txt, size_t size, int callstack ) { tracy::Profiler::Message( txt, size, callstack ); }
TRACY_API void ___tracy_emit_messageL( const char* txt, int callstack ) { tracy::Profiler::Message( txt, callstack ); }
TRACY_API void ___tracy_emit_messageC( const char* txt, size_t size, uint32_t color, int callstack ) { tracy::Profiler::MessageColor( txt, size, color, callstack ); }
TRACY_API void ___tracy_emit_messageLC( const char* txt, uint32_t color, int callstack ) { tracy::Profiler::MessageColor( txt, color, callstack ); }
TRACY_API void ___tracy_emit_message_appinfo( const char* txt, size_t size ) { tracy::Profiler::MessageAppInfo( txt, size ); }

TRACY_API uint64_t ___tracy_alloc_srcloc( uint32_t line, const char* source, size_t sourceSz, const char* function, size_t functionSz ) {
    return tracy::Profiler::AllocSourceLocation( line, source, sourceSz, function, functionSz );
}

TRACY_API uint64_t ___tracy_alloc_srcloc_name( uint32_t line, const char* source, size_t sourceSz, const char* function, size_t functionSz, const char* name, size_t nameSz ) {
    return tracy::Profiler::AllocSourceLocation( line, source, sourceSz, function, functionSz, name, nameSz );
}

TRACY_API void ___tracy_emit_gpu_zone_begin( const struct ___tracy_gpu_zone_begin_data data )
{
    TracyLfqPrepareC( tracy::QueueType::GpuZoneBegin );
    tracy::MemWrite( &item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuNewContext.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuZoneBegin.srcloc, data.srcloc );
    tracy::MemWrite( &item->gpuZoneBegin.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneBegin.context, data.context );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_zone_begin_callstack( const struct ___tracy_gpu_zone_begin_callstack_data data )
{
    tracy::GetProfiler().SendCallstack( data.depth );
    TracyLfqPrepareC( tracy::QueueType::GpuZoneBeginCallstack );
    tracy::MemWrite( &item->gpuZoneBegin.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuZoneBegin.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneBegin.context, data.context );
    tracy::MemWrite( &item->gpuZoneBegin.srcloc, data.srcloc );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_zone_begin_alloc( const struct ___tracy_gpu_zone_begin_data data )
{
    TracyLfqPrepareC( tracy::QueueType::GpuZoneBeginAllocSrcLoc  );
    tracy::MemWrite( &item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuNewContext.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuZoneBegin.srcloc, data.srcloc );
    tracy::MemWrite( &item->gpuZoneBegin.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneBegin.context, data.context );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_zone_begin_alloc_callstack( const struct ___tracy_gpu_zone_begin_callstack_data data )
{
    tracy::GetProfiler().SendCallstack( data.depth );
    TracyLfqPrepareC( tracy::QueueType::GpuZoneBeginAllocSrcLocCallstack  );
    tracy::MemWrite( &item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuNewContext.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuZoneBegin.srcloc, data.srcloc );
    tracy::MemWrite( &item->gpuZoneBegin.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneBegin.context, data.context );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_time( const struct ___tracy_gpu_time_data data )
{
    TracyLfqPrepareC( tracy::QueueType::GpuTime );
    tracy::MemWrite( &item->gpuTime.gpuTime, data.gpuTime );
    tracy::MemWrite( &item->gpuTime.queryId, data.queryId );
    tracy::MemWrite( &item->gpuTime.context, data.context );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_zone_end( const struct ___tracy_gpu_zone_end_data data )
{
    TracyLfqPrepareC( tracy::QueueType::GpuZoneEnd );
    tracy::MemWrite( &item->gpuZoneEnd.cpuTime, tracy::Profiler::GetTime() );
    memset( &item->gpuZoneEnd.thread, 0, sizeof( item->gpuZoneEnd.thread ) );
    tracy::MemWrite( &item->gpuZoneEnd.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneEnd.context, data.context );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_new_context( ___tracy_gpu_new_context_data data )
{
    TracyLfqPrepareC( tracy::QueueType::GpuNewContext );
    tracy::MemWrite( &item->gpuNewContext.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuNewContext.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuNewContext.gpuTime, data.gpuTime );
    tracy::MemWrite( &item->gpuNewContext.period, data.period );
    tracy::MemWrite( &item->gpuNewContext.context, data.context );
    tracy::MemWrite( &item->gpuNewContext.flags, data.flags );
    tracy::MemWrite( &item->gpuNewContext.type, data.type );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_context_name( const struct ___tracy_gpu_context_name_data data )
{
    auto ptr = (char*)tracy::tracy_malloc( data.len );
    memcpy( ptr, data.name, data.len );

    TracyLfqPrepareC( tracy::QueueType::GpuContextName );
    tracy::MemWrite( &item->gpuContextNameFat.context, data.context );
    tracy::MemWrite( &item->gpuContextNameFat.ptr, (uint64_t)ptr );
    tracy::MemWrite( &item->gpuContextNameFat.size, data.len );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_calibration( const struct ___tracy_gpu_calibration_data data )
{
    TracyLfqPrepareC( tracy::QueueType::GpuCalibration );
    tracy::MemWrite( &item->gpuCalibration.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuCalibration.gpuTime, data.gpuTime );
    tracy::MemWrite( &item->gpuCalibration.cpuDelta, data.cpuDelta );
    tracy::MemWrite( &item->gpuCalibration.context, data.context );
    TracyLfqCommitC;
}

TRACY_API void ___tracy_emit_gpu_zone_begin_serial( const struct ___tracy_gpu_zone_begin_data data )
{
    auto item = tracy::Profiler::QueueSerial();
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuZoneBeginSerial );
    tracy::MemWrite( &item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuZoneBegin.srcloc, data.srcloc );
    tracy::MemWrite( &item->gpuZoneBegin.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuZoneBegin.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneBegin.context, data.context );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API void ___tracy_emit_gpu_zone_begin_callstack_serial( const struct ___tracy_gpu_zone_begin_callstack_data data )
{
    auto item = tracy::Profiler::QueueSerialCallstack( tracy::Callstack( data.depth ) );
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuZoneBeginCallstackSerial );
    tracy::MemWrite( &item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuZoneBegin.srcloc, data.srcloc );
    tracy::MemWrite( &item->gpuZoneBegin.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuZoneBegin.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneBegin.context, data.context );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API void ___tracy_emit_gpu_zone_begin_alloc_serial( const struct ___tracy_gpu_zone_begin_data data )
{
    auto item = tracy::Profiler::QueueSerial();
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuZoneBeginAllocSrcLocSerial );
    tracy::MemWrite( &item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuNewContext.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuZoneBegin.srcloc, data.srcloc );
    tracy::MemWrite( &item->gpuZoneBegin.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneBegin.context, data.context );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API void ___tracy_emit_gpu_zone_begin_alloc_callstack_serial( const struct ___tracy_gpu_zone_begin_callstack_data data )
{
    auto item = tracy::Profiler::QueueSerialCallstack( tracy::Callstack( data.depth ) );
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuZoneBeginAllocSrcLocCallstackSerial );
    tracy::MemWrite( &item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuNewContext.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuZoneBegin.srcloc, data.srcloc );
    tracy::MemWrite( &item->gpuZoneBegin.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneBegin.context, data.context );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API void ___tracy_emit_gpu_time_serial( const struct ___tracy_gpu_time_data data )
{
    auto item = tracy::Profiler::QueueSerial();
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuTime );
    tracy::MemWrite( &item->gpuTime.gpuTime, data.gpuTime );
    tracy::MemWrite( &item->gpuTime.queryId, data.queryId );
    tracy::MemWrite( &item->gpuTime.context, data.context );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API void ___tracy_emit_gpu_zone_end_serial( const struct ___tracy_gpu_zone_end_data data )
{
    auto item = tracy::Profiler::QueueSerial();
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuZoneEndSerial );
    tracy::MemWrite( &item->gpuZoneEnd.cpuTime, tracy::Profiler::GetTime() );
    memset( &item->gpuZoneEnd.thread, 0, sizeof( item->gpuZoneEnd.thread ) );
    tracy::MemWrite( &item->gpuZoneEnd.queryId, data.queryId );
    tracy::MemWrite( &item->gpuZoneEnd.context, data.context );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API void ___tracy_emit_gpu_new_context_serial( ___tracy_gpu_new_context_data data )
{
    auto item = tracy::Profiler::QueueSerial();
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuNewContext );
    tracy::MemWrite( &item->gpuNewContext.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuNewContext.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item->gpuNewContext.gpuTime, data.gpuTime );
    tracy::MemWrite( &item->gpuNewContext.period, data.period );
    tracy::MemWrite( &item->gpuNewContext.context, data.context );
    tracy::MemWrite( &item->gpuNewContext.flags, data.flags );
    tracy::MemWrite( &item->gpuNewContext.type, data.type );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API void ___tracy_emit_gpu_context_name_serial( const struct ___tracy_gpu_context_name_data data )
{
    auto ptr = (char*)tracy::tracy_malloc( data.len );
    memcpy( ptr, data.name, data.len );

    auto item = tracy::Profiler::QueueSerial();
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuContextName );
    tracy::MemWrite( &item->gpuContextNameFat.context, data.context );
    tracy::MemWrite( &item->gpuContextNameFat.ptr, (uint64_t)ptr );
    tracy::MemWrite( &item->gpuContextNameFat.size, data.len );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API void ___tracy_emit_gpu_calibration_serial( const struct ___tracy_gpu_calibration_data data )
{
    auto item = tracy::Profiler::QueueSerial();
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuCalibration );
    tracy::MemWrite( &item->gpuCalibration.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item->gpuCalibration.gpuTime, data.gpuTime );
    tracy::MemWrite( &item->gpuCalibration.cpuDelta, data.cpuDelta );
    tracy::MemWrite( &item->gpuCalibration.context, data.context );
    tracy::Profiler::QueueSerialFinish();
}

TRACY_API int ___tracy_connected( void )
{
    return tracy::GetProfiler().IsConnected();
}

#ifdef TRACY_FIBERS
TRACY_API void ___tracy_fiber_enter( const char* fiber ){ tracy::Profiler::EnterFiber( fiber ); }
TRACY_API void ___tracy_fiber_leave( void ){ tracy::Profiler::LeaveFiber(); }
#endif

#  ifdef TRACY_MANUAL_LIFETIME
TRACY_API void ___tracy_startup_profiler( void )
{
    tracy::StartupProfiler();
}

TRACY_API void ___tracy_shutdown_profiler( void )
{
    tracy::ShutdownProfiler();
}
#  endif

#ifdef __cplusplus
}
#endif

#endif
