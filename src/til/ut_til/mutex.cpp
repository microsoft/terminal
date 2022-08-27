// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/mutex.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class MutexTests
{
    BEGIN_TEST_CLASS(MutexTests)
        TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:10") // 10s timeout
    END_TEST_CLASS()

    TEST_METHOD(Basic)
    {
        struct TestData
        {
            int foo;
            int bar;
        };

        const til::shared_mutex<TestData> mutex{ TestData{ 1, 2 } };

        {
            auto lock = mutex.lock();
            *lock = TestData{ 3, 4 };
            lock->foo = 5;
        }

        {
            auto lock1 = mutex.lock_shared();
            auto lock2 = mutex.lock_shared();

            VERIFY_ARE_EQUAL(5, lock1->foo);
            VERIFY_ARE_EQUAL(4, lock2->bar);
        }

        // This is here just to ensure that the prior
        // .lock_shared() properly unlocked the mutex.
        auto lock = mutex.lock();
    }
};
