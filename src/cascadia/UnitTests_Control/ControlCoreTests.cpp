// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalControl/EventArgs.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using namespace winrt;
using namespace winrt::Microsoft::Terminal;

namespace ControlUnitTests
{
    class ControlCoreTests
    {
        BEGIN_TEST_CLASS(ControlCoreTests)
        END_TEST_CLASS()

        TEST_METHOD(PlaceholderTest);
    };

    void ControlCoreTests::PlaceholderTest()
    {
        Log::Comment(L"This test is a placeholder while the rest of this test library is being authored.");
        VERIFY_IS_TRUE(true);
    }

}
