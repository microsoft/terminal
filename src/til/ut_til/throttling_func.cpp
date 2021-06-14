// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/throttled_func.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

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

        while (true)
        {
            auto value = counter.load(std::memory_order_relaxed);
            if (value == 2)
            {
                break;
            }

            WaitOnAddress(&counter, &value, sizeof(value), INFINITE);
        }

        VERIFY_IS_TRUE(true);
    }
};
