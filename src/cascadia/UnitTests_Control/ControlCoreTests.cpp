// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalControl/EventArgs.h"
#include "../TerminalControl/ControlCore.h"
#include "MockControlSettings.h"
#include "MockConnection.h"

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
        TEST_METHOD(InstantiateCore);

        TEST_CLASS_SETUP(ModuleSetup)
        {
            winrt::init_apartment(winrt::apartment_type::single_threaded);

            return true;
        }
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

    void ControlCoreTests::InstantiateCore()
    {
        Log::Comment(L"Create settings object");
        winrt::com_ptr<MockControlSettings> settings;
        settings.attach(new MockControlSettings());

        Log::Comment(L"Create connection object");
        winrt::com_ptr<MockConnection> conn;
        conn.attach(new MockConnection());
        VERIFY_IS_NOT_NULL(conn);

        Log::Comment(L"Create ControlCore object");
        auto core = winrt::make_self<Control::implementation::ControlCore>(*settings, *conn);
        VERIFY_IS_NOT_NULL(core);
    }

}
