// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppLocalTests
{
    // Unfortunately, these tests _WILL NOT_ work in our CI, until we have a lab
    // machine available that can run Windows version 18362.

    class KeyBindingsTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(KeyBindingsTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TerminalApp.LocalTests.AppxManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(ManyKeysSameAction);
        TEST_METHOD(LayerKeybindings);
        TEST_METHOD(UnbindKeybindings);

        TEST_METHOD(TestArbitraryArgs);
        TEST_METHOD(TestSplitPaneArgs);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }

        // Function Description:
        // - This is a helper to retrieve the ActionAndArgs from the keybindings
        //   for a given chord.
        // Arguments:
        // - bindings: The AppKeyBindings to lookup the ActionAndArgs from.
        // - kc: The key chord to look up the bound ActionAndArgs for.
        // Return Value:
        // - The ActionAndArgs bound to the given key, or nullptr if nothing is bound to it.
        static const ActionAndArgs KeyBindingsTests::GetActionAndArgs(const implementation::AppKeyBindings& bindings,
                                                                      const KeyChord& kc)
        {
            std::wstring buffer{ L"" };
            if (WI_IsFlagSet(kc.Modifiers(), KeyModifiers::Ctrl))
            {
                buffer += L"Ctrl+";
            }
            if (WI_IsFlagSet(kc.Modifiers(), KeyModifiers::Shift))
            {
                buffer += L"Shift+";
            }
            if (WI_IsFlagSet(kc.Modifiers(), KeyModifiers::Alt))
            {
                buffer += L"Alt+";
            }
            buffer += static_cast<wchar_t>(MapVirtualKeyW(kc.Vkey(), MAPVK_VK_TO_CHAR));
            Log::Comment(NoThrowString().Format(L"Looking for key:%s", buffer.c_str()));

            const auto keyIter = bindings._keyShortcuts.find(kc);
            VERIFY_IS_TRUE(keyIter != bindings._keyShortcuts.end(), L"Expected to find an action bound to the given KeyChord");
            if (keyIter != bindings._keyShortcuts.end())
            {
                return keyIter->second;
            }
            return nullptr;
        };
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
            { "command": "copyTextWithoutNewlines", "keys": ["alt+c"] },
            { "command": { "action": "copy", "trimWhitespace": false }, "keys": ["ctrl+shift+c"] },
            { "command": { "action": "copy", "trimWhitespace": true }, "keys": ["alt+shift+c"] },

            { "command": "newTab", "keys": ["ctrl+t"] },
            { "command": { "action": "newTab", "index": 0 }, "keys": ["ctrl+shift+t"] },
            { "command": "newTabProfile0", "keys": ["alt+shift+t"] },
            { "command": { "action": "newTab", "index": 11 }, "keys": ["ctrl+shift+y"] },
            { "command": "newTabProfile8", "keys": ["alt+shift+y"] },

            { "command": { "action": "copy", "madeUpBool": true }, "keys": ["ctrl+b"] },
            { "command": { "action": "copy" }, "keys": ["ctrl+shift+b"] },

            { "command": "increaseFontSize", "keys": ["ctrl+f"] },
            { "command": "decreaseFontSize", "keys": ["ctrl+g"] }

        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto appKeyBindings = winrt::make_self<implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(13u, appKeyBindings->_keyShortcuts.size());

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` without args parses as Copy(TrimWhitespace=true)"));
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_TRUE(realArgs.TrimWhitespace());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copyTextWithoutNewlines` parses as Copy(TrimWhitespace=false)"));
            KeyChord kc{ false, true, false, static_cast<int32_t>('C') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.TrimWhitespace());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` with args parses them correctly"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('C') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.TrimWhitespace());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` with args parses them correctly"));
            KeyChord kc{ false, true, true, static_cast<int32_t>('C') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_TRUE(realArgs.TrimWhitespace());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` without args parses as NewTab(Index=null)"));
            KeyChord kc{ true, false, false, static_cast<int32_t>('T') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.ProfileIndex());
        }
        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` parses args correctly"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('T') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.ProfileIndex());
            VERIFY_ARE_EQUAL(0, realArgs.ProfileIndex().Value());
        }
        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTabProfile0` parses as NewTab(Index=0)"));
            KeyChord kc{ false, true, true, static_cast<int32_t>('T') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTabProfile0, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.ProfileIndex());
            VERIFY_ARE_EQUAL(0, realArgs.ProfileIndex().Value());
        }
        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` with an index greater than the legacy "
                L"args afforded parses correctly"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('Y') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.ProfileIndex());
            VERIFY_ARE_EQUAL(11, realArgs.ProfileIndex().Value());
        }
        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTabProfile8` parses as NewTab(Index=8)"));
            KeyChord kc{ false, true, true, static_cast<int32_t>('Y') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTabProfile8, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.ProfileIndex());
            VERIFY_ARE_EQUAL(8, realArgs.ProfileIndex().Value());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` ignores args it doesn't understand"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('B') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::CopyText, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_TRUE(realArgs.TrimWhitespace());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` null as it's `args` parses as the default option"));
            KeyChord kc{ true, false, true, static_cast<int32_t>('B') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::CopyText, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_TRUE(realArgs.TrimWhitespace());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `increaseFontSize` without args parses as AdjustFontSize(Delta=1)"));
            KeyChord kc{ true, false, false, static_cast<int32_t>('F') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::IncreaseFontSize, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<AdjustFontSizeArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(1, realArgs.Delta());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `decreaseFontSize` without args parses as AdjustFontSize(Delta=-1)"));
            KeyChord kc{ true, false, false, static_cast<int32_t>('G') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::DecreaseFontSize, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<AdjustFontSizeArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(-1, realArgs.Delta());
        }
    }

    void KeyBindingsTests::TestSplitPaneArgs()
    {
        const std::string bindings0String{ R"([
            { "keys": ["ctrl+a"], "command": "splitVertical" },
            { "keys": ["ctrl+b"], "command": "splitHorizontal" },
            { "keys": ["ctrl+c"], "command": { "action": "splitPane", "split": null } },
            { "keys": ["ctrl+d"], "command": { "action": "splitPane", "split": "vertical" } },
            { "keys": ["ctrl+e"], "command": { "action": "splitPane", "split": "horizontal" } },
            { "keys": ["ctrl+f"], "command": { "action": "splitPane", "split": "none" } },
            { "keys": ["ctrl+g"], "command": { "action": "splitPane" } }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto appKeyBindings = winrt::make_self<implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(7u, appKeyBindings->_keyShortcuts.size());

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('A') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitVertical, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('B') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitHorizontal, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Horizontal, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::None, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('D') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('E') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Horizontal, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('F') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::None, realArgs.SplitStyle());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('G') };
            auto actionAndArgs = GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::None, realArgs.SplitStyle());
        }
    }

}
