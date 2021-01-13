// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../Remoting/Monarch.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using namespace winrt;
using namespace winrt::Microsoft::Terminal;

// These are some gross macros that let us call a private ctor for
// Monarch/Peasant. We can't just use make_self, because that doesn't let us
// call a private ctor. We can use com_ptr::attach, but since we're allocating
// the thing on the stack, we need to make sure to call detach before the object
// is destructed.

#define MAKE_MONARCH(name, pid)                               \
    Remoting::implementation::Monarch _local_##name##{ pid }; \
    com_ptr<Remoting::implementation::Monarch> name;          \
    name.attach(&_local_##name##);                            \
    auto cleanup_##name## = wil::scope_exit([&]() { name.detach(); });

#define MAKE_PEASANT(name, pid)                               \
    Remoting::implementation::Peasant _local_##name##{ pid }; \
    com_ptr<Remoting::implementation::Peasant> name;          \
    name.attach(&_local_##name##);                            \
    auto cleanup_##name## = wil::scope_exit([&]() { name.detach(); });

namespace RemotingUnitTests
{
    class RemotingTests
    {
        BEGIN_TEST_CLASS(RemotingTests)
        END_TEST_CLASS()

        TEST_METHOD(CreateMonarch);
        TEST_METHOD(CreatePeasant);

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }
    };

    void RemotingTests::CreateMonarch()
    {
        auto m1 = winrt::make_self<Remoting::implementation::Monarch>();
        VERIFY_IS_NOT_NULL(m1);
        VERIFY_ARE_EQUAL(GetCurrentProcessId(),
                         m1->GetPID(),
                         L"A Monarch without an explicit PID should use the current PID");

        Log::Comment(L"That's what we need for window process management, but for tests, it'll be more useful to fake the PIDs.");

        auto expectedFakePID = 1234u;
        MAKE_MONARCH(m2, expectedFakePID);

        VERIFY_IS_NOT_NULL(m2);
        VERIFY_ARE_EQUAL(expectedFakePID,
                         m2->GetPID(),
                         L"A Monarch with an explicit PID should use the one we provided");
    }

    void RemotingTests::CreatePeasant()
    {
        auto p1 = winrt::make_self<Remoting::implementation::Peasant>();
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_ARE_EQUAL(GetCurrentProcessId(),
                         p1->GetPID(),
                         L"A Peasant without an explicit PID should use the current PID");

        Log::Comment(L"That's what we need for window process management, but for tests, it'll be more useful to fake the PIDs.");

        auto expectedFakePID = 2345u;
        MAKE_PEASANT(p2, expectedFakePID);

        VERIFY_IS_NOT_NULL(p2);
        VERIFY_ARE_EQUAL(expectedFakePID,
                         p2->GetPID(),
                         L"A Peasant with an explicit PID should use the one we provided");
    }

}
