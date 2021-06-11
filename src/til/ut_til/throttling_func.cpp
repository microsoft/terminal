// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/throttled_func.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

template<typename T>
static void wait_until(std::atomic<T>& atomic, T goal, std::memory_order order = std::memory_order_seq_cst)
{
    static_assert(sizeof(atomic) == sizeof(goal));

    while (true)
    {
        auto value = atomic.load(order);
        if (value == goal)
        {
            break;
        }

        WaitOnAddress(&atomic, &value, sizeof(value), INFINITE);
    }
}

class ThrottlingFuncTests
{
    BEGIN_TEST_CLASS(ThrottlingFuncTests)
        TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:10") // 10s timeout
    END_TEST_CLASS()

    TEST_METHOD(Basic)
    {
        using namespace std::chrono_literals;
        using throttled_func = til::throttled_func_trailing<bool>;

        std::atomic<int> counter;

        std::unique_ptr<throttled_func> tf;
        tf = std::make_unique<throttled_func>(10ms, [&](bool reschedule) {
            counter.fetch_add(1, std::memory_order_relaxed);
            WakeByAddressAll(&counter);

            if (reschedule)
            {
                tf->operator()(false);
            }
        });
        tf->operator()(true);

        wait_until(counter, 2, std::memory_order_relaxed);
        VERIFY_IS_TRUE(true);
    }
};
