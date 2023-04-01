// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "atomic.h"

namespace til
{
    // ticket_lock implements a classic fair lock.
    //
    // Compared to a SRWLOCK this implementation is significantly more unsafe to use:
    // Forgetting to call unlock or calling unlock more than once, will lead to deadlocks,
    // as _now_serving will remain out of sync with _next_ticket and prevent any further lockings.
    //
    // I recommend to use the following with this class:
    // * A low number of concurrent accesses (this lock doesn't scale well beyond 2 threads)
    // * alignas(std::hardware_destructive_interference_size) to prevent false sharing
    // * std::unique_lock or std::scoped_lock to prevent unbalanced lock/unlock calls
    struct ticket_lock
    {
        void lock() noexcept
        {
            const auto ticket = _next_ticket.fetch_add(1, std::memory_order_relaxed);

            for (;;)
            {
                const auto current = _now_serving.load(std::memory_order_acquire);
                if (current == ticket)
                {
                    break;
                }

                til::atomic_wait(_now_serving, current);
            }
        }

        void unlock() noexcept
        {
            _now_serving.fetch_add(1, std::memory_order_release);
            til::atomic_notify_all(_now_serving);
        }

    private:
        // You may be inclined to add alignas(std::hardware_destructive_interference_size)
        // here to force the two atomics on separate cache lines, but I suggest to carefully
        // benchmark such a change. Since this ticket_lock is primarily used to synchronize
        // exactly 2 threads, it actually helps us that these atomic are on the same cache line
        // as any change by one thread is flushed to the other, which will then read it anyways.
        //
        // Integer overflow doesn't break the algorithm, as these two
        // atomics are treated more like "IDs" and less like counters.
        std::atomic<uint32_t> _next_ticket{ 0 };
        std::atomic<uint32_t> _now_serving{ 0 };
    };

    struct recursive_ticket_lock
    {
        struct recursive_ticket_lock_suspension
        {
            constexpr recursive_ticket_lock_suspension(recursive_ticket_lock& lock, uint32_t owner, uint32_t recursion) noexcept :
                _lock{ lock },
                _owner{ owner },
                _recursion{ recursion }
            {
            }

            // When this class is destroyed it restores the recursive_ticket_lock state.
            // This of course only works if the lock wasn't moved to another thread or something.
            recursive_ticket_lock_suspension(const recursive_ticket_lock_suspension&) = delete;
            recursive_ticket_lock_suspension& operator=(const recursive_ticket_lock_suspension&) = delete;
            recursive_ticket_lock_suspension(recursive_ticket_lock_suspension&&) = delete;
            recursive_ticket_lock_suspension& operator=(recursive_ticket_lock_suspension&&) = delete;

            ~recursive_ticket_lock_suspension()
            {
                if (_owner)
                {
                    // If someone reacquired the lock on the current thread, we shouldn't lock it again.
                    if (_lock._owner.load(std::memory_order_relaxed) != _owner)
                    {
                        _lock._lock.lock(); // lock-lock-lock lol
                        _lock._owner.store(_owner, std::memory_order_relaxed);
                    }
                    // ...but we should restore the original recursion count.
                    _lock._recursion += _recursion;
                }
            }

        private:
            friend struct recursive_ticket_lock;

            recursive_ticket_lock& _lock;
            uint32_t _owner = 0;
            uint32_t _recursion = 0;
        };

        void lock() noexcept
        {
            const auto id = GetCurrentThreadId();

            if (_owner.load(std::memory_order_relaxed) != id)
            {
                _lock.lock();
                _owner.store(id, std::memory_order_relaxed);
            }

            _recursion++;
        }

        void unlock() noexcept
        {
            if (--_recursion == 0)
            {
                _owner.store(0, std::memory_order_relaxed);
                _lock.unlock();
            }
        }

        [[nodiscard]] recursive_ticket_lock_suspension suspend() noexcept
        {
            const auto id = GetCurrentThreadId();
            uint32_t owner = 0;
            uint32_t recursion = 0;

            if (_owner.load(std::memory_order_relaxed) == id)
            {
                owner = id;
                recursion = _recursion;
                _owner.store(0, std::memory_order_relaxed);
                _recursion = 0;
                _lock.unlock();
            }

            return { *this, owner, recursion };
        }

        uint32_t is_locked() const noexcept
        {
            const auto id = GetCurrentThreadId();
            return _owner.load(std::memory_order_relaxed) == id;
        }

        uint32_t recursion_depth() const noexcept
        {
            return is_locked() ? _recursion : 0;
        }

    private:
        ticket_lock _lock;
        std::atomic<uint32_t> _owner = 0;
        uint32_t _recursion = 0;
    };

    using recursive_ticket_lock_suspension = recursive_ticket_lock::recursive_ticket_lock_suspension;
}
