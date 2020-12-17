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

#define MAKE_MONARCH(name, pid)                               \
    Remoting::implementation::Monarch _local_##name##{ pid }; \
    com_ptr<Remoting::implementation::Monarch> name;          \
    name.attach(&_local_##name##);                            \
    auto cleanup = wil::scope_exit([&]() { name.detach(); });

namespace RemotingLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class RemotingTests
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(RemotingTests)
            // TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            // TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(CreateMonarch);

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

}
