#include "TracyDebug.hpp"
#include "TracyStringHelpers.hpp"
#include "TracySysTrace.hpp"
#include "../common/TracySystem.hpp"

#ifdef TRACY_HAS_SYSTEM_TRACING

#ifndef TRACY_SAMPLING_HZ
#  if defined _WIN32
#    define TRACY_SAMPLING_HZ 8000
#  elif defined __linux__
#    define TRACY_SAMPLING_HZ 10000
#  endif
#endif

namespace tracy
{

static constexpr int GetSamplingFrequency()
{
#if defined _WIN32
    return TRACY_SAMPLING_HZ > 8000 ? 8000 : ( TRACY_SAMPLING_HZ < 1 ? 1 : TRACY_SAMPLING_HZ );
#else
    return TRACY_SAMPLING_HZ > 1000000 ? 1000000 : ( TRACY_SAMPLING_HZ < 1 ? 1 : TRACY_SAMPLING_HZ );
#endif
}

static constexpr int GetSamplingPeriod()
{
    return 1000000000 / GetSamplingFrequency();
}

}

#  if defined _WIN32

#    ifndef NOMINMAX
#      define NOMINMAX
#    endif

#    define INITGUID
#    include <assert.h>
#    include <string.h>
#    include <windows.h>
#    include <dbghelp.h>
#    include <evntrace.h>
#    include <evntcons.h>
#    include <psapi.h>
#    include <winternl.h>

#    include "../common/TracyAlloc.hpp"
#    include "../common/TracySystem.hpp"
#    include "TracyProfiler.hpp"
#    include "TracyThread.hpp"

namespace tracy
{

static const GUID PerfInfoGuid = { 0xce1dbfb4, 0x137e, 0x4da6, { 0x87, 0xb0, 0x3f, 0x59, 0xaa, 0x10, 0x2c, 0xbc } };
static const GUID DxgKrnlGuid  = { 0x802ec45a, 0x1e99, 0x4b83, { 0x99, 0x20, 0x87, 0xc9, 0x82, 0x77, 0xba, 0x9d } };
static const GUID ThreadV2Guid = { 0x3d6fa8d1, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };


static TRACEHANDLE s_traceHandle;
static TRACEHANDLE s_traceHandle2;
static EVENT_TRACE_PROPERTIES* s_prop;
static DWORD s_pid;

static EVENT_TRACE_PROPERTIES* s_propVsync;
static TRACEHANDLE s_traceHandleVsync;
static TRACEHANDLE s_traceHandleVsync2;
Thread* s_threadVsync = nullptr;

struct CSwitch
{
    uint32_t    newThreadId;
    uint32_t    oldThreadId;
    int8_t      newThreadPriority;
    int8_t      oldThreadPriority;
    uint8_t     previousCState;
    int8_t      spareByte;
    int8_t      oldThreadWaitReason;
    int8_t      oldThreadWaitMode;
    int8_t      oldThreadState;
    int8_t      oldThreadWaitIdealProcessor;
    uint32_t    newThreadWaitTime;
    uint32_t    reserved;
};

struct ReadyThread
{
    uint32_t    threadId;
    int8_t      adjustReason;
    int8_t      adjustIncrement;
    int8_t      flag;
    int8_t      reserverd;
};

struct ThreadTrace
{
    uint32_t processId;
    uint32_t threadId;
    uint32_t stackBase;
    uint32_t stackLimit;
    uint32_t userStackBase;
    uint32_t userStackLimit;
    uint32_t startAddr;
    uint32_t win32StartAddr;
    uint32_t tebBase;
    uint32_t subProcessTag;
};

struct StackWalkEvent
{
    uint64_t eventTimeStamp;
    uint32_t stackProcess;
    uint32_t stackThread;
    uint64_t stack[192];
};

struct VSyncInfo
{
    void*       dxgAdapter;
    uint32_t    vidPnTargetId;
    uint64_t    scannedPhysicalAddress;
    uint32_t    vidPnSourceId;
    uint32_t    frameNumber;
    int64_t     frameQpcTime;
    void*       hFlipDevice;
    uint32_t    flipType;
    uint64_t    flipFenceId;
};

extern "C" typedef NTSTATUS (WINAPI *t_NtQueryInformationThread)( HANDLE, THREADINFOCLASS, PVOID, ULONG, PULONG );
extern "C" typedef BOOL (WINAPI *t_EnumProcessModules)( HANDLE, HMODULE*, DWORD, LPDWORD );
extern "C" typedef BOOL (WINAPI *t_GetModuleInformation)( HANDLE, HMODULE, LPMODULEINFO, DWORD );
extern "C" typedef DWORD (WINAPI *t_GetModuleBaseNameA)( HANDLE, HMODULE, LPSTR, DWORD );
extern "C" typedef HRESULT (WINAPI *t_GetThreadDescription)( HANDLE, PWSTR* );

t_NtQueryInformationThread NtQueryInformationThread = (t_NtQueryInformationThread)GetProcAddress( GetModuleHandleA( "ntdll.dll" ), "NtQueryInformationThread" );
t_EnumProcessModules _EnumProcessModules = (t_EnumProcessModules)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "K32EnumProcessModules" );
t_GetModuleInformation _GetModuleInformation = (t_GetModuleInformation)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "K32GetModuleInformation" );
t_GetModuleBaseNameA _GetModuleBaseNameA = (t_GetModuleBaseNameA)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "K32GetModuleBaseNameA" );

static t_GetThreadDescription _GetThreadDescription = 0;


void WINAPI EventRecordCallback( PEVENT_RECORD record )
{
#ifdef TRACY_ON_DEMAND
    if( !GetProfiler().IsConnected() ) return;
#endif

    const auto& hdr = record->EventHeader;
    switch( hdr.ProviderId.Data1 )
    {
    case 0x3d6fa8d1:    // Thread Guid
        if( hdr.EventDescriptor.Opcode == 36 )
        {
            const auto cswitch = (const CSwitch*)record->UserData;

            TracyLfqPrepare( QueueType::ContextSwitch );
            MemWrite( &item->contextSwitch.time, hdr.TimeStamp.QuadPart );
            MemWrite( &item->contextSwitch.oldThread, cswitch->oldThreadId );
            MemWrite( &item->contextSwitch.newThread, cswitch->newThreadId );
            MemWrite( &item->contextSwitch.cpu, record->BufferContext.ProcessorNumber );
            MemWrite( &item->contextSwitch.reason, cswitch->oldThreadWaitReason );
            MemWrite( &item->contextSwitch.state, cswitch->oldThreadState );
            TracyLfqCommit;
        }
        else if( hdr.EventDescriptor.Opcode == 50 )
        {
            const auto rt = (const ReadyThread*)record->UserData;

            TracyLfqPrepare( QueueType::ThreadWakeup );
            MemWrite( &item->threadWakeup.time, hdr.TimeStamp.QuadPart );
            MemWrite( &item->threadWakeup.thread, rt->threadId );
            TracyLfqCommit;
        }
        else if( hdr.EventDescriptor.Opcode == 1 || hdr.EventDescriptor.Opcode == 3 )
        {
            const auto tt = (const ThreadTrace*)record->UserData;

            uint64_t tid = tt->threadId;
            if( tid == 0 ) return;
            uint64_t pid = tt->processId;
            TracyLfqPrepare( QueueType::TidToPid );
            MemWrite( &item->tidToPid.tid, tid );
            MemWrite( &item->tidToPid.pid, pid );
            TracyLfqCommit;
        }
        break;
    case 0xdef2fe46:    // StackWalk Guid
        if( hdr.EventDescriptor.Opcode == 32 )
        {
            const auto sw = (const StackWalkEvent*)record->UserData;
            if( sw->stackProcess == s_pid )
            {
                const uint64_t sz = ( record->UserDataLength - 16 ) / 8;
                if( sz > 0 )
                {
                    auto trace = (uint64_t*)tracy_malloc( ( 1 + sz ) * sizeof( uint64_t ) );
                    memcpy( trace, &sz, sizeof( uint64_t ) );
                    memcpy( trace+1, sw->stack, sizeof( uint64_t ) * sz );
                    TracyLfqPrepare( QueueType::CallstackSample );
                    MemWrite( &item->callstackSampleFat.time, sw->eventTimeStamp );
                    MemWrite( &item->callstackSampleFat.thread, sw->stackThread );
                    MemWrite( &item->callstackSampleFat.ptr, (uint64_t)trace );
                    TracyLfqCommit;
                }
            }
        }
        break;
    default:
        break;
    }
}

void WINAPI EventRecordCallbackVsync( PEVENT_RECORD record )
{
#ifdef TRACY_ON_DEMAND
    if( !GetProfiler().IsConnected() ) return;
#endif

    const auto& hdr = record->EventHeader;
    assert( hdr.ProviderId.Data1 == 0x802EC45A );
    assert( hdr.EventDescriptor.Id == 0x0011 );

    const auto vs = (const VSyncInfo*)record->UserData;

    TracyLfqPrepare( QueueType::FrameVsync );
    MemWrite( &item->frameVsync.time, hdr.TimeStamp.QuadPart );
    MemWrite( &item->frameVsync.id, vs->vidPnTargetId );
    TracyLfqCommit;
}

static void SetupVsync()
{
#if _WIN32_WINNT >= _WIN32_WINNT_WINBLUE && !defined(__MINGW32__)
    const auto psz = sizeof( EVENT_TRACE_PROPERTIES ) + MAX_PATH;
    s_propVsync = (EVENT_TRACE_PROPERTIES*)tracy_malloc( psz );
    memset( s_propVsync, 0, sizeof( EVENT_TRACE_PROPERTIES ) );
    s_propVsync->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    s_propVsync->Wnode.BufferSize = psz;
#ifdef TRACY_TIMER_QPC
    s_propVsync->Wnode.ClientContext = 1;
#else
    s_propVsync->Wnode.ClientContext = 3;
#endif
    s_propVsync->LoggerNameOffset = sizeof( EVENT_TRACE_PROPERTIES );
    strcpy( ((char*)s_propVsync) + sizeof( EVENT_TRACE_PROPERTIES ), "TracyVsync" );

    auto backup = tracy_malloc( psz );
    memcpy( backup, s_propVsync, psz );

    const auto controlStatus = ControlTraceA( 0, "TracyVsync", s_propVsync, EVENT_TRACE_CONTROL_STOP );
    if( controlStatus != ERROR_SUCCESS && controlStatus != ERROR_WMI_INSTANCE_NOT_FOUND )
    {
        tracy_free( backup );
        tracy_free( s_propVsync );
        return;
    }

    memcpy( s_propVsync, backup, psz );
    tracy_free( backup );

    const auto startStatus = StartTraceA( &s_traceHandleVsync, "TracyVsync", s_propVsync );
    if( startStatus != ERROR_SUCCESS )
    {
        tracy_free( s_propVsync );
        return;
    }

    EVENT_FILTER_EVENT_ID fe = {};
    fe.FilterIn = TRUE;
    fe.Count = 1;
    fe.Events[0] = 0x0011;  // VSyncDPC_Info

    EVENT_FILTER_DESCRIPTOR desc = {};
    desc.Ptr = (ULONGLONG)&fe;
    desc.Size = sizeof( fe );
    desc.Type = EVENT_FILTER_TYPE_EVENT_ID;

    ENABLE_TRACE_PARAMETERS params = {};
    params.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    params.EnableProperty = EVENT_ENABLE_PROPERTY_IGNORE_KEYWORD_0;
    params.SourceId = s_propVsync->Wnode.Guid;
    params.EnableFilterDesc = &desc;
    params.FilterDescCount = 1;

    uint64_t mask = 0x4000000000000001;   // Microsoft_Windows_DxgKrnl_Performance | Base
    if( EnableTraceEx2( s_traceHandleVsync, &DxgKrnlGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, mask, mask, 0, &params ) != ERROR_SUCCESS )
    {
        tracy_free( s_propVsync );
        return;
    }

    char loggerName[MAX_PATH];
    strcpy( loggerName, "TracyVsync" );

    EVENT_TRACE_LOGFILEA log = {};
    log.LoggerName = loggerName;
    log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    log.EventRecordCallback = EventRecordCallbackVsync;

    s_traceHandleVsync2 = OpenTraceA( &log );
    if( s_traceHandleVsync2 == (TRACEHANDLE)INVALID_HANDLE_VALUE )
    {
        CloseTrace( s_traceHandleVsync );
        tracy_free( s_propVsync );
        return;
    }

    s_threadVsync = (Thread*)tracy_malloc( sizeof( Thread ) );
    new(s_threadVsync) Thread( [] (void*) {
        ThreadExitHandler threadExitHandler;
        SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
        SetThreadName( "Tracy Vsync" );
        ProcessTrace( &s_traceHandleVsync2, 1, nullptr, nullptr );
    }, nullptr );
#endif
}

static constexpr int GetSamplingInterval()
{
    return GetSamplingPeriod() / 100;
}

bool SysTraceStart( int64_t& samplingPeriod )
{
    if( !_GetThreadDescription ) _GetThreadDescription = (t_GetThreadDescription)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "GetThreadDescription" );

    s_pid = GetCurrentProcessId();

#if defined _WIN64
    constexpr bool isOs64Bit = true;
#else
    BOOL _iswow64;
    IsWow64Process( GetCurrentProcess(), &_iswow64 );
    const bool isOs64Bit = _iswow64;
#endif

    TOKEN_PRIVILEGES priv = {};
    priv.PrivilegeCount = 1;
    priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if( LookupPrivilegeValue( nullptr, SE_SYSTEM_PROFILE_NAME, &priv.Privileges[0].Luid ) == 0 ) return false;

    HANDLE pt;
    if( OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &pt ) == 0 ) return false;
    const auto adjust = AdjustTokenPrivileges( pt, FALSE, &priv, 0, nullptr, nullptr );
    CloseHandle( pt );
    if( adjust == 0 ) return false;
    const auto status = GetLastError();
    if( status != ERROR_SUCCESS ) return false;

    if( isOs64Bit )
    {
        TRACE_PROFILE_INTERVAL interval = {};
        interval.Interval = GetSamplingInterval();
        const auto intervalStatus = TraceSetInformation( 0, TraceSampledProfileIntervalInfo, &interval, sizeof( interval ) );
        if( intervalStatus != ERROR_SUCCESS ) return false;
        samplingPeriod = GetSamplingPeriod();
    }

    const auto psz = sizeof( EVENT_TRACE_PROPERTIES ) + sizeof( KERNEL_LOGGER_NAME );
    s_prop = (EVENT_TRACE_PROPERTIES*)tracy_malloc( psz );
    memset( s_prop, 0, sizeof( EVENT_TRACE_PROPERTIES ) );
    ULONG flags = 0;
#ifndef TRACY_NO_CONTEXT_SWITCH
    flags = EVENT_TRACE_FLAG_CSWITCH | EVENT_TRACE_FLAG_DISPATCHER | EVENT_TRACE_FLAG_THREAD;
#endif
#ifndef TRACY_NO_SAMPLING
    if( isOs64Bit ) flags |= EVENT_TRACE_FLAG_PROFILE;
#endif
    s_prop->EnableFlags = flags;
    s_prop->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    s_prop->Wnode.BufferSize = psz;
    s_prop->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
#ifdef TRACY_TIMER_QPC
    s_prop->Wnode.ClientContext = 1;
#else
    s_prop->Wnode.ClientContext = 3;
#endif
    s_prop->Wnode.Guid = SystemTraceControlGuid;
    s_prop->BufferSize = 1024;
    s_prop->MinimumBuffers = std::thread::hardware_concurrency() * 4;
    s_prop->MaximumBuffers = std::thread::hardware_concurrency() * 6;
    s_prop->LoggerNameOffset = sizeof( EVENT_TRACE_PROPERTIES );
    memcpy( ((char*)s_prop) + sizeof( EVENT_TRACE_PROPERTIES ), KERNEL_LOGGER_NAME, sizeof( KERNEL_LOGGER_NAME ) );

    auto backup = tracy_malloc( psz );
    memcpy( backup, s_prop, psz );

    const auto controlStatus = ControlTrace( 0, KERNEL_LOGGER_NAME, s_prop, EVENT_TRACE_CONTROL_STOP );
    if( controlStatus != ERROR_SUCCESS && controlStatus != ERROR_WMI_INSTANCE_NOT_FOUND )
    {
        tracy_free( backup );
        tracy_free( s_prop );
        return false;
    }

    memcpy( s_prop, backup, psz );
    tracy_free( backup );

    const auto startStatus = StartTrace( &s_traceHandle, KERNEL_LOGGER_NAME, s_prop );
    if( startStatus != ERROR_SUCCESS )
    {
        tracy_free( s_prop );
        return false;
    }

#ifndef TRACY_NO_SAMPLING
    if( isOs64Bit )
    {
        CLASSIC_EVENT_ID stackId[2] = {};
        stackId[0].EventGuid = PerfInfoGuid;
        stackId[0].Type = 46;
        stackId[1].EventGuid = ThreadV2Guid;
        stackId[1].Type = 36;
        const auto stackStatus = TraceSetInformation( s_traceHandle, TraceStackTracingInfo, &stackId, sizeof( stackId ) );
        if( stackStatus != ERROR_SUCCESS )
        {
            tracy_free( s_prop );
            return false;
        }
    }
#endif

#ifdef UNICODE
    WCHAR KernelLoggerName[sizeof( KERNEL_LOGGER_NAME )];
#else
    char KernelLoggerName[sizeof( KERNEL_LOGGER_NAME )];
#endif
    memcpy( KernelLoggerName, KERNEL_LOGGER_NAME, sizeof( KERNEL_LOGGER_NAME ) );
    EVENT_TRACE_LOGFILE log = {};
    log.LoggerName = KernelLoggerName;
    log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    log.EventRecordCallback = EventRecordCallback;

    s_traceHandle2 = OpenTrace( &log );
    if( s_traceHandle2 == (TRACEHANDLE)INVALID_HANDLE_VALUE )
    {
        CloseTrace( s_traceHandle );
        tracy_free( s_prop );
        return false;
    }

#ifndef TRACY_NO_VSYNC_CAPTURE
    SetupVsync();
#endif

    return true;
}

void SysTraceStop()
{
    if( s_threadVsync )
    {
        CloseTrace( s_traceHandleVsync2 );
        CloseTrace( s_traceHandleVsync );
        s_threadVsync->~Thread();
        tracy_free( s_threadVsync );
    }

    CloseTrace( s_traceHandle2 );
    CloseTrace( s_traceHandle );
}

void SysTraceWorker( void* ptr )
{
    ThreadExitHandler threadExitHandler;
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
    SetThreadName( "Tracy SysTrace" );
    ProcessTrace( &s_traceHandle2, 1, 0, 0 );
    ControlTrace( 0, KERNEL_LOGGER_NAME, s_prop, EVENT_TRACE_CONTROL_STOP );
    tracy_free( s_prop );
}

void SysTraceGetExternalName( uint64_t thread, const char*& threadName, const char*& name )
{
    bool threadSent = false;
    auto hnd = OpenThread( THREAD_QUERY_INFORMATION, FALSE, DWORD( thread ) );
    if( hnd == 0 )
    {
        hnd = OpenThread( THREAD_QUERY_LIMITED_INFORMATION, FALSE, DWORD( thread ) );
    }
    if( hnd != 0 )
    {
        if( _GetThreadDescription )
        {
            PWSTR tmp;
            _GetThreadDescription( hnd, &tmp );
            char buf[256];
            if( tmp )
            {
                auto ret = wcstombs( buf, tmp, 256 );
                if( ret != 0 )
                {
                    threadName = CopyString( buf, ret );
                    threadSent = true;
                }
            }
        }
        const auto pid = GetProcessIdOfThread( hnd );
        if( !threadSent && NtQueryInformationThread && _EnumProcessModules && _GetModuleInformation && _GetModuleBaseNameA )
        {
            void* ptr;
            ULONG retlen;
            auto status = NtQueryInformationThread( hnd, (THREADINFOCLASS)9 /*ThreadQuerySetWin32StartAddress*/, &ptr, sizeof( &ptr ), &retlen );
            if( status == 0 )
            {
                const auto phnd = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid );
                if( phnd != INVALID_HANDLE_VALUE )
                {
                    HMODULE modules[1024];
                    DWORD needed;
                    if( _EnumProcessModules( phnd, modules, 1024 * sizeof( HMODULE ), &needed ) != 0 )
                    {
                        const auto sz = std::min( DWORD( needed / sizeof( HMODULE ) ), DWORD( 1024 ) );
                        for( DWORD i=0; i<sz; i++ )
                        {
                            MODULEINFO info;
                            if( _GetModuleInformation( phnd, modules[i], &info, sizeof( info ) ) != 0 )
                            {
                                if( (uint64_t)ptr >= (uint64_t)info.lpBaseOfDll && (uint64_t)ptr <= (uint64_t)info.lpBaseOfDll + (uint64_t)info.SizeOfImage )
                                {
                                    char buf2[1024];
                                    const auto modlen = _GetModuleBaseNameA( phnd, modules[i], buf2, 1024 );
                                    if( modlen != 0 )
                                    {
                                        threadName = CopyString( buf2, modlen );
                                        threadSent = true;
                                    }
                                }
                            }
                        }
                    }
                    CloseHandle( phnd );
                }
            }
        }
        CloseHandle( hnd );
        if( !threadSent )
        {
            threadName = CopyString( "???", 3 );
            threadSent = true;
        }
        if( pid != 0 )
        {
            {
                uint64_t _pid = pid;
                TracyLfqPrepare( QueueType::TidToPid );
                MemWrite( &item->tidToPid.tid, thread );
                MemWrite( &item->tidToPid.pid, _pid );
                TracyLfqCommit;
            }
            if( pid == 4 )
            {
                name = CopyStringFast( "System", 6 );
                return;
            }
            else
            {
                const auto phnd = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );
                if( phnd != INVALID_HANDLE_VALUE )
                {
                    char buf2[1024];
                    const auto sz = GetProcessImageFileNameA( phnd, buf2, 1024 );
                    CloseHandle( phnd );
                    if( sz != 0 )
                    {
                        auto ptr = buf2 + sz - 1;
                        while( ptr > buf2 && *ptr != '\\' ) ptr--;
                        if( *ptr == '\\' ) ptr++;
                        name = CopyStringFast( ptr );
                        return;
                    }
                }
            }
        }
    }

    if( !threadSent )
    {
        threadName = CopyString( "???", 3 );
    }
    name = CopyStringFast( "???", 3 );
}

}

#  elif defined __linux__

#    include <sys/types.h>
#    include <sys/stat.h>
#    include <sys/wait.h>
#    include <fcntl.h>
#    include <inttypes.h>
#    include <limits>
#    include <poll.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <unistd.h>
#    include <atomic>
#    include <thread>
#    include <linux/perf_event.h>
#    include <linux/version.h>
#    include <sys/mman.h>
#    include <sys/ioctl.h>
#    include <sys/syscall.h>

#    if defined __i386 || defined __x86_64__
#      include "TracyCpuid.hpp"
#    endif

#    include "TracyProfiler.hpp"
#    include "TracyRingBuffer.hpp"
#    include "TracyThread.hpp"

namespace tracy
{

static std::atomic<bool> traceActive { false };
static int s_numCpus = 0;
static int s_numBuffers = 0;
static int s_ctxBufferIdx = 0;

static RingBuffer* s_ring = nullptr;

static const int ThreadHashSize = 4 * 1024;
static uint32_t s_threadHash[ThreadHashSize] = {};

static bool CurrentProcOwnsThread( uint32_t tid )
{
    const auto hash = tid & ( ThreadHashSize-1 );
    const auto hv = s_threadHash[hash];
    if( hv == tid ) return true;
    if( hv == -tid ) return false;

    char path[256];
    sprintf( path, "/proc/self/task/%d", tid );
    struct stat st;
    if( stat( path, &st ) == 0 )
    {
        s_threadHash[hash] = tid;
        return true;
    }
    else
    {
        s_threadHash[hash] = -tid;
        return false;
    }
}

static int perf_event_open( struct perf_event_attr* hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags )
{
    return syscall( __NR_perf_event_open, hw_event, pid, cpu, group_fd, flags );
}

enum TraceEventId
{
    EventCallstack,
    EventCpuCycles,
    EventInstructionsRetired,
    EventCacheReference,
    EventCacheMiss,
    EventBranchRetired,
    EventBranchMiss,
    EventVsync,
    EventContextSwitch,
    EventWakeup,
};

static void ProbePreciseIp( perf_event_attr& pe, unsigned long long config0, unsigned long long config1, pid_t pid )
{
    pe.config = config1;
    pe.precise_ip = 3;
    while( pe.precise_ip != 0 )
    {
        const int fd = perf_event_open( &pe, pid, 0, -1, PERF_FLAG_FD_CLOEXEC );
        if( fd != -1 )
        {
            close( fd );
            break;
        }
        pe.precise_ip--;
    }
    pe.config = config0;
    while( pe.precise_ip != 0 )
    {
        const int fd = perf_event_open( &pe, pid, 0, -1, PERF_FLAG_FD_CLOEXEC );
        if( fd != -1 )
        {
            close( fd );
            break;
        }
        pe.precise_ip--;
    }
    TracyDebug( "  Probed precise_ip: %i\n", pe.precise_ip );
}

static void ProbePreciseIp( perf_event_attr& pe, pid_t pid )
{
    pe.precise_ip = 3;
    while( pe.precise_ip != 0 )
    {
        const int fd = perf_event_open( &pe, pid, 0, -1, PERF_FLAG_FD_CLOEXEC );
        if( fd != -1 )
        {
            close( fd );
            break;
        }
        pe.precise_ip--;
    }
    TracyDebug( "  Probed precise_ip: %i\n", pe.precise_ip );
}

static bool IsGenuineIntel()
{
#if defined __i386 || defined __x86_64__
    uint32_t regs[4] = {};
    __get_cpuid( 0, regs, regs+1, regs+2, regs+3 );
    char manufacturer[12];
    memcpy( manufacturer, regs+1, 4 );
    memcpy( manufacturer+4, regs+3, 4 );
    memcpy( manufacturer+8, regs+2, 4 );
    return memcmp( manufacturer, "GenuineIntel", 12 ) == 0;
#else
    return false;
#endif
}

static const char* ReadFile( const char* path )
{
    int fd = open( path, O_RDONLY );
    if( fd < 0 ) return nullptr;

    static char tmp[64];
    const auto cnt = read( fd, tmp, 63 );
    close( fd );
    if( cnt < 0 ) return nullptr;
    tmp[cnt] = '\0';
    return tmp;
}

bool SysTraceStart( int64_t& samplingPeriod )
{
#ifndef CLOCK_MONOTONIC_RAW
    return false;
#endif

    const auto paranoidLevelStr = ReadFile( "/proc/sys/kernel/perf_event_paranoid" );
    if( !paranoidLevelStr ) return false;
#ifdef TRACY_VERBOSE
    int paranoidLevel = 2;
    paranoidLevel = atoi( paranoidLevelStr );
    TracyDebug( "perf_event_paranoid: %i\n", paranoidLevel );
#endif

    int switchId = -1, wakeupId = -1, vsyncId = -1;
    const auto switchIdStr = ReadFile( "/sys/kernel/debug/tracing/events/sched/sched_switch/id" );
    if( switchIdStr ) switchId = atoi( switchIdStr );
    const auto wakeupIdStr = ReadFile( "/sys/kernel/debug/tracing/events/sched/sched_wakeup/id" );
    if( wakeupIdStr ) wakeupId = atoi( wakeupIdStr );
    const auto vsyncIdStr = ReadFile( "/sys/kernel/debug/tracing/events/drm/drm_vblank_event/id" );
    if( vsyncIdStr ) vsyncId = atoi( vsyncIdStr );

    TracyDebug( "sched_switch id: %i\n", switchId );
    TracyDebug( "sched_wakeup id: %i\n", wakeupId );
    TracyDebug( "drm_vblank_event id: %i\n", vsyncId );

#ifdef TRACY_NO_SAMPLE_RETIREMENT
    const bool noRetirement = true;
#else
    const char* noRetirementEnv = GetEnvVar( "TRACY_NO_SAMPLE_RETIREMENT" );
    const bool noRetirement = noRetirementEnv && noRetirementEnv[0] == '1';
#endif

#ifdef TRACY_NO_SAMPLE_CACHE
    const bool noCache = true;
#else
    const char* noCacheEnv = GetEnvVar( "TRACY_NO_SAMPLE_CACHE" );
    const bool noCache = noCacheEnv && noCacheEnv[0] == '1';
#endif

#ifdef TRACY_NO_SAMPLE_BRANCH
    const bool noBranch = true;
#else
    const char* noBranchEnv = GetEnvVar( "TRACY_NO_SAMPLE_BRANCH" );
    const bool noBranch = noBranchEnv && noBranchEnv[0] == '1';
#endif

#ifdef TRACY_NO_CONTEXT_SWITCH
    const bool noCtxSwitch = true;
#else
    const char* noCtxSwitchEnv = GetEnvVar( "TRACY_NO_CONTEXT_SWITCH" );
    const bool noCtxSwitch = noCtxSwitchEnv && noCtxSwitchEnv[0] == '1';
#endif

#ifdef TRACY_NO_VSYNC_CAPTURE
    const bool noVsync = true;
#else
    const char* noVsyncEnv = GetEnvVar( "TRACY_NO_VSYNC_CAPTURE" );
    const bool noVsync = noVsyncEnv && noVsyncEnv[0] == '1';
#endif

    samplingPeriod = GetSamplingPeriod();
    uint32_t currentPid = (uint32_t)getpid();

    s_numCpus = (int)std::thread::hardware_concurrency();

    const auto maxNumBuffers = s_numCpus * (
        1 +     // software sampling
        2 +     // CPU cycles + instructions retired
        2 +     // cache reference + miss
        2 +     // branch retired + miss
        2 +     // context switches + wakeups
        1       // vsync
    );
    s_ring = (RingBuffer*)tracy_malloc( sizeof( RingBuffer ) * maxNumBuffers );
    s_numBuffers = 0;

    // software sampling
    perf_event_attr pe = {};
    pe.type = PERF_TYPE_SOFTWARE;
    pe.size = sizeof( perf_event_attr );
    pe.config = PERF_COUNT_SW_CPU_CLOCK;
    pe.sample_freq = GetSamplingFrequency();
    pe.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN;
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 4, 8, 0 )
    pe.sample_max_stack = 127;
#endif
    pe.disabled = 1;
    pe.freq = 1;
    pe.inherit = 1;
#if !defined TRACY_HW_TIMER || !( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
    pe.use_clockid = 1;
    pe.clockid = CLOCK_MONOTONIC_RAW;
#endif

    TracyDebug( "Setup software sampling\n" );
    ProbePreciseIp( pe, currentPid );
    for( int i=0; i<s_numCpus; i++ )
    {
        int fd = perf_event_open( &pe, currentPid, i, -1, PERF_FLAG_FD_CLOEXEC );
        if( fd == -1 )
        {
            pe.exclude_kernel = 1;
            ProbePreciseIp( pe, currentPid );
            fd = perf_event_open( &pe, currentPid, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd == -1 )
            {
                TracyDebug( "  Failed to setup!\n");
                break;
            }
            TracyDebug( "  No access to kernel samples\n" );
        }
        new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventCallstack );
        if( s_ring[s_numBuffers].IsValid() )
        {
            s_numBuffers++;
            TracyDebug( "  Core %i ok\n", i );
        }
    }

    // CPU cycles + instructions retired
    pe = {};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof( perf_event_attr );
    pe.sample_freq = 5000;
    pe.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TIME;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_guest = 1;
    pe.exclude_hv = 1;
    pe.freq = 1;
    pe.inherit = 1;
#if !defined TRACY_HW_TIMER || !( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
    pe.use_clockid = 1;
    pe.clockid = CLOCK_MONOTONIC_RAW;
#endif

    if( !noRetirement )
    {
        TracyDebug( "Setup sampling cycles + retirement\n" );
        ProbePreciseIp( pe, PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS, currentPid );
        for( int i=0; i<s_numCpus; i++ )
        {
            const int fd = perf_event_open( &pe, currentPid, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd != -1 )
            {
                new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventCpuCycles );
                if( s_ring[s_numBuffers].IsValid() )
                {
                    s_numBuffers++;
                    TracyDebug( "  Core %i ok\n", i );
                }
            }
        }

        pe.config = PERF_COUNT_HW_INSTRUCTIONS;
        for( int i=0; i<s_numCpus; i++ )
        {
            const int fd = perf_event_open( &pe, currentPid, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd != -1 )
            {
                new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventInstructionsRetired );
                if( s_ring[s_numBuffers].IsValid() )
                {
                    s_numBuffers++;
                    TracyDebug( "  Core %i ok\n", i );
                }
            }
        }
    }

    // cache reference + miss
    if( !noCache )
    {
        TracyDebug( "Setup sampling CPU cache references + misses\n" );
        ProbePreciseIp( pe, PERF_COUNT_HW_CACHE_REFERENCES, PERF_COUNT_HW_CACHE_MISSES, currentPid );
        if( IsGenuineIntel() )
        {
            pe.precise_ip = 0;
            TracyDebug( "  CPU is GenuineIntel, forcing precise_ip down to 0\n" );
        }
        for( int i=0; i<s_numCpus; i++ )
        {
            const int fd = perf_event_open( &pe, currentPid, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd != -1 )
            {
                new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventCacheReference );
                if( s_ring[s_numBuffers].IsValid() )
                {
                    s_numBuffers++;
                    TracyDebug( "  Core %i ok\n", i );
                }
            }
        }

        pe.config = PERF_COUNT_HW_CACHE_MISSES;
        for( int i=0; i<s_numCpus; i++ )
        {
            const int fd = perf_event_open( &pe, currentPid, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd != -1 )
            {
                new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventCacheMiss );
                if( s_ring[s_numBuffers].IsValid() )
                {
                    s_numBuffers++;
                    TracyDebug( "  Core %i ok\n", i );
                }
            }
        }
    }

    // branch retired + miss
    if( !noBranch )
    {
        TracyDebug( "Setup sampling CPU branch retirements + misses\n" );
        ProbePreciseIp( pe, PERF_COUNT_HW_BRANCH_INSTRUCTIONS, PERF_COUNT_HW_BRANCH_MISSES, currentPid );
        for( int i=0; i<s_numCpus; i++ )
        {
            const int fd = perf_event_open( &pe, currentPid, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd != -1 )
            {
                new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventBranchRetired );
                if( s_ring[s_numBuffers].IsValid() )
                {
                    s_numBuffers++;
                    TracyDebug( "  Core %i ok\n", i );
                }
            }
        }

        pe.config = PERF_COUNT_HW_BRANCH_MISSES;
        for( int i=0; i<s_numCpus; i++ )
        {
            const int fd = perf_event_open( &pe, currentPid, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd != -1 )
            {
                new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventBranchMiss );
                if( s_ring[s_numBuffers].IsValid() )
                {
                    s_numBuffers++;
                    TracyDebug( "  Core %i ok\n", i );
                }
            }
        }
    }

    s_ctxBufferIdx = s_numBuffers;

    // vsync
    if( !noVsync && vsyncId != -1 )
    {
        pe = {};
        pe.type = PERF_TYPE_TRACEPOINT;
        pe.size = sizeof( perf_event_attr );
        pe.sample_period = 1;
        pe.sample_type = PERF_SAMPLE_TIME | PERF_SAMPLE_RAW;
        pe.disabled = 1;
        pe.config = vsyncId;
#if !defined TRACY_HW_TIMER || !( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
        pe.use_clockid = 1;
        pe.clockid = CLOCK_MONOTONIC_RAW;
#endif

        TracyDebug( "Setup vsync capture\n" );
        for( int i=0; i<s_numCpus; i++ )
        {
            const int fd = perf_event_open( &pe, -1, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd != -1 )
            {
                new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventVsync, i );
                if( s_ring[s_numBuffers].IsValid() )
                {
                    s_numBuffers++;
                    TracyDebug( "  Core %i ok\n", i );
                }
            }
        }
    }

    // context switches
    if( !noCtxSwitch && switchId != -1 )
    {
        pe = {};
        pe.type = PERF_TYPE_TRACEPOINT;
        pe.size = sizeof( perf_event_attr );
        pe.sample_period = 1;
        pe.sample_type = PERF_SAMPLE_TIME | PERF_SAMPLE_RAW | PERF_SAMPLE_CALLCHAIN;
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 4, 8, 0 )
        pe.sample_max_stack = 127;
#endif
        pe.disabled = 1;
        pe.inherit = 1;
        pe.config = switchId;
#if !defined TRACY_HW_TIMER || !( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
        pe.use_clockid = 1;
        pe.clockid = CLOCK_MONOTONIC_RAW;
#endif

        TracyDebug( "Setup context switch capture\n" );
        for( int i=0; i<s_numCpus; i++ )
        {
            const int fd = perf_event_open( &pe, -1, i, -1, PERF_FLAG_FD_CLOEXEC );
            if( fd != -1 )
            {
                new( s_ring+s_numBuffers ) RingBuffer( 256*1024, fd, EventContextSwitch, i );
                if( s_ring[s_numBuffers].IsValid() )
                {
                    s_numBuffers++;
                    TracyDebug( "  Core %i ok\n", i );
                }
            }
        }

        if( wakeupId != -1 )
        {
            pe.config = wakeupId;
            pe.config &= ~PERF_SAMPLE_CALLCHAIN;

            TracyDebug( "Setup wakeup capture\n" );
            for( int i=0; i<s_numCpus; i++ )
            {
                const int fd = perf_event_open( &pe, -1, i, -1, PERF_FLAG_FD_CLOEXEC );
                if( fd != -1 )
                {
                    new( s_ring+s_numBuffers ) RingBuffer( 64*1024, fd, EventWakeup, i );
                    if( s_ring[s_numBuffers].IsValid() )
                    {
                        s_numBuffers++;
                        TracyDebug( "  Core %i ok\n", i );
                    }
                }
            }
        }
    }

    TracyDebug( "Ringbuffers in use: %i\n", s_numBuffers );

    traceActive.store( true, std::memory_order_relaxed );
    return true;
}

void SysTraceStop()
{
    traceActive.store( false, std::memory_order_relaxed );
}

static uint64_t* GetCallstackBlock( uint64_t cnt, RingBuffer& ring, uint64_t offset )
{
    auto trace = (uint64_t*)tracy_malloc_fast( ( 1 + cnt ) * sizeof( uint64_t ) );
    ring.Read( trace+1, offset, sizeof( uint64_t ) * cnt );

#if defined __x86_64__ || defined _M_X64
    // remove non-canonical pointers
    do
    {
        const auto test = (int64_t)trace[cnt];
        const auto m1 = test >> 63;
        const auto m2 = test >> 47;
        if( m1 == m2 ) break;
    }
    while( --cnt > 0 );
    for( uint64_t j=1; j<cnt; j++ )
    {
        const auto test = (int64_t)trace[j];
        const auto m1 = test >> 63;
        const auto m2 = test >> 47;
        if( m1 != m2 ) trace[j] = 0;
    }
#endif

    for( uint64_t j=1; j<=cnt; j++ )
    {
        if( trace[j] >= (uint64_t)-4095 )       // PERF_CONTEXT_MAX
        {
            memmove( trace+j, trace+j+1, sizeof( uint64_t ) * ( cnt - j ) );
            cnt--;
        }
    }

    memcpy( trace, &cnt, sizeof( uint64_t ) );
    return trace;
}

void SysTraceWorker( void* ptr )
{
    ThreadExitHandler threadExitHandler;
    SetThreadName( "Tracy Sampling" );
    InitRpmalloc();
    sched_param sp = { 99 };
    if( pthread_setschedparam( pthread_self(), SCHED_FIFO, &sp ) != 0 ) TracyDebug( "Failed to increase SysTraceWorker thread priority!\n" );
    auto ctxBufferIdx = s_ctxBufferIdx;
    auto ringArray = s_ring;
    auto numBuffers = s_numBuffers;
    for( int i=0; i<numBuffers; i++ ) ringArray[i].Enable();
    for(;;)
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() )
        {
            if( !traceActive.load( std::memory_order_relaxed ) ) break;
            for( int i=0; i<numBuffers; i++ )
            {
                auto& ring = ringArray[i];
                const auto head = ring.LoadHead();
                const auto tail = ring.GetTail();
                if( head != tail )
                {
                    const auto end = head - tail;
                    ring.Advance( end );
                }
            }
            if( !traceActive.load( std::memory_order_relaxed ) ) break;
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
            continue;
        }
#endif

        bool hadData = false;
        for( int i=0; i<ctxBufferIdx; i++ )
        {
            if( !traceActive.load( std::memory_order_relaxed ) ) break;
            auto& ring = ringArray[i];
            const auto head = ring.LoadHead();
            const auto tail = ring.GetTail();
            if( head == tail ) continue;
            assert( head > tail );
            hadData = true;

            const auto id = ring.GetId();
            assert( id != EventContextSwitch );
            const auto end = head - tail;
            uint64_t pos = 0;
            if( id == EventCallstack )
            {
                while( pos < end )
                {
                    perf_event_header hdr;
                    ring.Read( &hdr, pos, sizeof( perf_event_header ) );
                    if( hdr.type == PERF_RECORD_SAMPLE )
                    {
                        auto offset = pos + sizeof( perf_event_header );

                        // Layout:
                        //   u32 pid, tid
                        //   u64 time
                        //   u64 cnt
                        //   u64 ip[cnt]

                        uint32_t tid;
                        uint64_t t0;
                        uint64_t cnt;

                        offset += sizeof( uint32_t );
                        ring.Read( &tid, offset, sizeof( uint32_t ) );
                        offset += sizeof( uint32_t );
                        ring.Read( &t0, offset, sizeof( uint64_t ) );
                        offset += sizeof( uint64_t );
                        ring.Read( &cnt, offset, sizeof( uint64_t ) );
                        offset += sizeof( uint64_t );

                        if( cnt > 0 )
                        {
#if defined TRACY_HW_TIMER && ( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
                            t0 = ring.ConvertTimeToTsc( t0 );
#endif
                            auto trace = GetCallstackBlock( cnt, ring, offset );

                            TracyLfqPrepare( QueueType::CallstackSample );
                            MemWrite( &item->callstackSampleFat.time, t0 );
                            MemWrite( &item->callstackSampleFat.thread, tid );
                            MemWrite( &item->callstackSampleFat.ptr, (uint64_t)trace );
                            TracyLfqCommit;
                        }
                    }
                    pos += hdr.size;
                }
            }
            else
            {
                while( pos < end )
                {
                    perf_event_header hdr;
                    ring.Read( &hdr, pos, sizeof( perf_event_header ) );
                    if( hdr.type == PERF_RECORD_SAMPLE )
                    {
                        auto offset = pos + sizeof( perf_event_header );

                        // Layout:
                        //   u64 ip
                        //   u64 time

                        uint64_t ip, t0;
                        ring.Read( &ip, offset, sizeof( uint64_t ) );
                        offset += sizeof( uint64_t );
                        ring.Read( &t0, offset, sizeof( uint64_t ) );

#if defined TRACY_HW_TIMER && ( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
                        t0 = ring.ConvertTimeToTsc( t0 );
#endif
                        QueueType type;
                        switch( id )
                        {
                        case EventCpuCycles:
                            type = QueueType::HwSampleCpuCycle;
                            break;
                        case EventInstructionsRetired:
                            type = QueueType::HwSampleInstructionRetired;
                            break;
                        case EventCacheReference:
                            type = QueueType::HwSampleCacheReference;
                            break;
                        case EventCacheMiss:
                            type = QueueType::HwSampleCacheMiss;
                            break;
                        case EventBranchRetired:
                            type = QueueType::HwSampleBranchRetired;
                            break;
                        case EventBranchMiss:
                            type = QueueType::HwSampleBranchMiss;
                            break;
                        default:
                            abort();
                        }

                        TracyLfqPrepare( type );
                        MemWrite( &item->hwSample.ip, ip );
                        MemWrite( &item->hwSample.time, t0 );
                        TracyLfqCommit;
                    }
                    pos += hdr.size;
                }
            }
            assert( pos == end );
            ring.Advance( end );
        }
        if( !traceActive.load( std::memory_order_relaxed ) ) break;

        if( ctxBufferIdx != numBuffers )
        {
            const auto ctxBufNum = numBuffers - ctxBufferIdx;

            int activeNum = 0;
            uint16_t active[512];
            uint32_t end[512];
            uint32_t pos[512];
            for( int i=0; i<ctxBufNum; i++ )
            {
                const auto rbIdx = ctxBufferIdx + i;
                const auto rbHead = ringArray[rbIdx].LoadHead();
                const auto rbTail = ringArray[rbIdx].GetTail();
                const auto rbActive = rbHead != rbTail;

                if( rbActive )
                {
                    active[activeNum] = (uint16_t)i;
                    activeNum++;
                    end[i] = rbHead - rbTail;
                    pos[i] = 0;
                }
                else
                {
                    end[i] = 0;
                }
            }
            if( activeNum > 0 )
            {
                hadData = true;
                while( activeNum > 0 )
                {
                    int sel = -1;
                    int selPos;
                    int64_t t0 = std::numeric_limits<int64_t>::max();
                    for( int i=0; i<activeNum; i++ )
                    {
                        auto idx = active[i];
                        auto rbPos = pos[idx];
                        assert( rbPos < end[idx] );
                        const auto rbIdx = ctxBufferIdx + idx;
                        perf_event_header hdr;
                        ringArray[rbIdx].Read( &hdr, rbPos, sizeof( perf_event_header ) );
                        if( hdr.type == PERF_RECORD_SAMPLE )
                        {
                            int64_t rbTime;
                            ringArray[rbIdx].Read( &rbTime, rbPos + sizeof( perf_event_header ), sizeof( int64_t ) );
                            if( rbTime < t0 )
                            {
                                t0 = rbTime;
                                sel = idx;
                                selPos = i;
                            }
                        }
                        else
                        {
                            rbPos += hdr.size;
                            if( rbPos == end[idx] )
                            {
                                memmove( active+i, active+i+1, sizeof(*active) * ( activeNum - i - 1 ) );
                                activeNum--;
                                i--;
                            }
                            else
                            {
                                pos[idx] = rbPos;
                            }
                        }
                    }
                    if( sel >= 0 )
                    {
                        auto& ring = ringArray[ctxBufferIdx + sel];
                        auto rbPos = pos[sel];
                        auto offset = rbPos;
                        perf_event_header hdr;
                        ring.Read( &hdr, offset, sizeof( perf_event_header ) );

#if defined TRACY_HW_TIMER && ( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
                        t0 = ring.ConvertTimeToTsc( t0 );
#endif

                        const auto rid = ring.GetId();
                        if( rid == EventContextSwitch )
                        {
                            // Layout:
                            //   u64 time
                            //   u64 cnt
                            //   u64 ip[cnt]
                            //   u32 size
                            //   u8  data[size]
                            // Data (not ABI stable, but has not changed since it was added, in 2009):
                            //   u8  hdr[8]
                            //   u8  prev_comm[16]
                            //   u32 prev_pid
                            //   u32 prev_prio
                            //   lng prev_state
                            //   u8  next_comm[16]
                            //   u32 next_pid
                            //   u32 next_prio

                            offset += sizeof( perf_event_header ) + sizeof( uint64_t );

                            uint64_t cnt;
                            ring.Read( &cnt, offset, sizeof( uint64_t ) );
                            offset += sizeof( uint64_t );
                            const auto traceOffset = offset;
                            offset += sizeof( uint64_t ) * cnt + sizeof( uint32_t ) + 8 + 16;

                            uint32_t prev_pid, next_pid;
                            long prev_state;

                            ring.Read( &prev_pid, offset, sizeof( uint32_t ) );
                            offset += sizeof( uint32_t ) + sizeof( uint32_t );
                            ring.Read( &prev_state, offset, sizeof( long ) );
                            offset += sizeof( long ) + 16;
                            ring.Read( &next_pid, offset, sizeof( uint32_t ) );

                            uint8_t reason = 100;
                            uint8_t state;

                            if(      prev_state & 0x0001 ) state = 104;
                            else if( prev_state & 0x0002 ) state = 101;
                            else if( prev_state & 0x0004 ) state = 105;
                            else if( prev_state & 0x0008 ) state = 106;
                            else if( prev_state & 0x0010 ) state = 108;
                            else if( prev_state & 0x0020 ) state = 109;
                            else if( prev_state & 0x0040 ) state = 110;
                            else if( prev_state & 0x0080 ) state = 102;
                            else                           state = 103;

                            TracyLfqPrepare( QueueType::ContextSwitch );
                            MemWrite( &item->contextSwitch.time, t0 );
                            MemWrite( &item->contextSwitch.oldThread, prev_pid );
                            MemWrite( &item->contextSwitch.newThread, next_pid );
                            MemWrite( &item->contextSwitch.cpu, uint8_t( ring.GetCpu() ) );
                            MemWrite( &item->contextSwitch.reason, reason );
                            MemWrite( &item->contextSwitch.state, state );
                            TracyLfqCommit;

                            if( cnt > 0 && prev_pid != 0 && CurrentProcOwnsThread( prev_pid ) )
                            {
                                auto trace = GetCallstackBlock( cnt, ring, traceOffset );

                                TracyLfqPrepare( QueueType::CallstackSampleContextSwitch );
                                MemWrite( &item->callstackSampleFat.time, t0 );
                                MemWrite( &item->callstackSampleFat.thread, prev_pid );
                                MemWrite( &item->callstackSampleFat.ptr, (uint64_t)trace );
                                TracyLfqCommit;
                            }
                        }
                        else if( rid == EventWakeup )
                        {
                            // Layout:
                            //   u64 time
                            //   u32 size
                            //   u8  data[size]
                            // Data:
                            //   u8  hdr[8]
                            //   u8  comm[16]
                            //   u32 pid
                            //   u32 prio
                            //   u64 target_cpu

                            offset += sizeof( perf_event_header ) + sizeof( uint64_t ) + sizeof( uint32_t ) + 8 + 16;

                            uint32_t pid;
                            ring.Read( &pid, offset, sizeof( uint32_t ) );

                            TracyLfqPrepare( QueueType::ThreadWakeup );
                            MemWrite( &item->threadWakeup.time, t0 );
                            MemWrite( &item->threadWakeup.thread, pid );
                            TracyLfqCommit;
                        }
                        else
                        {
                            assert( rid == EventVsync );
                            // Layout:
                            //   u64 time
                            //   u32 size
                            //   u8  data[size]
                            // Data (not ABI stable):
                            //   u8  hdr[8]
                            //   i32 crtc
                            //   u32 seq
                            //   i64 ktime
                            //   u8  high precision

                            offset += sizeof( perf_event_header ) + sizeof( uint64_t ) + sizeof( uint32_t ) + 8;

                            int32_t crtc;
                            ring.Read( &crtc, offset, sizeof( int32_t ) );

                            // Note: The timestamp value t0 might be off by a number of microseconds from the
                            // true hardware vblank event. The ktime value should be used instead, but it is
                            // measured in CLOCK_MONOTONIC time. Tracy only supports the timestamp counter
                            // register (TSC) or CLOCK_MONOTONIC_RAW clock.
#if 0
                            offset += sizeof( uint32_t ) * 2;
                            int64_t ktime;
                            ring.Read( &ktime, offset, sizeof( int64_t ) );
#endif

                            TracyLfqPrepare( QueueType::FrameVsync );
                            MemWrite( &item->frameVsync.id, crtc );
                            MemWrite( &item->frameVsync.time, t0 );
                            TracyLfqCommit;
                        }

                        rbPos += hdr.size;
                        if( rbPos == end[sel] )
                        {
                            memmove( active+selPos, active+selPos+1, sizeof(*active) * ( activeNum - selPos - 1 ) );
                            activeNum--;
                        }
                        else
                        {
                            pos[sel] = rbPos;
                        }
                    }
                }
                for( int i=0; i<ctxBufNum; i++ )
                {
                    if( end[i] != 0 ) ringArray[ctxBufferIdx + i].Advance( end[i] );
                }
            }
        }
        if( !traceActive.load( std::memory_order_relaxed ) ) break;
        if( !hadData )
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
        }
    }

    for( int i=0; i<numBuffers; i++ ) ringArray[i].~RingBuffer();
    tracy_free_fast( ringArray );
}

void SysTraceGetExternalName( uint64_t thread, const char*& threadName, const char*& name )
{
    FILE* f;
    char fn[256];
    sprintf( fn, "/proc/%" PRIu64 "/comm", thread );
    f = fopen( fn, "rb" );
    if( f )
    {
        char buf[256];
        const auto sz = fread( buf, 1, 256, f );
        if( sz > 0 && buf[sz-1] == '\n' ) buf[sz-1] = '\0';
        threadName = CopyString( buf );
        fclose( f );
    }
    else
    {
        threadName = CopyString( "???", 3 );
    }

    sprintf( fn, "/proc/%" PRIu64 "/status", thread );
    f = fopen( fn, "rb" );
    if( f )
    {
        char* tmp = (char*)tracy_malloc_fast( 8*1024 );
        const auto fsz = (ptrdiff_t)fread( tmp, 1, 8*1024, f );
        fclose( f );

        int pid = -1;
        auto line = tmp;
        for(;;)
        {
            if( memcmp( "Tgid:\t", line, 6 ) == 0 )
            {
                pid = atoi( line + 6 );
                break;
            }
            while( line - tmp < fsz && *line != '\n' ) line++;
            if( *line != '\n' ) break;
            line++;
        }
        tracy_free_fast( tmp );

        if( pid >= 0 )
        {
            {
                uint64_t _pid = pid;
                TracyLfqPrepare( QueueType::TidToPid );
                MemWrite( &item->tidToPid.tid, thread );
                MemWrite( &item->tidToPid.pid, _pid );
                TracyLfqCommit;
            }
            sprintf( fn, "/proc/%i/comm", pid );
            f = fopen( fn, "rb" );
            if( f )
            {
                char buf[256];
                const auto sz = fread( buf, 1, 256, f );
                if( sz > 0 && buf[sz-1] == '\n' ) buf[sz-1] = '\0';
                name = CopyStringFast( buf );
                fclose( f );
                return;
            }
        }
    }
    name = CopyStringFast( "???", 3 );
}

}

#  endif

#endif
