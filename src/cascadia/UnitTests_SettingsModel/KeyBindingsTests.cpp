// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/ActionMap.h"
#include "JsonTestClass.h"
#include "TestUtils.h"

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using VirtualKeyModifiers = winrt::Windows::System::VirtualKeyModifiers;

namespace SettingsModelUnitTests
{
    class KeyBindingsTests : public JsonTestClass
    {
        TEST_CLASS(KeyBindingsTests);

        TEST_METHOD(KeyChords);
        TEST_METHOD(ManyKeysSameAction);
        TEST_METHOD(LayerKeybindings);
        TEST_METHOD(HashDeduplication);
        TEST_METHOD(HashContentArgs);
        TEST_METHOD(UnbindKeybindings);
        TEST_METHOD(LayerScancodeKeybindings);
        TEST_METHOD(TestExplicitUnbind);
        TEST_METHOD(TestArbitraryArgs);
        TEST_METHOD(TestSplitPaneArgs);
        TEST_METHOD(TestStringOverload);
        TEST_METHOD(TestSetTabColorArgs);
        TEST_METHOD(TestScrollArgs);
        TEST_METHOD(TestToggleCommandPaletteArgs);
        TEST_METHOD(TestMoveTabArgs);
        TEST_METHOD(TestGetKeyBindingForAction);
        TEST_METHOD(KeybindingsWithoutVkey);
    };

    void KeyBindingsTests::KeyChords()
    {
        struct testCase
        {
            VirtualKeyModifiers modifiers;
            int32_t vkey;
            int32_t scanCode;
            std::wstring_view expected;
        };

        static constexpr std::array testCases{
            testCase{
                VirtualKeyModifiers::None,
                'A',
                0,
                L"a",
            },
            testCase{
                VirtualKeyModifiers::Control,
                'A',
                0,
                L"ctrl+a",
            },
            testCase{
                VirtualKeyModifiers::Control | VirtualKeyModifiers::Shift,
                VK_OEM_PLUS,
                0,
                L"ctrl+shift+plus",
            },
            testCase{
                VirtualKeyModifiers::Control | VirtualKeyModifiers::Menu | VirtualKeyModifiers::Shift | VirtualKeyModifiers::Windows,
                255,
                0,
                L"win+ctrl+alt+shift+vk(255)",
            },
            testCase{
                VirtualKeyModifiers::Control | VirtualKeyModifiers::Menu | VirtualKeyModifiers::Shift | VirtualKeyModifiers::Windows,
                0,
                123,
                L"win+ctrl+alt+shift+sc(123)",
            },
        };

        for (const auto& tc : testCases)
        {
            Log::Comment(NoThrowString().Format(L"Testing case:\"%s\"", tc.expected.data()));

            const auto actualString = KeyChordSerialization::ToString({ tc.modifiers, tc.vkey, tc.scanCode });
            VERIFY_ARE_EQUAL(tc.expected, actualString);

            auto expectedVkey = tc.vkey;
            if (!expectedVkey)
            {
                expectedVkey = MapVirtualKeyW(tc.scanCode, MAPVK_VSC_TO_VK_EX);
            }

            const auto actualKeyChord = KeyChordSerialization::FromString(actualString);
            VERIFY_ARE_EQUAL(tc.modifiers, actualKeyChord.Modifiers());
            VERIFY_ARE_EQUAL(expectedVkey, actualKeyChord.Vkey());
            VERIFY_ARE_EQUAL(tc.scanCode, actualKeyChord.ScanCode());
        }
    }

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

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings1Json, OriginTag::None);
        VERIFY_ARE_EQUAL(2u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings2Json, OriginTag::None);
        VERIFY_ARE_EQUAL(4u, actionMap->_KeyMap.size());
    }

    void KeyBindingsTests::LayerKeybindings()
    {
        const std::string bindings0String{ R"([ { "command": "copy", "keys": ["ctrl+c"] } ])" };
        const std::string bindings1String{ R"([ { "command": "paste", "keys": ["ctrl+c"] } ])" };
        const std::string bindings2String{ R"([ { "command": "copy", "keys": ["enter"] } ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);
        const auto bindings1Json = VerifyParseSucceeded(bindings1String);
        const auto bindings2Json = VerifyParseSucceeded(bindings2String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings1Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings2Json, OriginTag::None);
        VERIFY_ARE_EQUAL(2u, actionMap->_KeyMap.size());
    }

    void KeyBindingsTests::HashDeduplication()
    {
        const auto actionMap = winrt::make_self<implementation::ActionMap>();
        actionMap->LayerJson(VerifyParseSucceeded(R"([ { "command": "splitPane", "keys": ["ctrl+c"] } ])"), OriginTag::User);
        actionMap->LayerJson(VerifyParseSucceeded(R"([ { "command": "splitPane", "keys": ["ctrl+c"] } ])"), OriginTag::User);
        VERIFY_ARE_EQUAL(1u, actionMap->_ActionMap.size());
    }

    void KeyBindingsTests::HashContentArgs()
    {
        Log::Comment(L"These are two actions with different content args. They should have different generated IDs for their terminal args.");
        const auto actionMap = winrt::make_self<implementation::ActionMap>();
        actionMap->LayerJson(VerifyParseSucceeded(R"([ { "command": { "action": "newTab",            } , "keys": ["ctrl+c"]       } ])"), OriginTag::User);
        actionMap->LayerJson(VerifyParseSucceeded(R"([ { "command": { "action": "newTab", "index": 0 } , "keys": ["ctrl+shift+c"] } ])"), OriginTag::User);
        VERIFY_ARE_EQUAL(2u, actionMap->_ActionMap.size());

        KeyChord ctrlC{ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 };
        KeyChord ctrlShiftC{ VirtualKeyModifiers::Control | VirtualKeyModifiers::Shift, static_cast<int32_t>('C'), 0 };

        auto hashFromKey = [&](auto& kc) {
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs.ContentArgs());
            const auto terminalArgs{ realArgs.ContentArgs().try_as<NewTerminalArgs>() };
            return terminalArgs.Hash();
        };

        const auto hashOne = hashFromKey(ctrlC);
        const auto hashTwo = hashFromKey(ctrlShiftC);

        VERIFY_ARE_NOT_EQUAL(hashOne, hashTwo);
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

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings1Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using `\"unbound\"` to unbind the key"));
        actionMap->LayerJson(bindings2Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 }));

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using `null` to unbind the key"));
        // First add back a good binding
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
        // Then try layering in the bad setting
        actionMap->LayerJson(bindings3Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 }));

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using an unrecognized command to unbind the key"));
        // First add back a good binding
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
        // Then try layering in the bad setting
        actionMap->LayerJson(bindings4Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 }));

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using a straight up invalid value to unbind the key"));
        // First add back a good binding
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
        // Then try layering in the bad setting
        actionMap->LayerJson(bindings5Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 }));

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key that wasn't bound at all"));
        actionMap->LayerJson(bindings2Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 }));
    }

    void KeyBindingsTests::TestExplicitUnbind()
    {
        const std::string bindings0String{ R"([ { "command": "copy", "keys": ["ctrl+c"] } ])" };
        const std::string bindings1String{ R"([ { "command": "unbound", "keys": ["ctrl+c"] } ])" };
        const std::string bindings2String{ R"([ { "command": "copy", "keys": ["ctrl+c"] } ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);
        const auto bindings1Json = VerifyParseSucceeded(bindings1String);
        const auto bindings2Json = VerifyParseSucceeded(bindings2String);

        const KeyChord keyChord{ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 };

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_IS_FALSE(actionMap->IsKeyChordExplicitlyUnbound(keyChord));

        actionMap->LayerJson(bindings0Json, OriginTag::User);
        VERIFY_IS_FALSE(actionMap->IsKeyChordExplicitlyUnbound(keyChord));

        actionMap->LayerJson(bindings1Json, OriginTag::User);
        VERIFY_IS_TRUE(actionMap->IsKeyChordExplicitlyUnbound(keyChord));

        actionMap->LayerJson(bindings2Json, OriginTag::User);
        VERIFY_IS_FALSE(actionMap->IsKeyChordExplicitlyUnbound(keyChord));
    }

    void KeyBindingsTests::TestArbitraryArgs()
    {
        const std::string bindings0String{ R"([
            { "command": "copy", "id": "Test.CopyNoArgs", "keys": ["ctrl+c"] },
            { "command": { "action": "copy", "singleLine": false }, "id": "Test.CopyMultiline", "keys": ["ctrl+shift+c"] },
            { "command": { "action": "copy", "singleLine": true }, "id": "Test.CopySingleline", "keys": ["alt+shift+c"] },

            { "command": "newTab", "id": "Test.NewTabNoArgs", "keys": ["ctrl+t"] },
            { "command": { "action": "newTab", "index": 0 }, "id": "Test.NewTab0", "keys": ["ctrl+shift+t"] },
            { "command": { "action": "newTab", "index": 11 }, "id": "Test.NewTab11", "keys": ["ctrl+shift+y"] },

            { "command": { "action": "copy", "madeUpBool": true }, "id": "Test.CopyFakeArgs", "keys": ["ctrl+b"] },
            { "command": { "action": "copy" }, "id": "Test.CopyNullArgs", "keys": ["ctrl+shift+b"] },

            { "command": { "action": "adjustFontSize", "delta": 1 }, "id": "Test.EnlargeFont", "keys": ["ctrl+f"] },
            { "command": { "action": "adjustFontSize", "delta": -1 }, "id": "Test.ReduceFont", "keys": ["ctrl+g"] }

        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(10u, actionMap->_KeyMap.size());

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` without args parses as Copy(SingleLine=false)"));
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('C'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            const auto& realArgs = actionAndArgs.Args().as<CopyTextArgs>();
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` with args parses them correctly"));
            KeyChord kc{ true, false, true, false, static_cast<int32_t>('C'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            const auto& realArgs = actionAndArgs.Args().as<CopyTextArgs>();
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` with args parses them correctly"));
            KeyChord kc{ false, true, true, false, static_cast<int32_t>('C'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            const auto& realArgs = actionAndArgs.Args().as<CopyTextArgs>();
            // Verify the args have the expected value
            VERIFY_IS_TRUE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` without args parses as NewTab(Index=null)"));
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('T'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<NewTabArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.ContentArgs());
            const auto terminalArgs{ realArgs.ContentArgs().try_as<NewTerminalArgs>() };
            VERIFY_IS_NOT_NULL(terminalArgs);
            VERIFY_IS_NULL(terminalArgs.ProfileIndex());
        }
        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` parses args correctly"));
            KeyChord kc{ true, false, true, false, static_cast<int32_t>('T'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<NewTabArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.ContentArgs());
            const auto terminalArgs{ realArgs.ContentArgs().try_as<NewTerminalArgs>() };
            VERIFY_IS_NOT_NULL(terminalArgs);
            VERIFY_IS_NOT_NULL(terminalArgs.ProfileIndex());
            VERIFY_ARE_EQUAL(0, terminalArgs.ProfileIndex().Value());
        }
        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `newTab` with an index greater than the legacy "
                L"args afforded parses correctly"));
            KeyChord kc{ true, false, true, false, static_cast<int32_t>('Y'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<NewTabArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.ContentArgs());
            const auto terminalArgs{ realArgs.ContentArgs().try_as<NewTerminalArgs>() };
            VERIFY_IS_NOT_NULL(terminalArgs);
            VERIFY_IS_NOT_NULL(terminalArgs.ProfileIndex());
            VERIFY_ARE_EQUAL(11, terminalArgs.ProfileIndex().Value());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` ignores args it doesn't understand"));
            KeyChord kc{ true, false, true, false, static_cast<int32_t>('B'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::CopyText, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<CopyTextArgs>();
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `copy` null as it's `args` parses as the default option"));
            KeyChord kc{ true, false, true, false, static_cast<int32_t>('B'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::CopyText, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<CopyTextArgs>();
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `adjustFontSize` with a positive delta parses args correctly"));
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('F'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::AdjustFontSize, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<AdjustFontSizeArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(1, realArgs.Delta());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Verify that `adjustFontSize` with a negative delta parses args correctly"));
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('G'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::AdjustFontSize, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<AdjustFontSizeArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(-1, realArgs.Delta());
        }
    }

    void KeyBindingsTests::TestSplitPaneArgs()
    {
        const std::string bindings0String{ R"([
            { "keys": ["ctrl+d"], "id": "Test.SplitPaneVertical", "command": { "action": "splitPane", "split": "vertical" } },
            { "keys": ["ctrl+e"], "id": "Test.SplitPaneHorizontal", "command": { "action": "splitPane", "split": "horizontal" } },
            { "keys": ["ctrl+g"], "id": "Test.SplitPane", "command": { "action": "splitPane" } },
            { "keys": ["ctrl+h"], "id": "Test.SplitPaneAuto", "command": { "action": "splitPane", "split": "auto" } }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(4u, actionMap->_KeyMap.size());

        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('D'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<SplitPaneArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('E'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<SplitPaneArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Down, realArgs.SplitDirection());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('G'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<SplitPaneArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('H'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<SplitPaneArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
        }
    }

    void KeyBindingsTests::TestSetTabColorArgs()
    {
        const std::string bindings0String{ R"([
            { "keys": ["ctrl+c"], "id": "Test.SetTabColorNull", "command": { "action": "setTabColor", "color": null } },
            { "keys": ["ctrl+d"], "id": "Test.SetTabColor", "command": { "action": "setTabColor", "color": "#123456" } },
            { "keys": ["ctrl+f"], "id": "Test.SetTabColorNoArgs", "command": "setTabColor" },
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(3u, actionMap->_KeyMap.size());

        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('C'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SetTabColor, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<SetTabColorArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.TabColor());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('D'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SetTabColor, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<SetTabColorArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TabColor());
            // Remember that COLORREFs are actually BBGGRR order, while the string is in #RRGGBB order
            VERIFY_ARE_EQUAL(til::color(0x563412), til::color(realArgs.TabColor().Value()));
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('F'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SetTabColor, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<SetTabColorArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.TabColor());
        }
    }

    void KeyBindingsTests::TestStringOverload()
    {
        const std::string bindings0String{ R"([
            { "command": "copy", "id": "Test.Copy", "keys": "ctrl+c" }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());

        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('C'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            const auto& realArgs = actionAndArgs.Args().as<CopyTextArgs>();
            // Verify the args have the expected value
            VERIFY_IS_FALSE(realArgs.SingleLine());
        }
    }

    void KeyBindingsTests::TestScrollArgs()
    {
        const std::string bindings0String{ R"([
            { "keys": ["up"], "id": "Test.ScrollUp0", "command": "scrollUp" },
            { "keys": ["down"], "id": "Test.ScrollDown0", "command": "scrollDown" },
            { "keys": ["ctrl+up"], "id": "Test.ScrollUp1", "command": { "action": "scrollUp" } },
            { "keys": ["ctrl+down"], "id": "Test.ScrollDown1", "command": { "action": "scrollDown" } },
            { "keys": ["ctrl+shift+up"], "id": "Test.ScrollUp2", "command": { "action": "scrollUp", "rowsToScroll": 10 } },
            { "keys": ["ctrl+shift+down"], "id": "Test.ScrollDown2", "command": { "action": "scrollDown", "rowsToScroll": 10 } }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(6u, actionMap->_KeyMap.size());

        {
            KeyChord kc{ false, false, false, false, static_cast<int32_t>(VK_UP), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ScrollUp, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ScrollUpArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.RowsToScroll());
        }
        {
            KeyChord kc{ false, false, false, false, static_cast<int32_t>(VK_DOWN), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ScrollDown, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ScrollDownArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.RowsToScroll());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>(VK_UP), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ScrollUp, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ScrollUpArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.RowsToScroll());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>(VK_DOWN), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ScrollDown, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ScrollDownArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NULL(realArgs.RowsToScroll());
        }
        {
            KeyChord kc{ true, false, true, false, static_cast<int32_t>(VK_UP), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ScrollUp, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ScrollUpArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.RowsToScroll());
            VERIFY_ARE_EQUAL(10u, realArgs.RowsToScroll().Value());
        }
        {
            KeyChord kc{ true, false, true, false, static_cast<int32_t>(VK_DOWN), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ScrollDown, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ScrollDownArgs>();
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.RowsToScroll());
            VERIFY_ARE_EQUAL(10u, realArgs.RowsToScroll().Value());
        }
        {
            const std::string bindingsInvalidString{ R"([{ "keys": ["up"], "command": { "action": "scrollDown", "rowsToScroll": -1 } }])" };
            const auto bindingsInvalidJson = VerifyParseSucceeded(bindingsInvalidString);
            auto invalidActionMap = winrt::make_self<implementation::ActionMap>();
            VERIFY_ARE_EQUAL(0u, invalidActionMap->_KeyMap.size());
            VERIFY_THROWS(invalidActionMap->LayerJson(bindingsInvalidJson, OriginTag::None);, std::exception);
        }
    }

    void KeyBindingsTests::TestMoveTabArgs()
    {
        const std::string bindings0String{ R"([
            { "keys": ["up"], "id": "Test.MoveTabUp", "command": { "action": "moveTab", "direction": "forward" } },
            { "keys": ["down"], "id": "Test.MoveTabDown", "command": { "action": "moveTab", "direction": "backward" } }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(2u, actionMap->_KeyMap.size());

        {
            KeyChord kc{ false, false, false, false, static_cast<int32_t>(VK_UP), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::MoveTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<MoveTabArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(realArgs.Direction(), MoveTabDirection::Forward);
        }
        {
            KeyChord kc{ false, false, false, false, static_cast<int32_t>(VK_DOWN), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::MoveTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<MoveTabArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(realArgs.Direction(), MoveTabDirection::Backward);
        }
        {
            const std::string bindingsInvalidString{ R"([{ "keys": ["up"], "command": "moveTab" }])" };
            auto actionMapNoArgs = winrt::make_self<implementation::ActionMap>();
            actionMapNoArgs->LayerJson(bindingsInvalidString, OriginTag::None);
            VERIFY_ARE_EQUAL(0u, actionMapNoArgs->_KeyMap.size());
        }
        {
            const std::string bindingsInvalidString{ R"([{ "keys": ["up"], "command": { "action": "moveTab", "direction": "bad" } }])" };
            const auto bindingsInvalidJson = VerifyParseSucceeded(bindingsInvalidString);
            auto invalidActionMap = winrt::make_self<implementation::ActionMap>();
            VERIFY_ARE_EQUAL(0u, invalidActionMap->_KeyMap.size());
            VERIFY_THROWS(invalidActionMap->LayerJson(bindingsInvalidJson, OriginTag::None);, std::exception);
        }
    }

    void KeyBindingsTests::TestToggleCommandPaletteArgs()
    {
        const std::string bindings0String{ R"([
            { "keys": ["up"], "id": "Test.CmdPal", "command": "commandPalette" },
            { "keys": ["ctrl+up"], "id": "Test.CmdPalActionMode", "command": { "action": "commandPalette", "launchMode" : "action" } },
            { "keys": ["ctrl+shift+up"], "id": "Test.CmdPalLineMode", "command": { "action": "commandPalette", "launchMode" : "commandLine" } }
        ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());
        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(3u, actionMap->_KeyMap.size());

        {
            KeyChord kc{ false, false, false, false, static_cast<int32_t>(VK_UP), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ToggleCommandPalette, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ToggleCommandPaletteArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(realArgs.LaunchMode(), CommandPaletteLaunchMode::Action);
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>(VK_UP), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ToggleCommandPalette, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ToggleCommandPaletteArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(realArgs.LaunchMode(), CommandPaletteLaunchMode::Action);
        }
        {
            KeyChord kc{ true, false, true, false, static_cast<int32_t>(VK_UP), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::ToggleCommandPalette, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().as<ToggleCommandPaletteArgs>();
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(realArgs.LaunchMode(), CommandPaletteLaunchMode::CommandLine);
        }
        {
            const std::string bindingsInvalidString{ R"([{ "keys": ["up"], "command": { "action": "commandPalette", "launchMode": "bad" } }])" };
            const auto bindingsInvalidJson = VerifyParseSucceeded(bindingsInvalidString);
            auto invalidActionMap = winrt::make_self<implementation::ActionMap>();
            VERIFY_ARE_EQUAL(0u, invalidActionMap->_KeyMap.size());
            VERIFY_THROWS(invalidActionMap->LayerJson(bindingsInvalidJson, OriginTag::None);, std::exception);
        }
    }

    void KeyBindingsTests::TestGetKeyBindingForAction()
    {
        const std::string bindings0String{ R"([ { "command": "closeWindow", "id": "Test.CloseWindow", "keys": "ctrl+a" } ])" };
        const std::string bindings1String{ R"([ { "command": { "action": "copy", "singleLine": true }, "id": "Test.Copy", "keys": "ctrl+b" } ])" };
        const std::string bindings2String{ R"([ { "command": { "action": "newTab", "index": 0 }, "id": "Test.NewTab", "keys": "ctrl+c" } ])" };
        const std::string bindings3String{ R"([ { "command": "commandPalette", "id": "Test.CmdPal", "keys": "ctrl+shift+p" } ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);
        const auto bindings1Json = VerifyParseSucceeded(bindings1String);
        const auto bindings2Json = VerifyParseSucceeded(bindings2String);
        const auto bindings3Json = VerifyParseSucceeded(bindings3String);

        auto VerifyKeyChordEquality = [](const KeyChord& expected, const KeyChord& actual) {
            if (expected)
            {
                VERIFY_IS_NOT_NULL(actual);
                VERIFY_ARE_EQUAL(expected.Modifiers(), actual.Modifiers());
                VERIFY_ARE_EQUAL(expected.Vkey(), actual.Vkey());
            }
            else
            {
                VERIFY_IS_NULL(actual);
            }
        };

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());

        {
            Log::Comment(L"simple command: no args");
            actionMap->LayerJson(bindings0Json, OriginTag::None);
            VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());
            const auto& kbd{ actionMap->GetKeyBindingForAction(L"Test.CloseWindow") };
            VerifyKeyChordEquality({ VirtualKeyModifiers::Control, static_cast<int32_t>('A'), 0 }, kbd);
        }
        {
            Log::Comment(L"command with args");
            actionMap->LayerJson(bindings1Json, OriginTag::None);
            VERIFY_ARE_EQUAL(2u, actionMap->_KeyMap.size());

            const auto& kbd{ actionMap->GetKeyBindingForAction(L"Test.Copy") };
            VerifyKeyChordEquality({ VirtualKeyModifiers::Control, static_cast<int32_t>('B'), 0 }, kbd);
        }
        {
            Log::Comment(L"command with new terminal args");
            actionMap->LayerJson(bindings2Json, OriginTag::None);
            VERIFY_ARE_EQUAL(3u, actionMap->_KeyMap.size());

            const auto& kbd{ actionMap->GetKeyBindingForAction(L"Test.NewTab") };
            VerifyKeyChordEquality({ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 }, kbd);
        }
        {
            Log::Comment(L"command with hidden args");
            actionMap->LayerJson(bindings3Json, OriginTag::None);
            VERIFY_ARE_EQUAL(4u, actionMap->_KeyMap.size());

            const auto& kbd{ actionMap->GetKeyBindingForAction(L"Test.CmdPal") };
            VerifyKeyChordEquality({ VirtualKeyModifiers::Control | VirtualKeyModifiers::Shift, static_cast<int32_t>('P'), 0 }, kbd);
        }
    }

    void KeyBindingsTests::LayerScancodeKeybindings()
    {
        Log::Comment(L"Layering a keybinding with a character literal on top of"
                     L" an equivalent sc() key should replace it.");

        // Wrap the first one in `R"!(...)!"` because it has `()` internally.
        const std::string bindings0String{ R"!([ { "command": "quakeMode", "keys":"win+sc(41)" } ])!" };
        const std::string bindings1String{ R"([ { "keys": "win+`", "command": { "action": "globalSummon", "monitor": "any" } } ])" };
        const std::string bindings2String{ R"([ { "keys": "ctrl+shift+`", "command": { "action": "quakeMode" } } ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);
        const auto bindings1Json = VerifyParseSucceeded(bindings1String);
        const auto bindings2Json = VerifyParseSucceeded(bindings2String);

        auto actionMap = winrt::make_self<implementation::ActionMap>();
        VERIFY_ARE_EQUAL(0u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings0Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size());

        actionMap->LayerJson(bindings1Json, OriginTag::None);
        VERIFY_ARE_EQUAL(1u, actionMap->_KeyMap.size(), L"Layering the second action should replace the first one.");

        actionMap->LayerJson(bindings2Json, OriginTag::None);
        VERIFY_ARE_EQUAL(2u, actionMap->_KeyMap.size());
    }

    void KeyBindingsTests::KeybindingsWithoutVkey()
    {
        const auto json = VerifyParseSucceeded(R"!([{"command": "quakeMode", "id": "Test.NoVKey", "keys":"shift+sc(255)"}])!");

        const auto actionMap = winrt::make_self<implementation::ActionMap>();
        actionMap->LayerJson(json, OriginTag::None);

        const auto action = actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Shift, 0, 255 });
        VERIFY_IS_NOT_NULL(action);
    }
}
