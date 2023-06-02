#ifndef __TRACYLOCK_HPP__
#define __TRACYLOCK_HPP__

#include <atomic>
#include <limits>

#include "../common/TracySystem.hpp"
#include "../common/TracyAlign.hpp"
#include "TracyProfiler.hpp"

namespace tracy
{

class LockableCtx
{
public:
    tracy_force_inline LockableCtx( const SourceLocationData* srcloc )
        : m_id( GetLockCounter().fetch_add( 1, std::memory_order_relaxed ) )
#ifdef TRACY_ON_DEMAND
        , m_lockCount( 0 )
        , m_active( false )
#endif
    {
        assert( m_id != std::numeric_limits<uint32_t>::max() );

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockAnnounce );
        MemWrite( &item->lockAnnounce.id, m_id );
        MemWrite( &item->lockAnnounce.time, Profiler::GetTime() );
        MemWrite( &item->lockAnnounce.lckloc, (uint64_t)srcloc );
        MemWrite( &item->lockAnnounce.type, LockType::Lockable );
#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif
        Profiler::QueueSerialFinish();
    }

    LockableCtx( const LockableCtx& ) = delete;
    LockableCtx& operator=( const LockableCtx& ) = delete;

    tracy_force_inline ~LockableCtx()
    {
        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockTerminate );
        MemWrite( &item->lockTerminate.id, m_id );
        MemWrite( &item->lockTerminate.time, Profiler::GetTime() );
#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline bool BeforeLock()
    {
#ifdef TRACY_ON_DEMAND
        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return false;
#endif

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockWait );
        MemWrite( &item->lockWait.thread, GetThreadHandle() );
        MemWrite( &item->lockWait.id, m_id );
        MemWrite( &item->lockWait.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
        return true;
    }

    tracy_force_inline void AfterLock()
    {
        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockObtain );
        MemWrite( &item->lockObtain.thread, GetThreadHandle() );
        MemWrite( &item->lockObtain.id, m_id );
        MemWrite( &item->lockObtain.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline void AfterUnlock()
    {
#ifdef TRACY_ON_DEMAND
        m_lockCount.fetch_sub( 1, std::memory_order_relaxed );
        if( !m_active.load( std::memory_order_relaxed ) ) return;
        if( !GetProfiler().IsConnected() )
        {
            m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockRelease );
        MemWrite( &item->lockRelease.id, m_id );
        MemWrite( &item->lockRelease.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline void AfterTryLock( bool acquired )
    {
#ifdef TRACY_ON_DEMAND
        if( !acquired ) return;

        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return;
#endif

        if( acquired )
        {
            auto item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::LockObtain );
            MemWrite( &item->lockObtain.thread, GetThreadHandle() );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            Profiler::QueueSerialFinish();
        }
    }

    tracy_force_inline void Mark( const SourceLocationData* srcloc )
    {
#ifdef TRACY_ON_DEMAND
        const auto active = m_active.load( std::memory_order_relaxed );
        if( !active ) return;
        const auto connected = GetProfiler().IsConnected();
        if( !connected )
        {
            if( active ) m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockMark );
        MemWrite( &item->lockMark.thread, GetThreadHandle() );
        MemWrite( &item->lockMark.id, m_id );
        MemWrite( &item->lockMark.srcloc, (uint64_t)srcloc );
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline void CustomName( const char* name, size_t size )
    {
        assert( size < std::numeric_limits<uint16_t>::max() );
        auto ptr = (char*)tracy_malloc( size );
        memcpy( ptr, name, size );
        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockName );
        MemWrite( &item->lockNameFat.id, m_id );
        MemWrite( &item->lockNameFat.name, (uint64_t)ptr );
        MemWrite( &item->lockNameFat.size, (uint16_t)size );
#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif
        Profiler::QueueSerialFinish();
    }

private:
    uint32_t m_id;

#ifdef TRACY_ON_DEMAND
    std::atomic<uint32_t> m_lockCount;
    std::atomic<bool> m_active;
#endif
};

template<class T>
class Lockable
{
public:
    tracy_force_inline Lockable( const SourceLocationData* srcloc )
        : m_ctx( srcloc )
    {
    }

    Lockable( const Lockable& ) = delete;
    Lockable& operator=( const Lockable& ) = delete;

    tracy_force_inline void lock()
    {
        const auto runAfter = m_ctx.BeforeLock();
        m_lockable.lock();
        if( runAfter ) m_ctx.AfterLock();
    }

    tracy_force_inline void unlock()
    {
        m_lockable.unlock();
        m_ctx.AfterUnlock();
    }

    tracy_force_inline bool try_lock()
    {
        const auto acquired = m_lockable.try_lock();
        m_ctx.AfterTryLock( acquired );
        return acquired;
    }

    tracy_force_inline void Mark( const SourceLocationData* srcloc )
    {
        m_ctx.Mark( srcloc );
    }

    tracy_force_inline void CustomName( const char* name, size_t size )
    {
        m_ctx.CustomName( name, size );
    }

private:
    T m_lockable;
    LockableCtx m_ctx;
};


class SharedLockableCtx
{
public:
    tracy_force_inline SharedLockableCtx( const SourceLocationData* srcloc )
        : m_id( GetLockCounter().fetch_add( 1, std::memory_order_relaxed ) )
#ifdef TRACY_ON_DEMAND
        , m_lockCount( 0 )
        , m_active( false )
#endif
    {
        assert( m_id != std::numeric_limits<uint32_t>::max() );

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockAnnounce );
        MemWrite( &item->lockAnnounce.id, m_id );
        MemWrite( &item->lockAnnounce.time, Profiler::GetTime() );
        MemWrite( &item->lockAnnounce.lckloc, (uint64_t)srcloc );
        MemWrite( &item->lockAnnounce.type, LockType::SharedLockable );
#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif
        Profiler::QueueSerialFinish();
    }

    SharedLockableCtx( const SharedLockableCtx& ) = delete;
    SharedLockableCtx& operator=( const SharedLockableCtx& ) = delete;

    tracy_force_inline ~SharedLockableCtx()
    {
        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockTerminate );
        MemWrite( &item->lockTerminate.id, m_id );
        MemWrite( &item->lockTerminate.time, Profiler::GetTime() );
#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline bool BeforeLock()
    {
#ifdef TRACY_ON_DEMAND
        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return false;
#endif

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockWait );
        MemWrite( &item->lockWait.thread, GetThreadHandle() );
        MemWrite( &item->lockWait.id, m_id );
        MemWrite( &item->lockWait.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
        return true;
    }

    tracy_force_inline void AfterLock()
    {
        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockObtain );
        MemWrite( &item->lockObtain.thread, GetThreadHandle() );
        MemWrite( &item->lockObtain.id, m_id );
        MemWrite( &item->lockObtain.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline void AfterUnlock()
    {
#ifdef TRACY_ON_DEMAND
        m_lockCount.fetch_sub( 1, std::memory_order_relaxed );
        if( !m_active.load( std::memory_order_relaxed ) ) return;
        if( !GetProfiler().IsConnected() )
        {
            m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockRelease );
        MemWrite( &item->lockRelease.id, m_id );
        MemWrite( &item->lockRelease.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline void AfterTryLock( bool acquired )
    {
#ifdef TRACY_ON_DEMAND
        if( !acquired ) return;

        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return;
#endif

        if( acquired )
        {
            auto item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::LockObtain );
            MemWrite( &item->lockObtain.thread, GetThreadHandle() );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            Profiler::QueueSerialFinish();
        }
    }

    tracy_force_inline bool BeforeLockShared()
    {
#ifdef TRACY_ON_DEMAND
        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return false;
#endif

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockSharedWait );
        MemWrite( &item->lockWait.thread, GetThreadHandle() );
        MemWrite( &item->lockWait.id, m_id );
        MemWrite( &item->lockWait.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
        return true;
    }

    tracy_force_inline void AfterLockShared()
    {
        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockSharedObtain );
        MemWrite( &item->lockObtain.thread, GetThreadHandle() );
        MemWrite( &item->lockObtain.id, m_id );
        MemWrite( &item->lockObtain.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline void AfterUnlockShared()
    {
#ifdef TRACY_ON_DEMAND
        m_lockCount.fetch_sub( 1, std::memory_order_relaxed );
        if( !m_active.load( std::memory_order_relaxed ) ) return;
        if( !GetProfiler().IsConnected() )
        {
            m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockSharedRelease );
        MemWrite( &item->lockReleaseShared.thread, GetThreadHandle() );
        MemWrite( &item->lockReleaseShared.id, m_id );
        MemWrite( &item->lockReleaseShared.time, Profiler::GetTime() );
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline void AfterTryLockShared( bool acquired )
    {
#ifdef TRACY_ON_DEMAND
        if( !acquired ) return;

        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return;
#endif

        if( acquired )
        {
            auto item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::LockSharedObtain );
            MemWrite( &item->lockObtain.thread, GetThreadHandle() );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            Profiler::QueueSerialFinish();
        }
    }

    tracy_force_inline void Mark( const SourceLocationData* srcloc )
    {
#ifdef TRACY_ON_DEMAND
        const auto active = m_active.load( std::memory_order_relaxed );
        if( !active ) return;
        const auto connected = GetProfiler().IsConnected();
        if( !connected )
        {
            if( active ) m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockMark );
        MemWrite( &item->lockMark.thread, GetThreadHandle() );
        MemWrite( &item->lockMark.id, m_id );
        MemWrite( &item->lockMark.srcloc, (uint64_t)srcloc );
        Profiler::QueueSerialFinish();
    }

    tracy_force_inline void CustomName( const char* name, size_t size )
    {
        assert( size < std::numeric_limits<uint16_t>::max() );
        auto ptr = (char*)tracy_malloc( size );
        memcpy( ptr, name, size );
        auto item = Profiler::QueueSerial();
        MemWrite( &item->hdr.type, QueueType::LockName );
        MemWrite( &item->lockNameFat.id, m_id );
        MemWrite( &item->lockNameFat.name, (uint64_t)ptr );
        MemWrite( &item->lockNameFat.size, (uint16_t)size );
#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif
        Profiler::QueueSerialFinish();
    }

private:
    uint32_t m_id;

#ifdef TRACY_ON_DEMAND
    std::atomic<uint32_t> m_lockCount;
    std::atomic<bool> m_active;
#endif
};

template<class T>
class SharedLockable
{
public:
    tracy_force_inline SharedLockable( const SourceLocationData* srcloc )
        : m_ctx( srcloc )
    {
    }

    SharedLockable( const SharedLockable& ) = delete;
    SharedLockable& operator=( const SharedLockable& ) = delete;

    tracy_force_inline void lock()
    {
        const auto runAfter = m_ctx.BeforeLock();
        m_lockable.lock();
        if( runAfter ) m_ctx.AfterLock();
    }

    tracy_force_inline void unlock()
    {
        m_lockable.unlock();
        m_ctx.AfterUnlock();
    }

    tracy_force_inline bool try_lock()
    {
        const auto acquired = m_lockable.try_lock();
        m_ctx.AfterTryLock( acquired );
        return acquired;
    }

    tracy_force_inline void lock_shared()
    {
        const auto runAfter = m_ctx.BeforeLockShared();
        m_lockable.lock_shared();
        if( runAfter ) m_ctx.AfterLockShared();
    }

    tracy_force_inline void unlock_shared()
    {
        m_lockable.unlock_shared();
        m_ctx.AfterUnlockShared();
    }

    tracy_force_inline bool try_lock_shared()
    {
        const auto acquired = m_lockable.try_lock_shared();
        m_ctx.AfterTryLockShared( acquired );
        return acquired;
    }

    tracy_force_inline void Mark( const SourceLocationData* srcloc )
    {
        m_ctx.Mark( srcloc );
    }

    tracy_force_inline void CustomName( const char* name, size_t size )
    {
        m_ctx.CustomName( name, size );
    }

private:
    T m_lockable;
    SharedLockableCtx m_ctx;
};


}

#endif
