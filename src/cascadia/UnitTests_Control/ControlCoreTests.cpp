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
        TEST_METHOD(TestInitialize);
        TEST_METHOD(TestAdjustAcrylic);

        TEST_METHOD(TestFreeAfterClose);

        TEST_METHOD(TestFontInitializedInCtor);

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

    void ControlCoreTests::TestInitialize()
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

        VERIFY_IS_FALSE(core->_initializedTerminal);
        // "Cascadia Mono" ends up with an actual size of 9x19 at 96DPI. So
        // let's just arbitrarily start with a 270x380px (30x20 chars) window
        core->InitializeTerminal(270, 380, 1.0, 1.0);
        VERIFY_IS_TRUE(core->_initializedTerminal);
        VERIFY_ARE_EQUAL(30, core->_terminal->GetViewport().Width());
    }

    void ControlCoreTests::TestAdjustAcrylic()
    {
        winrt::com_ptr<MockControlSettings> settings;
        settings.attach(new MockControlSettings());
        winrt::com_ptr<MockConnection> conn;
        conn.attach(new MockConnection());

        settings->UseAcrylic(true);
        settings->TintOpacity(0.5f);

        Log::Comment(L"Create ControlCore object");
        auto core = winrt::make_self<Control::implementation::ControlCore>(*settings, *conn);
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

        VERIFY_IS_FALSE(core->_initializedTerminal);
        // "Cascadia Mono" ends up with an actual size of 9x19 at 96DPI. So
        // let's just arbitrarily start with a 270x380px (30x20 chars) window
        core->InitializeTerminal(270, 380, 1.0, 1.0);
        VERIFY_IS_TRUE(core->_initializedTerminal);

        Log::Comment(L"Increasing opacity till fully opaque");
        expectedOpacity += 0.1; // = 0.6;
        core->AdjustOpacity(0.1);
        expectedOpacity += 0.1; // = 0.7;
        core->AdjustOpacity(0.1);
        expectedOpacity += 0.1; // = 0.8;
        core->AdjustOpacity(0.1);
        expectedOpacity += 0.1; // = 0.9;
        core->AdjustOpacity(0.1);
        expectedOpacity += 0.1; // = 1.0;
        // cast to float because floating point numbers are mean
        VERIFY_ARE_EQUAL(1.0f, base::saturated_cast<float>(expectedOpacity));
        core->AdjustOpacity(0.1);

        Log::Comment(L"Increasing opacity more doesn't actually change it to be >1.0");
        // DebugBreak();
        expectedOpacity = 1.0;
        core->AdjustOpacity(0.1);

        Log::Comment(L"Decrease opacity");
        expectedOpacity -= 0.25; // = 0.75;
        core->AdjustOpacity(-0.25);
        expectedOpacity -= 0.25; // = 0.5;
        core->AdjustOpacity(-0.25);
        expectedOpacity -= 0.25; // = 0.25;
        core->AdjustOpacity(-0.25);
        expectedOpacity -= 0.25; // = 0.05;
        // cast to float because floating point numbers are mean
        VERIFY_ARE_EQUAL(0.0f, base::saturated_cast<float>(expectedOpacity));
        core->AdjustOpacity(-0.25);

        Log::Comment(L"Decreasing opacity more doesn't actually change it to be < 0");
        expectedOpacity = 0.0;
        core->AdjustOpacity(-0.25);
    }

    void ControlCoreTests::TestFreeAfterClose()
    {
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

            Log::Comment(L"Close the Core, like a TermControl would");
            core->Close();
        }

        VERIFY_IS_TRUE(true, L"Make sure that the test didn't crash when the core when out of scope");
    }

    void ControlCoreTests::TestFontInitializedInCtor()
    {
        // This is to catch a dumb programming mistake I made while working on
        // the ore/control split. We want the font initialized in the ctor,
        // before we even get to Core::Initialize.
        VERIFY_IS_TRUE(false, L"Make sure the core's _font is initialized from the settings in the ctor");

        Log::Comment(L"Create settings object");
        winrt::com_ptr<MockControlSettings> settings;
        settings.attach(new MockControlSettings());
        // Make sure to use something dumb like "Impact" as a font name here so
        // that you don't default to Cascadia*
        settings->FontFace(L"Impact");

        Log::Comment(L"Create connection object");
        winrt::com_ptr<MockConnection> conn;
        conn.attach(new MockConnection());
        VERIFY_IS_NOT_NULL(conn);

        Log::Comment(L"Create ControlCore object");
        auto core = winrt::make_self<Control::implementation::ControlCore>(*settings, *conn);
        VERIFY_IS_NOT_NULL(core);

        VERIFY_ARE_EQUAL(L"Impact", std::wstring{ core->_actualFont.GetFaceName() });
    }

}
