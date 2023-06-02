#ifndef __TRACYPROFILER_HPP__
#define __TRACYPROFILER_HPP__

#include <assert.h>
#include <atomic>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "tracy_concurrentqueue.h"
#include "tracy_SPSCQueue.h"
#include "TracyCallstack.hpp"
#include "TracySysTime.hpp"
#include "TracyFastVector.hpp"
#include "../common/TracyQueue.hpp"
#include "../common/TracyAlign.hpp"
#include "../common/TracyAlloc.hpp"
#include "../common/TracyMutex.hpp"
#include "../common/TracyProtocol.hpp"

#if defined _WIN32
#  include <intrin.h>
#endif
#ifdef __APPLE__
#  include <TargetConditionals.h>
#  include <mach/mach_time.h>
#endif

#if ( defined _WIN32 || ( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 ) || ( defined TARGET_OS_IOS && TARGET_OS_IOS == 1 ) )
#  define TRACY_HW_TIMER
#endif

#ifdef __linux__
#  include <signal.h>
#endif

#if defined TRACY_TIMER_FALLBACK || !defined TRACY_HW_TIMER
#  include <chrono>
#endif

#ifndef TracyConcat
#  define TracyConcat(x,y) TracyConcatIndirect(x,y)
#endif
#ifndef TracyConcatIndirect
#  define TracyConcatIndirect(x,y) x##y
#endif

namespace tracy
{
#if defined(TRACY_DELAYED_INIT) && defined(TRACY_MANUAL_LIFETIME)
TRACY_API void StartupProfiler();
TRACY_API void ShutdownProfiler();
#endif

class GpuCtx;
class Profiler;
class Socket;
class UdpBroadcast;

struct GpuCtxWrapper
{
    GpuCtx* ptr;
};

TRACY_API moodycamel::ConcurrentQueue<QueueItem>::ExplicitProducer* GetToken();
TRACY_API Profiler& GetProfiler();
TRACY_API std::atomic<uint32_t>& GetLockCounter();
TRACY_API std::atomic<uint8_t>& GetGpuCtxCounter();
TRACY_API GpuCtxWrapper& GetGpuCtx();
TRACY_API uint32_t GetThreadHandle();
TRACY_API bool ProfilerAvailable();
TRACY_API bool ProfilerAllocatorAvailable();
TRACY_API int64_t GetFrequencyQpc();

#if defined TRACY_TIMER_FALLBACK && defined TRACY_HW_TIMER && ( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 )
TRACY_API bool HardwareSupportsInvariantTSC();  // check, if we need fallback scenario
#else
#  if defined TRACY_HW_TIMER
tracy_force_inline bool HardwareSupportsInvariantTSC()
{
    return true;  // this is checked at startup
}
#  else
tracy_force_inline bool HardwareSupportsInvariantTSC()
{
    return false;
}
#  endif
#endif


struct SourceLocationData
{
    const char* name;
    const char* function;
    const char* file;
    uint32_t line;
    uint32_t color;
};

#ifdef TRACY_ON_DEMAND
struct LuaZoneState
{
    uint32_t counter;
    bool active;
};
#endif


#define TracyLfqPrepare( _type ) \
    moodycamel::ConcurrentQueueDefaultTraits::index_t __magic; \
    auto __token = GetToken(); \
    auto& __tail = __token->get_tail_index(); \
    auto item = __token->enqueue_begin( __magic ); \
    MemWrite( &item->hdr.type, _type );

#define TracyLfqCommit \
    __tail.store( __magic + 1, std::memory_order_release );

#define TracyLfqPrepareC( _type ) \
    tracy::moodycamel::ConcurrentQueueDefaultTraits::index_t __magic; \
    auto __token = tracy::GetToken(); \
    auto& __tail = __token->get_tail_index(); \
    auto item = __token->enqueue_begin( __magic ); \
    tracy::MemWrite( &item->hdr.type, _type );

#define TracyLfqCommitC \
    __tail.store( __magic + 1, std::memory_order_release );


#ifdef TRACY_FIBERS
#  define TracyQueuePrepare( _type ) \
    auto item = Profiler::QueueSerial(); \
    MemWrite( &item->hdr.type, _type );
#  define TracyQueueCommit( _name ) \
    MemWrite( &item->_name.thread, GetThreadHandle() ); \
    Profiler::QueueSerialFinish();
#  define TracyQueuePrepareC( _type ) \
    auto item = tracy::Profiler::QueueSerial(); \
    tracy::MemWrite( &item->hdr.type, _type );
#  define TracyQueueCommitC( _name ) \
    tracy::MemWrite( &item->_name.thread, tracy::GetThreadHandle() ); \
    tracy::Profiler::QueueSerialFinish();
#else
#  define TracyQueuePrepare( _type ) TracyLfqPrepare( _type )
#  define TracyQueueCommit( _name ) TracyLfqCommit
#  define TracyQueuePrepareC( _type ) TracyLfqPrepareC( _type )
#  define TracyQueueCommitC( _name ) TracyLfqCommitC
#endif


typedef void(*ParameterCallback)( void* data, uint32_t idx, int32_t val );
typedef char*(*SourceContentsCallback)( void* data, const char* filename, size_t& size );

class Profiler
{
    struct FrameImageQueueItem
    {
        void* image;
        uint32_t frame;
        uint16_t w;
        uint16_t h;
        bool flip;
    };

    enum class SymbolQueueItemType
    {
        CallstackFrame,
        SymbolQuery,
        ExternalName,
        KernelCode,
        SourceCode
    };

    struct SymbolQueueItem
    {
        SymbolQueueItemType type;
        uint64_t ptr;
        uint64_t extra;
        uint32_t id;
    };

public:
    Profiler();
    ~Profiler();

    void SpawnWorkerThreads();

    static tracy_force_inline int64_t GetTime()
    {
#ifdef TRACY_HW_TIMER
#  if defined TARGET_OS_IOS && TARGET_OS_IOS == 1
        if( HardwareSupportsInvariantTSC() ) return mach_absolute_time();
#  elif defined _WIN32
#    ifdef TRACY_TIMER_QPC
        return GetTimeQpc();
#    else
        if( HardwareSupportsInvariantTSC() ) return int64_t( __rdtsc() );
#    endif
#  elif defined __i386 || defined _M_IX86
        if( HardwareSupportsInvariantTSC() )
        {
            uint32_t eax, edx;
            asm volatile ( "rdtsc" : "=a" (eax), "=d" (edx) );
            return ( uint64_t( edx ) << 32 ) + uint64_t( eax );
        }
#  elif defined __x86_64__ || defined _M_X64
        if( HardwareSupportsInvariantTSC() )
        {
            uint64_t rax, rdx;
            asm volatile ( "rdtsc" : "=a" (rax), "=d" (rdx) );
            return (int64_t)(( rdx << 32 ) + rax);
        }
#  else
#    error "TRACY_HW_TIMER detection logic needs fixing"
#  endif
#endif

#if !defined TRACY_HW_TIMER || defined TRACY_TIMER_FALLBACK
#  if defined __linux__ && defined CLOCK_MONOTONIC_RAW
        struct timespec ts;
        clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
        return int64_t( ts.tv_sec ) * 1000000000ll + int64_t( ts.tv_nsec );
#  else
        return std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::high_resolution_clock::now().time_since_epoch() ).count();
#  endif
#endif

#if !defined TRACY_TIMER_FALLBACK
        return 0;  // unreachable branch
#endif
    }

    tracy_force_inline uint32_t GetNextZoneId()
    {
        return m_zoneId.fetch_add( 1, std::memory_order_relaxed );
    }

    static tracy_force_inline QueueItem* QueueSerial()
    {
        auto& p = GetProfiler();
        p.m_serialLock.lock();
        return p.m_serialQueue.prepare_next();
    }

    static tracy_force_inline QueueItem* QueueSerialCallstack( void* ptr )
    {
        auto& p = GetProfiler();
        p.m_serialLock.lock();
        p.SendCallstackSerial( ptr );
        return p.m_serialQueue.prepare_next();
    }

    static tracy_force_inline void QueueSerialFinish()
    {
        auto& p = GetProfiler();
        p.m_serialQueue.commit_next();
        p.m_serialLock.unlock();
    }

    static tracy_force_inline void SendFrameMark( const char* name )
    {
        if( !name ) GetProfiler().m_frameCount.fetch_add( 1, std::memory_order_relaxed );
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        auto item = QueueSerial();
        MemWrite( &item->hdr.type, QueueType::FrameMarkMsg );
        MemWrite( &item->frameMark.time, GetTime() );
        MemWrite( &item->frameMark.name, uint64_t( name ) );
        QueueSerialFinish();
    }

    static tracy_force_inline void SendFrameMark( const char* name, QueueType type )
    {
        assert( type == QueueType::FrameMarkMsgStart || type == QueueType::FrameMarkMsgEnd );
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        auto item = QueueSerial();
        MemWrite( &item->hdr.type, type );
        MemWrite( &item->frameMark.time, GetTime() );
        MemWrite( &item->frameMark.name, uint64_t( name ) );
        QueueSerialFinish();
    }

    static tracy_force_inline void SendFrameImage( const void* image, uint16_t w, uint16_t h, uint8_t offset, bool flip )
    {
#ifndef TRACY_NO_FRAME_IMAGE
        auto& profiler = GetProfiler();
        assert( profiler.m_frameCount.load( std::memory_order_relaxed ) < std::numeric_limits<uint32_t>::max() );
#  ifdef TRACY_ON_DEMAND
        if( !profiler.IsConnected() ) return;
#  endif
        const auto sz = size_t( w ) * size_t( h ) * 4;
        auto ptr = (char*)tracy_malloc( sz );
        memcpy( ptr, image, sz );

        profiler.m_fiLock.lock();
        auto fi = profiler.m_fiQueue.prepare_next();
        fi->image = ptr;
        fi->frame = uint32_t( profiler.m_frameCount.load( std::memory_order_relaxed ) - offset );
        fi->w = w;
        fi->h = h;
        fi->flip = flip;
        profiler.m_fiQueue.commit_next();
        profiler.m_fiLock.unlock();
#endif
    }

    static tracy_force_inline void PlotData( const char* name, int64_t val )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        TracyLfqPrepare( QueueType::PlotDataInt );
        MemWrite( &item->plotDataInt.name, (uint64_t)name );
        MemWrite( &item->plotDataInt.time, GetTime() );
        MemWrite( &item->plotDataInt.val, val );
        TracyLfqCommit;
    }

    static tracy_force_inline void PlotData( const char* name, float val )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        TracyLfqPrepare( QueueType::PlotDataFloat );
        MemWrite( &item->plotDataFloat.name, (uint64_t)name );
        MemWrite( &item->plotDataFloat.time, GetTime() );
        MemWrite( &item->plotDataFloat.val, val );
        TracyLfqCommit;
    }

    static tracy_force_inline void PlotData( const char* name, double val )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        TracyLfqPrepare( QueueType::PlotDataDouble );
        MemWrite( &item->plotDataDouble.name, (uint64_t)name );
        MemWrite( &item->plotDataDouble.time, GetTime() );
        MemWrite( &item->plotDataDouble.val, val );
        TracyLfqCommit;
    }

    static tracy_force_inline void ConfigurePlot( const char* name, PlotFormatType type, bool step, bool fill, uint32_t color )
    {
        TracyLfqPrepare( QueueType::PlotConfig );
        MemWrite( &item->plotConfig.name, (uint64_t)name );
        MemWrite( &item->plotConfig.type, (uint8_t)type );
        MemWrite( &item->plotConfig.step, (uint8_t)step );
        MemWrite( &item->plotConfig.fill, (uint8_t)fill );
        MemWrite( &item->plotConfig.color, color );

#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif

        TracyLfqCommit;
    }

    static tracy_force_inline void Message( const char* txt, size_t size, int callstack )
    {
        assert( size < std::numeric_limits<uint16_t>::max() );
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        if( callstack != 0 )
        {
            tracy::GetProfiler().SendCallstack( callstack );
        }

        auto ptr = (char*)tracy_malloc( size );
        memcpy( ptr, txt, size );

        TracyQueuePrepare( callstack == 0 ? QueueType::Message : QueueType::MessageCallstack );
        MemWrite( &item->messageFat.time, GetTime() );
        MemWrite( &item->messageFat.text, (uint64_t)ptr );
        MemWrite( &item->messageFat.size, (uint16_t)size );
        TracyQueueCommit( messageFatThread );
    }

    static tracy_force_inline void Message( const char* txt, int callstack )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        if( callstack != 0 )
        {
            tracy::GetProfiler().SendCallstack( callstack );
        }

        TracyQueuePrepare( callstack == 0 ? QueueType::MessageLiteral : QueueType::MessageLiteralCallstack );
        MemWrite( &item->messageLiteral.time, GetTime() );
        MemWrite( &item->messageLiteral.text, (uint64_t)txt );
        TracyQueueCommit( messageLiteralThread );
    }

    static tracy_force_inline void MessageColor( const char* txt, size_t size, uint32_t color, int callstack )
    {
        assert( size < std::numeric_limits<uint16_t>::max() );
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        if( callstack != 0 )
        {
            tracy::GetProfiler().SendCallstack( callstack );
        }

        auto ptr = (char*)tracy_malloc( size );
        memcpy( ptr, txt, size );

        TracyQueuePrepare( callstack == 0 ? QueueType::MessageColor : QueueType::MessageColorCallstack );
        MemWrite( &item->messageColorFat.time, GetTime() );
        MemWrite( &item->messageColorFat.text, (uint64_t)ptr );
        MemWrite( &item->messageColorFat.b, uint8_t( ( color       ) & 0xFF ) );
        MemWrite( &item->messageColorFat.g, uint8_t( ( color >> 8  ) & 0xFF ) );
        MemWrite( &item->messageColorFat.r, uint8_t( ( color >> 16 ) & 0xFF ) );
        MemWrite( &item->messageColorFat.size, (uint16_t)size );
        TracyQueueCommit( messageColorFatThread );
    }

    static tracy_force_inline void MessageColor( const char* txt, uint32_t color, int callstack )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        if( callstack != 0 )
        {
            tracy::GetProfiler().SendCallstack( callstack );
        }

        TracyQueuePrepare( callstack == 0 ? QueueType::MessageLiteralColor : QueueType::MessageLiteralColorCallstack );
        MemWrite( &item->messageColorLiteral.time, GetTime() );
        MemWrite( &item->messageColorLiteral.text, (uint64_t)txt );
        MemWrite( &item->messageColorLiteral.b, uint8_t( ( color       ) & 0xFF ) );
        MemWrite( &item->messageColorLiteral.g, uint8_t( ( color >> 8  ) & 0xFF ) );
        MemWrite( &item->messageColorLiteral.r, uint8_t( ( color >> 16 ) & 0xFF ) );
        TracyQueueCommit( messageColorLiteralThread );
    }

    static tracy_force_inline void MessageAppInfo( const char* txt, size_t size )
    {
        assert( size < std::numeric_limits<uint16_t>::max() );
        auto ptr = (char*)tracy_malloc( size );
        memcpy( ptr, txt, size );
        TracyLfqPrepare( QueueType::MessageAppInfo );
        MemWrite( &item->messageFat.time, GetTime() );
        MemWrite( &item->messageFat.text, (uint64_t)ptr );
        MemWrite( &item->messageFat.size, (uint16_t)size );

#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif

        TracyLfqCommit;
    }

    static tracy_force_inline void MemAlloc( const void* ptr, size_t size, bool secure )
    {
        if( secure && !ProfilerAvailable() ) return;
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        const auto thread = GetThreadHandle();

        GetProfiler().m_serialLock.lock();
        SendMemAlloc( QueueType::MemAlloc, thread, ptr, size );
        GetProfiler().m_serialLock.unlock();
    }

    static tracy_force_inline void MemFree( const void* ptr, bool secure )
    {
        if( secure && !ProfilerAvailable() ) return;
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        const auto thread = GetThreadHandle();

        GetProfiler().m_serialLock.lock();
        SendMemFree( QueueType::MemFree, thread, ptr );
        GetProfiler().m_serialLock.unlock();
    }

    static tracy_force_inline void MemAllocCallstack( const void* ptr, size_t size, int depth, bool secure )
    {
        if( secure && !ProfilerAvailable() ) return;
#ifdef TRACY_HAS_CALLSTACK
        auto& profiler = GetProfiler();
#  ifdef TRACY_ON_DEMAND
        if( !profiler.IsConnected() ) return;
#  endif
        const auto thread = GetThreadHandle();

        auto callstack = Callstack( depth );

        profiler.m_serialLock.lock();
        SendCallstackSerial( callstack );
        SendMemAlloc( QueueType::MemAllocCallstack, thread, ptr, size );
        profiler.m_serialLock.unlock();
#else
        static_cast<void>(depth); // unused
        MemAlloc( ptr, size, secure );
#endif
    }

    static tracy_force_inline void MemFreeCallstack( const void* ptr, int depth, bool secure )
    {
        if( secure && !ProfilerAvailable() ) return;
        if( !ProfilerAllocatorAvailable() )
        {
            MemFree( ptr, secure );
            return;
        }
#ifdef TRACY_HAS_CALLSTACK
        auto& profiler = GetProfiler();
#  ifdef TRACY_ON_DEMAND
        if( !profiler.IsConnected() ) return;
#  endif
        const auto thread = GetThreadHandle();

        auto callstack = Callstack( depth );

        profiler.m_serialLock.lock();
        SendCallstackSerial( callstack );
        SendMemFree( QueueType::MemFreeCallstack, thread, ptr );
        profiler.m_serialLock.unlock();
#else
        static_cast<void>(depth); // unused
        MemFree( ptr, secure );
#endif
    }

    static tracy_force_inline void MemAllocNamed( const void* ptr, size_t size, bool secure, const char* name )
    {
        if( secure && !ProfilerAvailable() ) return;
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        const auto thread = GetThreadHandle();

        GetProfiler().m_serialLock.lock();
        SendMemName( name );
        SendMemAlloc( QueueType::MemAllocNamed, thread, ptr, size );
        GetProfiler().m_serialLock.unlock();
    }

    static tracy_force_inline void MemFreeNamed( const void* ptr, bool secure, const char* name )
    {
        if( secure && !ProfilerAvailable() ) return;
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        const auto thread = GetThreadHandle();

        GetProfiler().m_serialLock.lock();
        SendMemName( name );
        SendMemFree( QueueType::MemFreeNamed, thread, ptr );
        GetProfiler().m_serialLock.unlock();
    }

    static tracy_force_inline void MemAllocCallstackNamed( const void* ptr, size_t size, int depth, bool secure, const char* name )
    {
        if( secure && !ProfilerAvailable() ) return;
#ifdef TRACY_HAS_CALLSTACK
        auto& profiler = GetProfiler();
#  ifdef TRACY_ON_DEMAND
        if( !profiler.IsConnected() ) return;
#  endif
        const auto thread = GetThreadHandle();

        auto callstack = Callstack( depth );

        profiler.m_serialLock.lock();
        SendCallstackSerial( callstack );
        SendMemName( name );
        SendMemAlloc( QueueType::MemAllocCallstackNamed, thread, ptr, size );
        profiler.m_serialLock.unlock();
#else
        static_cast<void>(depth); // unused
        static_cast<void>(name); // unused
        MemAlloc( ptr, size, secure );
#endif
    }

    static tracy_force_inline void MemFreeCallstackNamed( const void* ptr, int depth, bool secure, const char* name )
    {
        if( secure && !ProfilerAvailable() ) return;
#ifdef TRACY_HAS_CALLSTACK
        auto& profiler = GetProfiler();
#  ifdef TRACY_ON_DEMAND
        if( !profiler.IsConnected() ) return;
#  endif
        const auto thread = GetThreadHandle();

        auto callstack = Callstack( depth );

        profiler.m_serialLock.lock();
        SendCallstackSerial( callstack );
        SendMemName( name );
        SendMemFree( QueueType::MemFreeCallstackNamed, thread, ptr );
        profiler.m_serialLock.unlock();
#else
        static_cast<void>(depth); // unused
        static_cast<void>(name); // unused
        MemFree( ptr, secure );
#endif
    }

    static tracy_force_inline void SendCallstack( int depth )
    {
#ifdef TRACY_HAS_CALLSTACK
        auto ptr = Callstack( depth );
        TracyQueuePrepare( QueueType::Callstack );
        MemWrite( &item->callstackFat.ptr, (uint64_t)ptr );
        TracyQueueCommit( callstackFatThread );
#else
        static_cast<void>(depth); // unused
#endif
    }

    static tracy_force_inline void ParameterRegister( ParameterCallback cb, void* data )
    {
        auto& profiler = GetProfiler();
        profiler.m_paramCallback = cb;
        profiler.m_paramCallbackData = data;
    }

    static tracy_force_inline void ParameterSetup( uint32_t idx, const char* name, bool isBool, int32_t val )
    {
        TracyLfqPrepare( QueueType::ParamSetup );
        tracy::MemWrite( &item->paramSetup.idx, idx );
        tracy::MemWrite( &item->paramSetup.name, (uint64_t)name );
        tracy::MemWrite( &item->paramSetup.isBool, (uint8_t)isBool );
        tracy::MemWrite( &item->paramSetup.val, val );

#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif

        TracyLfqCommit;
    }

    static tracy_force_inline void SourceCallbackRegister( SourceContentsCallback cb, void* data )
    {
        auto& profiler = GetProfiler();
        profiler.m_sourceCallback = cb;
        profiler.m_sourceCallbackData = data;
    }

#ifdef TRACY_FIBERS
    static tracy_force_inline void EnterFiber( const char* fiber )
    {
        TracyQueuePrepare( QueueType::FiberEnter );
        MemWrite( &item->fiberEnter.time, GetTime() );
        MemWrite( &item->fiberEnter.fiber, (uint64_t)fiber );
        TracyQueueCommit( fiberEnter );
    }

    static tracy_force_inline void LeaveFiber()
    {
        TracyQueuePrepare( QueueType::FiberLeave );
        MemWrite( &item->fiberLeave.time, GetTime() );
        TracyQueueCommit( fiberLeave );
    }
#endif

    void SendCallstack( int depth, const char* skipBefore );
    static void CutCallstack( void* callstack, const char* skipBefore );

    static bool ShouldExit();

    tracy_force_inline bool IsConnected() const
    {
        return m_isConnected.load( std::memory_order_acquire );
    }

#ifdef TRACY_ON_DEMAND
    tracy_force_inline uint64_t ConnectionId() const
    {
        return m_connectionId.load( std::memory_order_acquire );
    }

    tracy_force_inline void DeferItem( const QueueItem& item )
    {
        m_deferredLock.lock();
        auto dst = m_deferredQueue.push_next();
        memcpy( dst, &item, sizeof( item ) );
        m_deferredLock.unlock();
    }
#endif

    void RequestShutdown() { m_shutdown.store( true, std::memory_order_relaxed ); m_shutdownManual.store( true, std::memory_order_relaxed ); }
    bool HasShutdownFinished() const { return m_shutdownFinished.load( std::memory_order_relaxed ); }

    void SendString( uint64_t str, const char* ptr, QueueType type ) { SendString( str, ptr, strlen( ptr ), type ); }
    void SendString( uint64_t str, const char* ptr, size_t len, QueueType type );
    void SendSingleString( const char* ptr ) { SendSingleString( ptr, strlen( ptr ) ); }
    void SendSingleString( const char* ptr, size_t len );
    void SendSecondString( const char* ptr ) { SendSecondString( ptr, strlen( ptr ) ); }
    void SendSecondString( const char* ptr, size_t len );


    // Allocated source location data layout:
    //  2b  payload size
    //  4b  color
    //  4b  source line
    //  fsz function name
    //  1b  null terminator
    //  ssz source file name
    //  1b  null terminator
    //  nsz zone name (optional)

    static tracy_force_inline uint64_t AllocSourceLocation( uint32_t line, const char* source, const char* function )
    {
        return AllocSourceLocation( line, source, function, nullptr, 0 );
    }

    static tracy_force_inline uint64_t AllocSourceLocation( uint32_t line, const char* source, const char* function, const char* name, size_t nameSz )
    {
        return AllocSourceLocation( line, source, strlen(source), function, strlen(function), name, nameSz );
    }

    static tracy_force_inline uint64_t AllocSourceLocation( uint32_t line, const char* source, size_t sourceSz, const char* function, size_t functionSz )
    {
        return AllocSourceLocation( line, source, sourceSz, function, functionSz, nullptr, 0 );
    }

    static tracy_force_inline uint64_t AllocSourceLocation( uint32_t line, const char* source, size_t sourceSz, const char* function, size_t functionSz, const char* name, size_t nameSz )
    {
        const auto sz32 = uint32_t( 2 + 4 + 4 + functionSz + 1 + sourceSz + 1 + nameSz );
        assert( sz32 <= std::numeric_limits<uint16_t>::max() );
        const auto sz = uint16_t( sz32 );
        auto ptr = (char*)tracy_malloc( sz );
        memcpy( ptr, &sz, 2 );
        memset( ptr + 2, 0, 4 );
        memcpy( ptr + 6, &line, 4 );
        memcpy( ptr + 10, function, functionSz );
        ptr[10 + functionSz] = '\0';
        memcpy( ptr + 10 + functionSz + 1, source, sourceSz );
        ptr[10 + functionSz + 1 + sourceSz] = '\0';
        if( nameSz != 0 )
        {
            memcpy( ptr + 10 + functionSz + 1 + sourceSz + 1, name, nameSz );
        }
        return uint64_t( ptr );
    }

private:
    enum class DequeueStatus { DataDequeued, ConnectionLost, QueueEmpty };
    enum class ThreadCtxStatus { Same, Changed, ConnectionLost };

    static void LaunchWorker( void* ptr ) { ((Profiler*)ptr)->Worker(); }
    void Worker();

#ifndef TRACY_NO_FRAME_IMAGE
    static void LaunchCompressWorker( void* ptr ) { ((Profiler*)ptr)->CompressWorker(); }
    void CompressWorker();
#endif

#ifdef TRACY_HAS_CALLSTACK
    static void LaunchSymbolWorker( void* ptr ) { ((Profiler*)ptr)->SymbolWorker(); }
    void SymbolWorker();
    void HandleSymbolQueueItem( const SymbolQueueItem& si );
#endif

    void ClearQueues( tracy::moodycamel::ConsumerToken& token );
    void ClearSerial();
    DequeueStatus Dequeue( tracy::moodycamel::ConsumerToken& token );
    DequeueStatus DequeueContextSwitches( tracy::moodycamel::ConsumerToken& token, int64_t& timeStop );
    DequeueStatus DequeueSerial();
    ThreadCtxStatus ThreadCtxCheck( uint32_t threadId );
    bool CommitData();

    tracy_force_inline bool AppendData( const void* data, size_t len )
    {
        const auto ret = NeedDataSize( len );
        AppendDataUnsafe( data, len );
        return ret;
    }

    tracy_force_inline bool NeedDataSize( size_t len )
    {
        assert( len <= TargetFrameSize );
        bool ret = true;
        if( m_bufferOffset - m_bufferStart + (int)len > TargetFrameSize )
        {
            ret = CommitData();
        }
        return ret;
    }

    tracy_force_inline void AppendDataUnsafe( const void* data, size_t len )
    {
        memcpy( m_buffer + m_bufferOffset, data, len );
        m_bufferOffset += int( len );
    }

    bool SendData( const char* data, size_t len );
    void SendLongString( uint64_t ptr, const char* str, size_t len, QueueType type );
    void SendSourceLocation( uint64_t ptr );
    void SendSourceLocationPayload( uint64_t ptr );
    void SendCallstackPayload( uint64_t ptr );
    void SendCallstackPayload64( uint64_t ptr );
    void SendCallstackAlloc( uint64_t ptr );

    void QueueCallstackFrame( uint64_t ptr );
    void QueueSymbolQuery( uint64_t symbol );
    void QueueExternalName( uint64_t ptr );
    void QueueKernelCode( uint64_t symbol, uint32_t size );
    void QueueSourceCodeQuery( uint32_t id );

    bool HandleServerQuery();
    void HandleDisconnect();
    void HandleParameter( uint64_t payload );
    void HandleSymbolCodeQuery( uint64_t symbol, uint32_t size );
    void HandleSourceCodeQuery( char* data, char* image, uint32_t id );

    void AckServerQuery();
    void AckSymbolCodeNotAvailable();

    void CalibrateTimer();
    void CalibrateDelay();
    void ReportTopology();

    static tracy_force_inline void SendCallstackSerial( void* ptr )
    {
#ifdef TRACY_HAS_CALLSTACK
        auto item = GetProfiler().m_serialQueue.prepare_next();
        MemWrite( &item->hdr.type, QueueType::CallstackSerial );
        MemWrite( &item->callstackFat.ptr, (uint64_t)ptr );
        GetProfiler().m_serialQueue.commit_next();
#else
        static_cast<void>(ptr); // unused
#endif
    }

    static tracy_force_inline void SendMemAlloc( QueueType type, const uint32_t thread, const void* ptr, size_t size )
    {
        assert( type == QueueType::MemAlloc || type == QueueType::MemAllocCallstack || type == QueueType::MemAllocNamed || type == QueueType::MemAllocCallstackNamed );

        auto item = GetProfiler().m_serialQueue.prepare_next();
        MemWrite( &item->hdr.type, type );
        MemWrite( &item->memAlloc.time, GetTime() );
        MemWrite( &item->memAlloc.thread, thread );
        MemWrite( &item->memAlloc.ptr, (uint64_t)ptr );
        if( compile_time_condition<sizeof( size ) == 4>::value )
        {
            memcpy( &item->memAlloc.size, &size, 4 );
            memset( &item->memAlloc.size + 4, 0, 2 );
        }
        else
        {
            assert( sizeof( size ) == 8 );
            memcpy( &item->memAlloc.size, &size, 4 );
            memcpy( ((char*)&item->memAlloc.size)+4, ((char*)&size)+4, 2 );
        }
        GetProfiler().m_serialQueue.commit_next();
    }

    static tracy_force_inline void SendMemFree( QueueType type, const uint32_t thread, const void* ptr )
    {
        assert( type == QueueType::MemFree || type == QueueType::MemFreeCallstack || type == QueueType::MemFreeNamed || type == QueueType::MemFreeCallstackNamed );

        auto item = GetProfiler().m_serialQueue.prepare_next();
        MemWrite( &item->hdr.type, type );
        MemWrite( &item->memFree.time, GetTime() );
        MemWrite( &item->memFree.thread, thread );
        MemWrite( &item->memFree.ptr, (uint64_t)ptr );
        GetProfiler().m_serialQueue.commit_next();
    }

    static tracy_force_inline void SendMemName( const char* name )
    {
        assert( name );
        auto item = GetProfiler().m_serialQueue.prepare_next();
        MemWrite( &item->hdr.type, QueueType::MemNamePayload );
        MemWrite( &item->memName.name, (uint64_t)name );
        GetProfiler().m_serialQueue.commit_next();
    }

#if defined _WIN32 && defined TRACY_TIMER_QPC
    static int64_t GetTimeQpc();
#endif

    double m_timerMul;
    uint64_t m_resolution;
    uint64_t m_delay;
    std::atomic<int64_t> m_timeBegin;
    uint32_t m_mainThread;
    uint64_t m_epoch, m_exectime;
    std::atomic<bool> m_shutdown;
    std::atomic<bool> m_shutdownManual;
    std::atomic<bool> m_shutdownFinished;
    Socket* m_sock;
    UdpBroadcast* m_broadcast;
    bool m_noExit;
    uint32_t m_userPort;
    std::atomic<uint32_t> m_zoneId;
    int64_t m_samplingPeriod;

    uint32_t m_threadCtx;
    int64_t m_refTimeThread;
    int64_t m_refTimeSerial;
    int64_t m_refTimeCtx;
    int64_t m_refTimeGpu;

    void* m_stream;     // LZ4_stream_t*
    char* m_buffer;
    int m_bufferOffset;
    int m_bufferStart;

    char* m_lz4Buf;

    FastVector<QueueItem> m_serialQueue, m_serialDequeue;
    TracyMutex m_serialLock;

#ifndef TRACY_NO_FRAME_IMAGE
    FastVector<FrameImageQueueItem> m_fiQueue, m_fiDequeue;
    TracyMutex m_fiLock;
#endif

    SPSCQueue<SymbolQueueItem> m_symbolQueue;

    std::atomic<uint64_t> m_frameCount;
    std::atomic<bool> m_isConnected;
#ifdef TRACY_ON_DEMAND
    std::atomic<uint64_t> m_connectionId;

    TracyMutex m_deferredLock;
    FastVector<QueueItem> m_deferredQueue;
#endif

#ifdef TRACY_HAS_SYSTIME
    void ProcessSysTime();

    SysTime m_sysTime;
    uint64_t m_sysTimeLast = 0;
#else
    void ProcessSysTime() {}
#endif

    ParameterCallback m_paramCallback;
    void* m_paramCallbackData;
    SourceContentsCallback m_sourceCallback;
    void* m_sourceCallbackData;

    char* m_queryImage;
    char* m_queryData;
    char* m_queryDataPtr;

#if defined _WIN32
    void* m_exceptionHandler;
#endif
#ifdef __linux__
    struct {
        struct sigaction pwr, ill, fpe, segv, pipe, bus, abrt;
    } m_prevSignal;
#endif
    bool m_crashHandlerInstalled;
};

}

#endif
