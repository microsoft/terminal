// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/latch.h"
#include "til/throttled_func.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class ThrottledFuncTests
{
    BEGIN_TEST_CLASS(ThrottledFuncTests)
        TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:10") // 10s timeout
    END_TEST_CLASS()

    TEST_METHOD(Basic)
    {
        using namespace std::chrono_literals;
        using throttled_func = til::throttled_func_trailing<bool>;

        til::latch latch{ 2 };

        std::unique_ptr<throttled_func> tf;
        tf = std::make_unique<throttled_func>(10ms, [&](bool reschedule) {
            latch.count_down();

            // This will ensure that the callback is called even if we
            // invoke the throttled_func from inside the callback itself.
            if (reschedule)
            {
                tf->operator()(false);
            }
        });
        // This will ensure that the throttled_func invokes the callback in general.
        tf->operator()(true);

        latch.wait();
    }
};
