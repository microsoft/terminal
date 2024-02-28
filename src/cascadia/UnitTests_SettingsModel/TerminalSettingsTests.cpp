// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include <til/rand.h>

#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/TerminalSettings.h"
#include "TestUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace SettingsModelUnitTests
{
    class TerminalSettingsTests
    {
        TEST_CLASS(TerminalSettingsTests);

        TEST_METHOD(TryCreateWinRTType);
        TEST_METHOD(TestTerminalArgsForBinding);
        TEST_METHOD(CommandLineToArgvW);
        TEST_METHOD(NormalizeCommandLine);
        TEST_METHOD(GetProfileForArgsWithCommandline);
        TEST_METHOD(MakeSettingsForProfile);
        TEST_METHOD(MakeSettingsForDefaultProfileThatDoesntExist);
        TEST_METHOD(TestLayerProfileOnColorScheme);
        TEST_METHOD(TestCommandlineToTitlePromotion);

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

        auto selfSettings = winrt::make_self<implementation::TerminalSettings>();
        VERIFY_ARE_EQUAL(oldFontSize, selfSettings->FontSize());

        selfSettings->FontSize(oldFontSize + 5);
        auto newFontSize = selfSettings->FontSize();
        VERIFY_ARE_NOT_EQUAL(oldFontSize, newFontSize);
    }

    // CascadiaSettings::_normalizeCommandLine abuses some aspects from CommandLineToArgvW
    // to simplify the implementation. It assumes that all arguments returned by
    // CommandLineToArgvW are returned back to back in memory as "arg1\0arg2\0arg3\0...".
    // This test ensures CommandLineToArgvW doesn't change just to be sure.
    void TerminalSettingsTests::CommandLineToArgvW()
    {
        pcg_engines::oneseq_dxsm_64_32 rng{ til::gen_random<uint64_t>() };

        const auto expectedArgc = static_cast<int>(rng(16) + 1);
        std::wstring expectedArgv;
        std::wstring input;

        // We generate up to 16 arguments. Each argument is up to 64 chars long, is quoted
        // (2 chars, only applies to the input) and separated by a whitespace (1 char).
        expectedArgv.reserve(expectedArgc * 65);
        input.reserve(expectedArgc * 67);

        for (auto i = 0; i < expectedArgc; ++i)
        {
            const auto useQuotes = static_cast<bool>(rng(2));
            // We need to ensure there is at least one character
            const auto count = static_cast<size_t>(rng(64) + 1);
            const auto ch = static_cast<wchar_t>(rng('z' - 'a' + 1) + 'a');

            if (i != 0)
            {
                expectedArgv.push_back(L'\0');
                input.push_back(L' ');
            }

            if (useQuotes)
            {
                input.push_back(L'"');
            }

            expectedArgv.append(count, ch);
            input.append(count, ch);

            if (useQuotes)
            {
                input.push_back(L'"');
            }
        }
        Log::Comment(NoThrowString().Format(input.c_str()));

        int argc;
        wil::unique_hlocal_ptr<PWSTR[]> argv{ ::CommandLineToArgvW(input.c_str(), &argc) };
        VERIFY_ARE_EQUAL(expectedArgc, argc);
        VERIFY_IS_NOT_NULL(argv);

        const auto lastArg = argv[argc - 1];
        const auto beg = argv[0];
        const auto end = lastArg + wcslen(lastArg);
        VERIFY_IS_GREATER_THAN(end, beg);
        VERIFY_ARE_EQUAL(expectedArgv.size(), static_cast<size_t>(end - beg));
        VERIFY_ARE_EQUAL(0, memcmp(beg, expectedArgv.data(), expectedArgv.size()));
    }

    // This unit test covers GH#12345.
    // * paths with more than 1 whitespace
    // * paths sharing a common prefix with another directory
    void TerminalSettingsTests::NormalizeCommandLine()
    {
        using namespace std::string_literals;

        static constexpr auto touch = [](const auto& path) {
            std::ofstream file{ path };
        };

        std::wstring guid;
        {
            GUID g{};
            THROW_IF_FAILED(CoCreateGuid(&g));
            guid = fmt::format(
                L"{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                g.Data1,
                g.Data2,
                g.Data3,
                g.Data4[0],
                g.Data4[1],
                g.Data4[2],
                g.Data4[3],
                g.Data4[4],
                g.Data4[5],
                g.Data4[6],
                g.Data4[7]);
        }

        const auto tmpdir = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto dir1 = tmpdir / guid;
        const auto dir2 = tmpdir / (guid + L" two");
        const auto file1 = dir1 / L"file 1.exe";
        const auto file2 = dir2 / L"file 2.exe";

        const auto cleanup = wil::scope_exit([&]() {
            std::error_code ec;
            remove_all(dir1, ec);
            remove_all(dir2, ec);
        });

        create_directory(dir1);
        create_directory(dir2);
        touch(file1);
        touch(file2);

        {
            const auto commandLine = file2.native() + LR"( -foo "bar1 bar2" -baz)"s;
            const auto expected = file2.native() + L"\0-foo\0bar1 bar2\0-baz"s;
            const auto actual = implementation::Profile::NormalizeCommandLine(commandLine.c_str());
            VERIFY_ARE_EQUAL(expected, actual);
        }
        {
            const auto commandLine = L"C:\\";
            const auto expected = L"C:\\";
            const auto actual = implementation::Profile::NormalizeCommandLine(commandLine);
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    void TerminalSettingsTests::GetProfileForArgsWithCommandline()
    {
        // I'm exclusively using cmd.exe as I know exactly where it resides at.
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "defaults": {
                    "historySize": 123
                },
                "list": [
                    {
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "commandline": "%SystemRoot%\\System32\\cmd.exe"
                    },
                    {
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                        "commandline": "cmd.exe /A"
                    },
                    {
                        "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                        "commandline": "cmd.exe /A /B"
                    },
                    {
                        "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
                        "commandline": "cmd.exe /A /C",
                        "connectionType": "{9a9977a7-1fe0-49c0-b6c0-13a0cd1c98a1}"
                    },
                    {
                        "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}",
                        "commandline": "C:\\invalid.exe",
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);

        struct TestCase
        {
            std::wstring_view input;
            int expected;
        };

        static constexpr std::array testCases{
            // Base test.
            TestCase{ L"cmd.exe", 0 },
            // SearchPathW() normalization + case insensitive matching.
            TestCase{ L"cmd.exe /a", 1 },
            TestCase{ L"%SystemRoot%\\System32\\cmd.exe /A", 1 },
            // Test that we don't pick the equally long but different "/A /B" variant.
            TestCase{ L"C:\\Windows\\System32\\cmd.exe /A /C", 1 },
            // Test that we don't pick the shorter "/A" variant,
            // but do pick the shorter "/A /B" variant for longer inputs.
            TestCase{ L"cmd.exe /A /B", 2 },
            TestCase{ L"cmd.exe /A /B /C", 2 },
            // Ignore profiles with a connection type, like the Azure cloud shell.
            // Instead it should pick any other prefix.
            TestCase{ L"C:\\Windows\\System32\\cmd.exe /A /C", 1 },
            // Failure to normalize a path (e.g. because the path doesn't exist)
            // should yield the unmodified input string (see NormalizeCommandLine).
            TestCase{ L"C:\\invalid.exe /A /B", 4 },
            // Return base layer profile for missing profiles.
            TestCase{ L"C:\\Windows\\regedit.exe", -1 },
        };

        for (const auto& testCase : testCases)
        {
            NewTerminalArgs args;
            args.Commandline(testCase.input);

            const auto profile = settings->GetProfileForArgs(args);
            VERIFY_IS_NOT_NULL(profile);

            if (testCase.expected < 0)
            {
                VERIFY_ARE_EQUAL(123, profile.HistorySize());
            }
            else
            {
                GUID expectedGUID{ 0x6239a42c, static_cast<uint16_t>(0x1111 * testCase.expected), 0x49a3, { 0x80, 0xbd, 0xe8, 0xfd, 0xd0, 0x45, 0x18, 0x5c } };
                VERIFY_ARE_EQUAL(expectedGUID, static_cast<const GUID&>(profile.Guid()));
            }
        }
    }

    void TerminalSettingsTests::TestTerminalArgsForBinding()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
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
            "defaults": {
                "historySize": 29
            } },
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

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);

        auto actionMap = settings->GlobalSettings().ActionMap();
        VERIFY_ARE_EQUAL(3u, settings->ActiveProfiles().Size());

        const auto profile2Guid = settings->ActiveProfiles().GetAt(2).Guid();
        VERIFY_ARE_NOT_EQUAL(winrt::guid{}, profile2Guid);

        const auto& actionMapImpl{ winrt::get_self<implementation::ActionMap>(actionMap) };
        VERIFY_ARE_EQUAL(12u, actionMapImpl->_KeyMap.size());

        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('A'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, profile.Guid());
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('B'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}", realArgs.TerminalArgs().Profile());

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid1, profile.Guid());
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('C'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid1, profile.Guid());
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('D'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(profile2Guid, profile.Guid());
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('E'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Down, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            // This action specified a command but no profile; it gets reassigned to the base profile
            VERIFY_ARE_EQUAL(settings->ProfileDefaults(), profile);
            VERIFY_ARE_EQUAL(29, termSettings.HistorySize());
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('F'), 0 };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Down, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid1, profile.Guid());
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('G'), 0 };
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

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, profile.Guid());
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('H'), 0 };
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

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, profile.Guid());
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('I'), 0 };
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

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(profile2Guid, profile.Guid());
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('J'), 0 };
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

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid0, profile.Guid());
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('K'), 0 };
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

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(profile2Guid, profile.Guid());
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('L'), 0 };
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

            const auto profile{ settings->GetProfileForArgs(realArgs.TerminalArgs()) };
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, realArgs.TerminalArgs(), nullptr) };
            const auto termSettings = settingsStruct.DefaultSettings();
            VERIFY_ARE_EQUAL(guid1, profile.Guid());
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
    }

    void TerminalSettingsTests::MakeSettingsForProfile()
    {
        // Test that making settings generally works.
        static constexpr std::string_view settingsString{ R"(
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
        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsString);

        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");

        const auto profile1 = settings->FindProfile(guid1);
        const auto profile2 = settings->FindProfile(guid2);

        try
        {
            auto terminalSettings{ TerminalSettings::CreateWithProfile(*settings, profile1, nullptr) };
            VERIFY_ARE_NOT_EQUAL(nullptr, terminalSettings);
            VERIFY_ARE_EQUAL(1, terminalSettings.DefaultSettings().HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to CreateWithProfile should succeed");
        }

        try
        {
            auto terminalSettings{ TerminalSettings::CreateWithProfile(*settings, profile2, nullptr) };
            VERIFY_ARE_NOT_EQUAL(nullptr, terminalSettings);
            VERIFY_ARE_EQUAL(2, terminalSettings.DefaultSettings().HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to CreateWithProfile should succeed");
        }

        try
        {
            const auto termSettings{ TerminalSettings::CreateWithNewTerminalArgs(*settings, nullptr, nullptr) };
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
        static constexpr std::string_view settingsString{ R"(
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
        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsString);

        VERIFY_ARE_EQUAL(2u, settings->Warnings().Size());
        VERIFY_ARE_EQUAL(2u, settings->ActiveProfiles().Size());
        VERIFY_ARE_EQUAL(settings->GlobalSettings().DefaultProfile(), settings->ActiveProfiles().GetAt(0).Guid());
        try
        {
            const auto termSettings{ TerminalSettings::CreateWithNewTerminalArgs(*settings, nullptr, nullptr) };
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

        static constexpr std::string_view settings0String{ R"(
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
                    "cursorColor": "#123456",
                    "black": "#121314",
                    "red": "#121314",
                    "green": "#121314",
                    "yellow": "#121314",
                    "blue": "#121314",
                    "purple": "#121314",
                    "cyan": "#121314",
                    "white": "#121314",
                    "brightBlack": "#121314",
                    "brightRed": "#121314",
                    "brightGreen": "#121314",
                    "brightYellow": "#121314",
                    "brightBlue": "#121314",
                    "brightPurple": "#121314",
                    "brightCyan": "#121314",
                    "brightWhite": "#121314"
                },
                {
                    "name": "schemeWithoutCursorColor",
                    "black": "#121314",
                    "red": "#121314",
                    "green": "#121314",
                    "yellow": "#121314",
                    "blue": "#121314",
                    "purple": "#121314",
                    "cyan": "#121314",
                    "white": "#121314",
                    "brightBlack": "#121314",
                    "brightRed": "#121314",
                    "brightGreen": "#121314",
                    "brightYellow": "#121314",
                    "brightBlue": "#121314",
                    "brightPurple": "#121314",
                    "brightCyan": "#121314",
                    "brightWhite": "#121314"
                }
            ]
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settings0String);

        VERIFY_ARE_EQUAL(6u, settings->ActiveProfiles().Size());
        VERIFY_ARE_EQUAL(2u, settings->GlobalSettings().ColorSchemes().Size());

        auto createTerminalSettings = [&](const auto& profile, const auto& schemes, const auto& Theme) {
            auto terminalSettings{ winrt::make_self<implementation::TerminalSettings>() };
            terminalSettings->_ApplyProfileSettings(profile);
            terminalSettings->_ApplyAppearanceSettings(profile.DefaultAppearance(), schemes, Theme);
            return terminalSettings;
        };

        const auto activeProfiles = settings->ActiveProfiles();
        const auto colorSchemes = settings->GlobalSettings().ColorSchemes();
        const auto currentTheme = settings->GlobalSettings().CurrentTheme();
        const auto terminalSettings0 = createTerminalSettings(activeProfiles.GetAt(0), colorSchemes, currentTheme);
        const auto terminalSettings1 = createTerminalSettings(activeProfiles.GetAt(1), colorSchemes, currentTheme);
        const auto terminalSettings2 = createTerminalSettings(activeProfiles.GetAt(2), colorSchemes, currentTheme);
        const auto terminalSettings3 = createTerminalSettings(activeProfiles.GetAt(3), colorSchemes, currentTheme);
        const auto terminalSettings4 = createTerminalSettings(activeProfiles.GetAt(4), colorSchemes, currentTheme);
        const auto terminalSettings5 = createTerminalSettings(activeProfiles.GetAt(5), colorSchemes, currentTheme);

        VERIFY_ARE_EQUAL(til::color(0x12, 0x34, 0x56), terminalSettings0->CursorColor()); // from color scheme
        VERIFY_ARE_EQUAL(DEFAULT_CURSOR_COLOR, terminalSettings1->CursorColor()); // default
        VERIFY_ARE_EQUAL(til::color(0x23, 0x45, 0x67), terminalSettings2->CursorColor()); // from profile (trumps color scheme)
        VERIFY_ARE_EQUAL(til::color(0x34, 0x56, 0x78), terminalSettings3->CursorColor()); // from profile (not set in color scheme)
        VERIFY_ARE_EQUAL(til::color(0x45, 0x67, 0x89), terminalSettings4->CursorColor()); // from profile (no color scheme)
        VERIFY_ARE_EQUAL(DEFAULT_CURSOR_COLOR, terminalSettings5->CursorColor()); // default
    }

    void TerminalSettingsTests::TestCommandlineToTitlePromotion()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
            ],
            "defaults": {
                "historySize": 29
            } }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);

        { // just a profile (profile wins)
            NewTerminalArgs args{};
            args.Profile(L"profile0");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"profile0", settingsStruct.DefaultSettings().StartingTitle());
        }
        { // profile and command line -> no promotion (profile wins)
            NewTerminalArgs args{};
            args.Profile(L"profile0");
            args.Commandline(L"foo.exe");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"profile0", settingsStruct.DefaultSettings().StartingTitle());
        }
        { // just a title -> it is propagated
            NewTerminalArgs args{};
            args.TabTitle(L"Analog Kid");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"Analog Kid", settingsStruct.DefaultSettings().StartingTitle());
        }
        { // title and command line -> no promotion
            NewTerminalArgs args{};
            args.TabTitle(L"Digital Man");
            args.Commandline(L"foo.exe");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"Digital Man", settingsStruct.DefaultSettings().StartingTitle());
        }
        { // just a commandline -> promotion
            NewTerminalArgs args{};
            args.Commandline(L"foo.exe");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"foo.exe", settingsStruct.DefaultSettings().StartingTitle());
        }
        // various typesof commandline follow
        {
            NewTerminalArgs args{};
            args.Commandline(L"foo.exe bar");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"foo.exe", settingsStruct.DefaultSettings().StartingTitle());
        }
        {
            NewTerminalArgs args{};
            args.Commandline(L"\"foo exe.exe\" bar");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"foo exe.exe", settingsStruct.DefaultSettings().StartingTitle());
        }
        {
            NewTerminalArgs args{};
            args.Commandline(L"\"\" grand designs");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"", settingsStruct.DefaultSettings().StartingTitle());
        }
        {
            NewTerminalArgs args{};
            args.Commandline(L" imagine a man");
            const auto settingsStruct{ TerminalSettings::CreateWithNewTerminalArgs(*settings, args, nullptr) };
            VERIFY_ARE_EQUAL(L"", settingsStruct.DefaultSettings().StartingTitle());
        }
    }
}
