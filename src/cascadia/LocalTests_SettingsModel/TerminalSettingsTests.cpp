// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/TerminalSettings.h"
#include "TestUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace SettingsModelLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class TerminalSettingsTests
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(TerminalSettingsTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(TryCreateWinRTType);

        TEST_METHOD(TestTerminalArgsForBinding);

        TEST_METHOD(MakeSettingsForProfileThatDoesntExist);
        TEST_METHOD(MakeSettingsForDefaultProfileThatDoesntExist);

        TEST_METHOD(TestLayerProfileOnColorScheme);

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }
    };

    void TerminalSettingsTests::TryCreateWinRTType()
    {
        TerminalSettings settings;
        VERIFY_IS_NOT_NULL(settings);
        auto oldFontSize = settings.FontSize();
        settings.FontSize(oldFontSize + 5);
        auto newFontSize = settings.FontSize();
        VERIFY_ARE_NOT_EQUAL(oldFontSize, newFontSize);
    }

    void TerminalSettingsTests::TestTerminalArgsForBinding()
    {
        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "keybindings": [
                { "keys": ["ctrl+a"], "command": { "action": "splitPane", "split": "vertical" } },
                { "keys": ["ctrl+b"], "command": { "action": "splitPane", "split": "vertical", "profile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" } },
                { "keys": ["ctrl+c"], "command": { "action": "splitPane", "split": "vertical", "profile": "profile1" } },
                { "keys": ["ctrl+d"], "command": { "action": "splitPane", "split": "vertical", "profile": "profile2" } },
                { "keys": ["ctrl+e"], "command": { "action": "splitPane", "split": "horizontal", "commandline": "foo.exe" } },
                { "keys": ["ctrl+f"], "command": { "action": "splitPane", "split": "horizontal", "profile": "profile1", "commandline": "foo.exe" } },
                { "keys": ["ctrl+g"], "command": { "action": "newTab" } },
                { "keys": ["ctrl+h"], "command": { "action": "newTab", "startingDirectory": "c:\\foo" } },
                { "keys": ["ctrl+i"], "command": { "action": "newTab", "profile": "profile2", "startingDirectory": "c:\\foo" } },
                { "keys": ["ctrl+j"], "command": { "action": "newTab", "tabTitle": "bar" } },
                { "keys": ["ctrl+k"], "command": { "action": "newTab", "profile": "profile2", "tabTitle": "bar" } },
                { "keys": ["ctrl+l"], "command": { "action": "newTab", "profile": "profile1", "tabTitle": "bar", "startingDirectory": "c:\\foo", "commandline":"foo.exe" } }
            ]
        })" };

        const winrt::guid guid0{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid1{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}") };

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        auto actionMap = settings.GlobalSettings().ActionMap();
        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        const auto profile2Guid = settings.ActiveProfiles().GetAt(2).Guid();
        VERIFY_ARE_NOT_EQUAL(winrt::guid{}, profile2Guid);

        const auto& actionMapImpl{ winrt::get_self<implementation::ActionMap>(actionMap) };
        VERIFY_ARE_EQUAL(12u, actionMapImpl->_KeyMap.size());

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('A') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('B') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}", realArgs.TerminalArgs().Profile());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('D') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('E') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('F') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('G') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('H') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('I') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('J') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('K') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('L') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());

            const auto guid{ settings.GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
    }

    void TerminalSettingsTests::MakeSettingsForProfileThatDoesntExist()
    {
        // Test that making settings throws when the GUID doesn't exist
        const std::string settingsString{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 1
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };
        CascadiaSettings settings{ til::u8u16(settingsString) };

        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        try
        {
            auto terminalSettings{ TerminalSettings::CreateWithProfileByID(settings, guid1, nullptr) };
            VERIFY_ARE_NOT_EQUAL(nullptr, terminalSettings);
            VERIFY_ARE_EQUAL(1, terminalSettings.DefaultSettings().HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to CreateWithProfileByID should succeed");
        }

        try
        {
            auto terminalSettings{ TerminalSettings::CreateWithProfileByID(settings, guid2, nullptr) };
            VERIFY_ARE_NOT_EQUAL(nullptr, terminalSettings);
            VERIFY_ARE_EQUAL(2, terminalSettings.DefaultSettings().HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to CreateWithProfileByID should succeed");
        }

        VERIFY_THROWS(auto terminalSettings = TerminalSettings::CreateWithProfileByID(settings, guid3, nullptr), wil::ResultException, L"This call to constructor should fail");

        try
        {
            const auto termSettings{ TerminalSettings::CreateWithNewTerminalArgs(settings, nullptr, nullptr) };
            VERIFY_ARE_NOT_EQUAL(nullptr, termSettings);
            VERIFY_ARE_EQUAL(1, termSettings.DefaultSettings().HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to CreateWithNewTerminalArgs should succeed");
        }
    }

    void TerminalSettingsTests::MakeSettingsForDefaultProfileThatDoesntExist()
    {
        // Test that MakeSettings _doesnt_ throw when we load settings with a
        // defaultProfile that's not in the list, we validate the settings, and
        // then call MakeSettings(nullopt). The validation should ensure that
        // the default profile is something reasonable
        const std::string settingsString{ R"(
        {
            "defaultProfile": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 1
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };
        CascadiaSettings settings{ til::u8u16(settingsString) };

        VERIFY_ARE_EQUAL(2u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(2u, settings.ActiveProfiles().Size());
        VERIFY_ARE_EQUAL(settings.GlobalSettings().DefaultProfile(), settings.ActiveProfiles().GetAt(0).Guid());
        try
        {
            const auto termSettings{ TerminalSettings::CreateWithNewTerminalArgs(settings, nullptr, nullptr) };
            VERIFY_ARE_NOT_EQUAL(nullptr, termSettings);
            VERIFY_ARE_EQUAL(1, termSettings.DefaultSettings().HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to CreateWithNewTerminalArgs should succeed");
        }
    }

    void TerminalSettingsTests::TestLayerProfileOnColorScheme()
    {
        Log::Comment(NoThrowString().Format(
            L"Ensure that setting (or not) a property in the profile that should override a property of the color scheme works correctly."));

        const std::string settings0String{ R"(
        {
            "defaultProfile": "profile5",
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "schemeWithCursorColor"
                },
                {
                    "name" : "profile1",
                    "colorScheme": "schemeWithoutCursorColor"
                },
                {
                    "name" : "profile2",
                    "colorScheme": "schemeWithCursorColor",
                    "cursorColor": "#234567"
                },
                {
                    "name" : "profile3",
                    "colorScheme": "schemeWithoutCursorColor",
                    "cursorColor": "#345678"
                },
                {
                    "name" : "profile4",
                    "cursorColor": "#456789"
                },
                {
                    "name" : "profile5"
                }
            ],
            "schemes": [
                {
                    "name": "schemeWithCursorColor",
                    "cursorColor": "#123456"
                },
                {
                    "name": "schemeWithoutCursorColor"
                }
            ]
        })" };

        CascadiaSettings settings{ til::u8u16(settings0String) };

        VERIFY_ARE_EQUAL(6u, settings.ActiveProfiles().Size());
        VERIFY_ARE_EQUAL(2u, settings.GlobalSettings().ColorSchemes().Size());

        auto createTerminalSettings = [&](const auto& profile, const auto& schemes) {
            auto terminalSettings{ winrt::make_self<implementation::TerminalSettings>() };
            terminalSettings->_ApplyProfileSettings(profile);
            terminalSettings->_ApplyAppearanceSettings(profile.DefaultAppearance(), schemes);
            return terminalSettings;
        };

        auto terminalSettings0 = createTerminalSettings(settings.ActiveProfiles().GetAt(0), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings1 = createTerminalSettings(settings.ActiveProfiles().GetAt(1), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings2 = createTerminalSettings(settings.ActiveProfiles().GetAt(2), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings3 = createTerminalSettings(settings.ActiveProfiles().GetAt(3), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings4 = createTerminalSettings(settings.ActiveProfiles().GetAt(4), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings5 = createTerminalSettings(settings.ActiveProfiles().GetAt(5), settings.GlobalSettings().ColorSchemes());

        VERIFY_ARE_EQUAL(ARGB(0, 0x12, 0x34, 0x56), terminalSettings0->CursorColor()); // from color scheme
        VERIFY_ARE_EQUAL(DEFAULT_CURSOR_COLOR, terminalSettings1->CursorColor()); // default
        VERIFY_ARE_EQUAL(ARGB(0, 0x23, 0x45, 0x67), terminalSettings2->CursorColor()); // from profile (trumps color scheme)
        VERIFY_ARE_EQUAL(ARGB(0, 0x34, 0x56, 0x78), terminalSettings3->CursorColor()); // from profile (not set in color scheme)
        VERIFY_ARE_EQUAL(ARGB(0, 0x45, 0x67, 0x89), terminalSettings4->CursorColor()); // from profile (no color scheme)
        VERIFY_ARE_EQUAL(DEFAULT_CURSOR_COLOR, terminalSettings5->CursorColor()); // default
    }
}
