// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalControl/EventArgs.h"
#include "../TerminalControl/ControlInteractivity.h"
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
    class ControlInteractivityTests
    {
        BEGIN_TEST_CLASS(ControlInteractivityTests)
        END_TEST_CLASS()

        TEST_METHOD(TestAdjustAcrylic);

        TEST_CLASS_SETUP(ClassSetup)
        {
            winrt::init_apartment(winrt::apartment_type::single_threaded);

            return true;
        }
    };

    void ControlInteractivityTests::TestAdjustAcrylic()
    {
        winrt::com_ptr<MockControlSettings> settings;
        settings.attach(new MockControlSettings());
        winrt::com_ptr<MockConnection> conn;
        conn.attach(new MockConnection());

        settings->UseAcrylic(true);
        settings->TintOpacity(0.5f);

        Log::Comment(L"Create ControlCore object");
        auto interactivity = winrt::make_self<Control::implementation::ControlInteractivity>(*settings, *conn);
        VERIFY_IS_NOT_NULL(interactivity);
        auto core = interactivity->_core;
        VERIFY_IS_NOT_NULL(core);
    }

}
