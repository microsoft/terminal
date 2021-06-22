// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef __cpp_lib_latch
#include <latch>
#endif

namespace til
{
#ifdef __cpp_lib_latch
    using latch = std::latch;
#else
    class latch
    {
    public:
        [[nodiscard]] static constexpr ptrdiff_t max() noexcept
        {
            return std::numeric_limits<ptrdiff_t>::max();
        }

        constexpr explicit latch(const ptrdiff_t expected) noexcept :
            counter{ expected }
        {
            assert(expected >= 0);
        }

        latch(const latch&) = delete;
        latch& operator=(const latch&) = delete;

        void count_down(const ptrdiff_t n = 1) noexcept
        {
            assert(n >= 0);

            const auto old = counter.fetch_sub(n, std::memory_order_release);
            if (old == n)
            {
                WakeByAddressAll(&counter);
                return;
            }

            assert(old > n);
        }

        [[nodiscard]] bool try_wait() const noexcept
        {
            return counter.load(std::memory_order_acquire) == 0;
        }

        void wait() const noexcept
        {
            while (true)
            {
                auto current = counter.load(std::memory_order_acquire);
                if (current == 0)
                {
                    return;
                }

                assert(current > 0);
                WaitOnAddress(const_cast<decltype(counter)*>(&counter), &current, sizeof(counter), INFINITE);
            }
        }

        void arrive_and_wait(const ptrdiff_t n = 1) noexcept
        {
            assert(n >= 0);

            auto old = counter.fetch_sub(n, std::memory_order_acq_rel);
            if (old == n)
            {
                WakeByAddressAll(&counter);
                return;
            }

            assert(old > n);
            WaitOnAddress(&counter, &old, sizeof(counter), INFINITE);
            wait();
        }

    private:
        std::atomic<ptrdiff_t> counter;
    };
#endif
}
