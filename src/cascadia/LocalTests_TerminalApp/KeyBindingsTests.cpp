// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "JsonTestClass.h"
#include "TestUtils.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class KeyBindingsTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(KeyBindingsTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(ManyKeysSameAction);
        TEST_METHOD(LayerKeybindings);
        TEST_METHOD(UnbindKeybindings);

        TEST_METHOD(TestArbitraryArgs);
        TEST_METHOD(TestSplitPaneArgs);

        TEST_METHOD(TestStringOverload);

        TEST_METHOD(TestSetTabColorArgs);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

    void KeyBindingsTests::ManyKeysSameAction()
    {
        const std::string bindings0String{ R"([ { "command": "copy", "keys": ["ctrl+c"] } ])" };
        const std::string bindings1String{ R"([ { "command": "copy", "keys": ["enter"] } ])" };
        const std::string bindings2String{ R"([
            { "command": "paste", "keys": ["ctrl+v"] },
            { "command": "paste", "keys": ["ctrl+shift+v"] }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);
        const auto bindings1Json = VerifyParseSucceeded(bindings1String);
        const auto bindings2Json = VerifyParseSucceeded(bindings2String);

        auto appKeyBindings = winrt::make_self<winrt::TerminalApp::implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings1Json);
        VERIFY_ARE_EQUAL(2u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings2Json);
        VERIFY_ARE_EQUAL(4u, appKeyBindings->_keyShortcuts.size());
    }

    void KeyBindingsTests::LayerKeybindings()
    {
        const std::string bindings0String{ R"([ { "command": "copy", "keys": ["ctrl+c"] } ])" };
        const std::string bindings1String{ R"([ { "command": "paste", "keys": ["ctrl+c"] } ])" };
        const std::string bindings2String{ R"([ { "command": "copy", "keys": ["enter"] } ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);
        const auto bindings1Json = VerifyParseSucceeded(bindings1String);
        const auto bindings2Json = VerifyParseSucceeded(bindings2String);

        auto appKeyBindings = winrt::make_self<winrt::TerminalApp::implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings1Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings2Json);
        VERIFY_ARE_EQUAL(2u, appKeyBindings->_keyShortcuts.size());
    }

    void KeyBindingsTests::UnbindKeybindings()
    {
        const std::string bindings0String{ R"([ { "command": "copy", "keys": ["ctrl+c"] } ])" };
        const std::string bindings1String{ R"([ { "command": "paste", "keys": ["ctrl+c"] } ])" };
        const std::string bindings2String{ R"([ { "command": "unbound", "keys": ["ctrl+c"] } ])" };
        const std::string bindings3String{ R"([ { "command": null, "keys": ["ctrl+c"] } ])" };
        const std::string bindings4String{ R"([ { "command": "garbage", "keys": ["ctrl+c"] } ])" };
        const std::string bindings5String{ R"([ { "command": 5, "keys": ["ctrl+c"] } ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);
        const auto bindings1Json = VerifyParseSucceeded(bindings1String);
        const auto bindings2Json = VerifyParseSucceeded(bindings2String);
        const auto bindings3Json = VerifyParseSucceeded(bindings3String);
        const auto bindings4Json = VerifyParseSucceeded(bindings4String);
        const auto bindings5Json = VerifyParseSucceeded(bindings5String);

        auto appKeyBindings = winrt::make_self<winrt::TerminalApp::implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings1Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using `\"unbound\"` to unbind the key"));
        appKeyBindings->LayerJson(bindings2Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using `null` to unbind the key"));
        // First add back a good binding
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());
        // Then try layering in the bad setting
        appKeyBindings->LayerJson(bindings3Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using an unrecognized command to unbind the key"));
        // First add back a good binding
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());
        // Then try layering in the bad setting
        appKeyBindings->LayerJson(bindings4Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using a straight up invalid value to unbind the key"));
        // First add back a good binding
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());
        // Then try layering in the bad setting
        appKeyBindings->LayerJson(bindings5Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key that wasn't bound at all"));
        appKeyBindings->LayerJson(bindings2Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());
    }

    void KeyBindingsTests::TestArbitraryArgs()
    {
        const std::string bindings0String{ R"([
            { "command": "copy", "keys": ["ctrl+c"] },
            { "command": { "action": "copy", "singleLine": false }, "keys": ["ctrl+shift+c"] },
            { "command": { "action": "copy", "singleLine": true }, "keys": ["alt+shift+c"] },

            { "command": "newTab", "keys": ["ctrl+t"] },
            { "command": { "action": "newTab", "index": 0 }, "keys": ["ctrl+shift+t"] },
            { "command": { "action": "newTab", "index": 11 }, "keys": ["ctrl+shift+y"] },

            { "command": { "action": "copy", "madeUpBool": true }, "keys": ["ctrl+b"] },
            { "command": { "action": "copy" }, "keys": ["ctrl+shift+b"] },

            { "command": { "action": "adjustFontSize", "delta": 1 }, "keys": ["ctrl+f"] },
            { "command": { "action": "adjustFontSize", "delta": -1 }, "keys": ["ctrl+g"] }

        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto appKeyBindings = winrt::make_self<implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(10u, appKeyBindings->_keyShortcuts.size());

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` without args parses as Copy(SingleLine=false)"));
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` with args parses them correctly"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('C') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` with args parses them correctly"));
            KeyChord kc{ false, true, true, static_cast<int32_t>('C') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_TRUE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` without args parses as NewTab(Index=null)"));
            KeyChord kc{ true, false, false, static_cast<int32_t>('T') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_NULL(realArgs.TerminalArgs().ProfileIndex());
        }
        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` parses args correctly"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('T') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs().ProfileIndex());
            VERIFY_ARE_EQUAL(0, realArgs.TerminalArgs().ProfileIndex().Value());
        }
        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` with an index greater than the legacy "
                L"args afforded parses correctly"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('Y') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs().ProfileIndex());
            VERIFY_ARE_EQUAL(11, realArgs.TerminalArgs().ProfileIndex().Value());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` ignores args it doesn't understand"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('B') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::CopyText, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` null as it's `args` parses as the default option"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('B') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::CopyText, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `adjustFontSize` with a positive delta parses args correctly"));
            KeyChord kc{ true, false, false, static_cast<int32_t>('F') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::AdjustFontSize, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<AdjustFontSizeArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(1, realArgs.Delta());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `adjustFontSize` with a negative delta parses args correctly"));
            KeyChord kc{ true, false, false, static_cast<int32_t>('G') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::AdjustFontSize, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<AdjustFontSizeArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(-1, realArgs.Delta());
        }
    }

    void KeyBindingsTests::TestSplitPaneArgs()
    {
        const std::string bindings0String{ R"([
            { "keys": ["ctrl+c"], "command": { "action": "splitPane", "split": null } },
            { "keys": ["ctrl+d"], "command": { "action": "splitPane", "split": "vertical" } },
            { "keys": ["ctrl+e"], "command": { "action": "splitPane", "split": "horizontal" } },
            { "keys": ["ctrl+g"], "command": { "action": "splitPane" } },
            { "keys": ["ctrl+h"], "command": { "action": "splitPane", "split": "auto" } }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto appKeyBindings = winrt::make_self<implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(5u, appKeyBindings->_keyShortcuts.size());

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('D') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('E') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Horizontal, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('G') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('H') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
        }
    }

    void KeyBindingsTests::TestSetTabColorArgs()
    {
        const std::string bindings0String{ R"([
            { "keys": ["ctrl+c"], "command": { "action": "setTabColor", "color": null } },
            { "keys": ["ctrl+d"], "command": { "action": "setTabColor", "color": "#123456" } },
            { "keys": ["ctrl+f"], "command": "setTabColor" },
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto appKeyBindings = winrt::make_self<implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(3u, appKeyBindings->_keyShortcuts.size());

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SetTabColor, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SetTabColorArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.TabColor());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('D') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SetTabColor, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SetTabColorArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TabColor());
            // Remember that COLORREFs are actually BBGGRR order, while the string is in #RRGGBB order
            VERIFY_ARE_EQUAL(static_cast<uint32_t>(til::color(0x563412)), realArgs.TabColor().Value());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('F') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SetTabColor, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SetTabColorArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.TabColor());
        }
    }

    void KeyBindingsTests::TestStringOverload()
    {
        const std::string bindings0String{ R"([
            { "command": "copy", "keys": "ctrl+c" }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto appKeyBindings = winrt::make_self<implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }
    }
}
