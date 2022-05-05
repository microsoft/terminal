// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

//
// The code in this file was adapted from the STL on the 2021-07-05. Commit:
// https://github.com/microsoft/STL/blob/e745bad3b1d05b5b19ec652d68abb37865ffa454/stl/src/atomic_wait.cpp
//
// It backports the following Windows 8 functions to Windows 7:
// * WaitOnAddress
// * WakeByAddressSingle
// * WakeByAddressAll
//

#include <cstdint>
#include <new>

#include <winsdkver.h>
#define _WIN32_WINNT 0x0601
#include <sdkddkver.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>

namespace
{
    class [[nodiscard]] SRWLockGuard
    {
    public:
        explicit SRWLockGuard(SRWLOCK& lock) noexcept :
            _lock(&lock)
        {
            AcquireSRWLockExclusive(_lock);
        }

        ~SRWLockGuard()
        {
            ReleaseSRWLockExclusive(_lock);
        }

        SRWLockGuard(const SRWLockGuard&) = delete;
        SRWLockGuard& operator=(const SRWLockGuard&) = delete;

        SRWLockGuard(SRWLockGuard&&) = delete;
        SRWLockGuard& operator=(SRWLockGuard&&) = delete;

    private:
        SRWLOCK* _lock;
    };

    struct WaitContext
    {
        const volatile void* address;
        WaitContext* next;
        WaitContext* prev;
        CONDITION_VARIABLE cv;
    };

    struct [[nodiscard]] GuardedWaitContext : WaitContext
    {
        GuardedWaitContext(const volatile void* storage, WaitContext* head) noexcept :
            WaitContext{ storage, head, head->prev, CONDITION_VARIABLE_INIT }
        {
            prev->next = this;
            next->prev = this;
        }

        ~GuardedWaitContext()
        {
            const auto n = next;
            const auto p = prev;
            next->prev = p;
            prev->next = n;
        }

        GuardedWaitContext(const GuardedWaitContext&) = delete;
        GuardedWaitContext& operator=(const GuardedWaitContext&) = delete;

        GuardedWaitContext(GuardedWaitContext&&) = delete;
        GuardedWaitContext& operator=(GuardedWaitContext&&) = delete;
    };

#pragma warning(push)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
    struct alignas(std::hardware_destructive_interference_size) WaitTableEntry
    {
        SRWLOCK lock = SRWLOCK_INIT;
        WaitContext head = { nullptr, &head, &head, CONDITION_VARIABLE_INIT };
    };
#pragma warning(pop)

    [[nodiscard]] WaitTableEntry& GetWaitTableEntry(const volatile void* const storage) noexcept
    {
        // A prime number for the hash table size was chosen to prevent collisions.
        constexpr size_t size = 251;
        constexpr std::hash<uintptr_t> hasher;

        static WaitTableEntry table[size];
#pragma warning(suppress : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator
#pragma warning(suppress : 26482) // Only index into arrays using constant expressions
#pragma warning(suppress : 26490) // Don't use reinterpret_cast
        return table[hasher(reinterpret_cast<uintptr_t>(storage)) % size];
    }

#pragma warning(suppress : 26429) // Symbol 'comparand' is never tested for nullness, it can be marked as not_null
    bool AreEqual(const volatile void* storage, const void* comparand, size_t size) noexcept
    {
        switch (size)
        {
        case 1:
            return __iso_volatile_load8(static_cast<const volatile __int8*>(storage)) == *static_cast<const __int8*>(comparand);
        case 2:
            return __iso_volatile_load16(static_cast<const volatile __int16*>(storage)) == *static_cast<const __int16*>(comparand);
        case 4:
            return __iso_volatile_load32(static_cast<const volatile __int32*>(storage)) == *static_cast<const __int32*>(comparand);
        case 8:
            return __iso_volatile_load64(static_cast<const volatile __int64*>(storage)) == *static_cast<const __int64*>(comparand);
        default:
            abort();
        }
    }
} // unnamed namespace

extern "C" BOOL WINAPI WaitOnAddress(_In_reads_bytes_(AddressSize) volatile VOID* Address, _In_reads_bytes_(AddressSize) PVOID CompareAddress, _In_ SIZE_T AddressSize, _In_opt_ DWORD dwMilliseconds)
{
    auto& entry = GetWaitTableEntry(Address);

    SRWLockGuard guard{ entry.lock };
    GuardedWaitContext context{ Address, &entry.head };

    for (;;)
    {
        // NOTE: under lock to prevent lost wakes
        if (!AreEqual(Address, CompareAddress, AddressSize))
        {
            return TRUE;
        }

        if (!SleepConditionVariableSRW(&context.cv, &entry.lock, dwMilliseconds, 0))
        {
#ifndef NDEBUG
            if (GetLastError() != ERROR_TIMEOUT)
            {
                abort();
            }
#endif
            return FALSE;
        }

        if (dwMilliseconds != INFINITE)
        {
            // spurious wake to recheck the clock
            return TRUE;
        }
    }
}

extern "C" VOID WINAPI WakeByAddressSingle(_In_ PVOID Address)
{
    auto& entry = GetWaitTableEntry(Address);
    SRWLockGuard guard(entry.lock);

    for (auto context = entry.head.next; context != &entry.head; context = context->next)
    {
        if (context->address == Address)
        {
            // Can't move wake outside SRWLOCKed section: SRWLOCK also protects the context itself
            WakeAllConditionVariable(&context->cv);
            // This break; is the difference between WakeByAddressSingle and WakeByAddressAll
            break;
        }
    }
}

extern "C" VOID WINAPI WakeByAddressAll(_In_ PVOID Address)
{
    auto& entry = GetWaitTableEntry(Address);
    SRWLockGuard guard(entry.lock);

    for (auto context = entry.head.next; context != &entry.head; context = context->next)
    {
        if (context->address == Address)
        {
            // Can't move wake outside SRWLOCKed section: SRWLOCK also protects the context itself
            WakeAllConditionVariable(&context->cv);
        }
    }
}
