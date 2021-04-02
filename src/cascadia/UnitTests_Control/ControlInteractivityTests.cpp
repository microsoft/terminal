// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalControl/EventArgs.h"
#include "../TerminalControl/ControlInteractivity.h"
#include "MockControlSettings.h"
#include "MockConnection.h"

using namespace ::Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace ::Microsoft::Terminal::Core;

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
        Log::Comment(L"Test that scrolling the mouse wheel with Ctrl+Shift changes opacity");
        Log::Comment(L"(This test won't log as it goes, because it does some 200 verifications.)");

        WEX::TestExecution::SetVerifyOutput verifyOutputScope{ WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures };

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

        // A callback to make sure that we're raising TransparencyChanged events
        double expectedOpacity = 0.5;
        auto opacityCallback = [&](auto&&, Control::TransparencyChangedEventArgs args) mutable {
            VERIFY_ARE_EQUAL(expectedOpacity, args.Opacity());
            VERIFY_ARE_EQUAL(expectedOpacity, settings->TintOpacity());
            VERIFY_ARE_EQUAL(expectedOpacity, core->_settings.TintOpacity());

            if (expectedOpacity < 1.0)
            {
                VERIFY_IS_TRUE(settings->UseAcrylic());
                VERIFY_IS_TRUE(core->_settings.UseAcrylic());
            }
            VERIFY_ARE_EQUAL(expectedOpacity < 1.0, settings->UseAcrylic());
            VERIFY_ARE_EQUAL(expectedOpacity < 1.0, core->_settings.UseAcrylic());
        };
        core->TransparencyChanged(opacityCallback);

        const auto modifiers = ControlKeyStates(ControlKeyStates::RightCtrlPressed | ControlKeyStates::ShiftPressed);

        Log::Comment(L"Scroll in the positive direction, increasing opacity");
        // Scroll more than enough times to get to 1.0 from .5.
        for (int i = 0; i < 55; i++)
        {
            // each mouse wheel only adjusts opacity by .01
            expectedOpacity += 0.01;
            if (expectedOpacity >= 1.0)
            {
                expectedOpacity = 1.0;
            }

            // The mouse location and buttons don't matter here.
            interactivity->MouseWheel(modifiers,
                                      30,
                                      til::point{ 0, 0 },
                                      { false, false, false });
        }

        Log::Comment(L"Scroll in the negative direction, decreasing opacity");
        // Scroll more than enough times to get to 0.0 from 1.0
        for (int i = 0; i < 105; i++)
        {
            // each mouse wheel only adjusts opacity by .01
            expectedOpacity -= 0.01;
            if (expectedOpacity <= 0.0)
            {
                expectedOpacity = 0.0;
            }

            // The mouse location and buttons don't matter here.
            interactivity->MouseWheel(modifiers,
                                      30,
                                      til::point{ 0, 0 },
                                      { false, false, false });
        }
    }

}
