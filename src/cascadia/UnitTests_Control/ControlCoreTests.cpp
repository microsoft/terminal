// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalControl/EventArgs.h"
#include "MockControlSettings.h"

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

        TEST_METHOD(OnStackSettings);
        TEST_METHOD(ComPtrSettings);
    };

    void ControlCoreTests::OnStackSettings()
    {
        Log::Comment(L"Just make sure we can instantiate a settings obj on the stack");

        MockControlSettings settings;

        Log::Comment(L"Verify literally any setting, it doesn't matter");
        VERIFY_ARE_EQUAL(DEFAULT_FOREGROUND, settings.DefaultForeground());
    }
    void ControlCoreTests::ComPtrSettings()
    {
        Log::Comment(L"Just make sure we can instantiate a settings obj in a com_ptr");
        winrt::com_ptr<MockControlSettings> settings;
        settings.attach(new MockControlSettings());

        Log::Comment(L"Verify literally any setting, it doesn't matter");
        VERIFY_ARE_EQUAL(DEFAULT_FOREGROUND, settings->DefaultForeground());
    }

}
