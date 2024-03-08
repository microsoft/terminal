// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/TerminalPage.h"
#include "../UnitTests_SettingsModel/TestUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace TerminalAppLocalTests
{
    static constexpr std::wstring_view inboxSettings{ LR"({
        "schemes": [{
            "name": "Campbell",
            "foreground": "#CCCCCC",
            "background": "#0C0C0C",
            "cursorColor": "#FFFFFF",
            "black": "#0C0C0C",
            "red": "#C50F1F",
            "green": "#13A10E",
            "yellow": "#C19C00",
            "blue": "#0037DA",
            "purple": "#881798",
            "cyan": "#3A96DD",
            "white": "#CCCCCC",
            "brightBlack": "#767676",
            "brightRed": "#E74856",
            "brightGreen": "#16C60C",
            "brightYellow": "#F9F1A5",
            "brightBlue": "#3B78FF",
            "brightPurple": "#B4009E",
            "brightCyan": "#61D6D6",
            "brightWhite": "#F2F2F2"
        }]
    })" };

    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class SettingsTests
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(SettingsTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(TestIterateCommands);
        TEST_METHOD(TestIterateOnGeneratedNamedCommands);
        TEST_METHOD(TestIterateOnBadJson);
        TEST_METHOD(TestNestedCommands);
        TEST_METHOD(TestNestedInNestedCommand);
        TEST_METHOD(TestNestedInIterableCommand);
        TEST_METHOD(TestIterableInNestedCommand);
        TEST_METHOD(TestMixedNestedAndIterableCommand);

        TEST_METHOD(TestIterableColorSchemeCommands);

        TEST_METHOD(TestElevateArg);

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }

    private:
        void _logCommandNames(winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, Command> commands, const int indentation = 1)
        {
            if (indentation == 1)
            {
                Log::Comment((commands.Size() == 0) ? L"Commands:\n  <none>" : L"Commands:");
            }
            for (const auto& nameAndCommand : commands)
            {
                Log::Comment(fmt::format(L"{0:>{1}}* {2}->{3}",
                                         L"",
                                         indentation,
                                         nameAndCommand.Key(),
                                         nameAndCommand.Value().Name())
                                 .c_str());

                if (nameAndCommand.Value().HasNestedCommands())
                {
                    _logCommandNames(nameAndCommand.Value().NestedCommands(), indentation + 2);
                }
            }
        }
        void _logCommands(winrt::Windows::Foundation::Collections::IVector<Command> commands, const int indentation = 1)
        {
            if (indentation == 1)
            {
                Log::Comment((commands.Size() == 0) ? L"Commands:\n  <none>" : L"Commands:");
            }
            for (const auto& cmd : commands)
            {
                Log::Comment(fmt::format(L"{0:>{1}}* {2}",
                                         L"",
                                         indentation,
                                         cmd.Name())
                                 .c_str());

                if (cmd.HasNestedCommands())
                {
                    _logCommandNames(cmd.NestedCommands(), indentation + 2);
                }
            }
        }
    };

    void SettingsTests::TestIterateCommands()
    {
        // For this test, put an iterable command with a given `name`,
        // containing a ${profile.name} to replace. When we expand it, it should
        // have created one command for each profile.

        static constexpr std::wstring_view settingsJson{ LR"(
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
            "actions": [
                {
                    "name": "iterable command ${profile.name}",
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ]
        })" };

        CascadiaSettings settings{ settingsJson, inboxSettings };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        auto nameMap{ settings.ActionMap().NameMap() };
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            auto command = nameMap.TryLookup(L"iterable command ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.GetAt(0);
            VERIFY_ARE_EQUAL(L"iterable command profile0", command.Name());
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.GetAt(1);
            VERIFY_ARE_EQUAL(L"iterable command profile1", command.Name());
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.GetAt(2);
            VERIFY_ARE_EQUAL(L"iterable command profile2", command.Name());
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestIterateOnGeneratedNamedCommands()
    {
        // For this test, put an iterable command without a given `name` to
        // replace. When we expand it, it should still work.

        static constexpr std::wstring_view settingsJson{ LR"(
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
            "actions": [
                {
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ]
        })" };

        CascadiaSettings settings{ settingsJson, inboxSettings };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        auto nameMap{ settings.ActionMap().NameMap() };
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            auto command = nameMap.TryLookup(L"Split pane, profile: ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.GetAt(0);
            VERIFY_ARE_EQUAL(L"Split pane, profile: profile0", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.GetAt(1);
            VERIFY_ARE_EQUAL(L"Split pane, profile: profile1", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.GetAt(2);
            VERIFY_ARE_EQUAL(L"Split pane, profile: profile2", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestIterateOnBadJson()
    {
        // For this test, put an iterable command with a profile name that would
        // cause bad json to be filled in. Something like a profile with a name
        // of "Foo\"", so the trailing '"' might break the json parsing.

        static constexpr std::wstring_view settingsJson{ LR"(
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
                    "name": "profile1\"",
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
            "actions": [
                {
                    "name": "iterable command ${profile.name}",
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ]
        })" };

        CascadiaSettings settings{ settingsJson, inboxSettings };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        auto nameMap{ settings.ActionMap().NameMap() };
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            auto command = nameMap.TryLookup(L"iterable command ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.GetAt(0);
            VERIFY_ARE_EQUAL(L"iterable command profile0", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.GetAt(1);
            VERIFY_ARE_EQUAL(L"iterable command profile1\"", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1\"", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.GetAt(2);
            VERIFY_ARE_EQUAL(L"iterable command profile2", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestNestedCommands()
    {
        // This test checks a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ Connect to ssh...
        //    ├─ first.com
        //    └─ second.com

        static constexpr std::wstring_view settingsJson{ LR"(
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
            "actions": [
                {
                    "name": "Connect to ssh...",
                    "commands": [
                        {
                            "name": "first.com",
                            "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                        },
                        {
                            "name": "second.com",
                            "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                        }
                    ]
                },
            ]
        })" };

        CascadiaSettings settings{ settingsJson, inboxSettings };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommand = expandedCommands.GetAt(0);
        VERIFY_IS_NOT_NULL(rootCommand);
        VERIFY_ARE_EQUAL(L"Connect to ssh...", rootCommand.Name());
        auto rootActionAndArgs = rootCommand.ActionAndArgs();
        VERIFY_IS_NOT_NULL(rootActionAndArgs);
        VERIFY_ARE_EQUAL(ShortcutAction::Invalid, rootActionAndArgs.Action());

        VERIFY_ARE_EQUAL(2u, rootCommand.NestedCommands().Size());

        {
            winrt::hstring commandName{ L"first.com" };
            auto command = rootCommand.NestedCommands().Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            VERIFY_IS_FALSE(command.HasNestedCommands());
        }
        {
            winrt::hstring commandName{ L"second.com" };
            auto command = rootCommand.NestedCommands().Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            VERIFY_IS_FALSE(command.HasNestedCommands());
        }
    }

    void SettingsTests::TestNestedInNestedCommand()
    {
        // This test checks a nested command that includes nested commands.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ grandparent
        //    └─ parent
        //       ├─ child1
        //       └─ child2

        static constexpr std::wstring_view settingsJson{ LR"(
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
            "actions": [
                {
                    "name": "grandparent",
                    "commands": [
                        {
                            "name": "parent",
                            "commands": [
                                {
                                    "name": "child1",
                                    "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                                },
                                {
                                    "name": "child2",
                                    "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                                }
                            ]
                        },
                    ]
                },
            ]
        })" };

        CascadiaSettings settings{ settingsJson, inboxSettings };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto grandparentCommand = expandedCommands.GetAt(0);
        VERIFY_IS_NOT_NULL(grandparentCommand);
        VERIFY_ARE_EQUAL(L"grandparent", grandparentCommand.Name());

        auto grandparentActionAndArgs = grandparentCommand.ActionAndArgs();
        VERIFY_IS_NOT_NULL(grandparentActionAndArgs);
        VERIFY_ARE_EQUAL(ShortcutAction::Invalid, grandparentActionAndArgs.Action());

        VERIFY_ARE_EQUAL(1u, grandparentCommand.NestedCommands().Size());

        winrt::hstring parentName{ L"parent" };
        auto parent = grandparentCommand.NestedCommands().Lookup(parentName);
        VERIFY_IS_NOT_NULL(parent);
        auto parentActionAndArgs = parent.ActionAndArgs();
        VERIFY_IS_NOT_NULL(parentActionAndArgs);
        VERIFY_ARE_EQUAL(ShortcutAction::Invalid, parentActionAndArgs.Action());

        VERIFY_ARE_EQUAL(2u, parent.NestedCommands().Size());
        {
            winrt::hstring childName{ L"child1" };
            auto child = parent.NestedCommands().Lookup(childName);
            VERIFY_IS_NOT_NULL(child);
            auto childActionAndArgs = child.ActionAndArgs();
            VERIFY_IS_NOT_NULL(childActionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, childActionAndArgs.Action());
            const auto& realArgs = childActionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"ssh me@first.com", realArgs.TerminalArgs().Commandline());

            VERIFY_IS_FALSE(child.HasNestedCommands());
        }
        {
            winrt::hstring childName{ L"child2" };
            auto child = parent.NestedCommands().Lookup(childName);
            VERIFY_IS_NOT_NULL(child);
            auto childActionAndArgs = child.ActionAndArgs();
            VERIFY_IS_NOT_NULL(childActionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, childActionAndArgs.Action());
            const auto& realArgs = childActionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"ssh me@second.com", realArgs.TerminalArgs().Commandline());

            VERIFY_IS_FALSE(child.HasNestedCommands());
        }
    }

    void SettingsTests::TestNestedInIterableCommand()
    {
        // This test checks a iterable command that includes a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        //  ├─ profile0...
        //  |  ├─ Split pane, profile: profile0
        //  |  ├─ Split pane, direction: right, profile: profile0
        //  |  └─ Split pane, direction: down, profile: profile0
        //  ├─ profile1...
        //  |  ├─Split pane, profile: profile1
        //  |  ├─Split pane, direction: right, profile: profile1
        //  |  └─Split pane, direction: down, profile: profile1
        //  └─ profile2...
        //     ├─ Split pane, profile: profile2
        //     ├─ Split pane, direction: right, profile: profile2
        //     └─ Split pane, direction: down, profile: profile2

        static constexpr std::wstring_view settingsJson{ LR"(
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
            "actions": [
                {
                    "iterateOn": "profiles",
                    "name": "${profile.name}...",
                    "commands": [
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "auto" } },
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "right" } },
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "down" } }
                    ]
                }
            ]
        })" };

        CascadiaSettings settings{ settingsJson, inboxSettings };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        const std::vector<std::wstring> profileNames{ L"profile0", L"profile1", L"profile2" };
        for (auto i = 0u; i < profileNames.size(); i++)
        {
            const auto& name{ profileNames[i] };
            winrt::hstring commandName{ profileNames[i] + L"..." };

            auto command = expandedCommands.GetAt(i);
            VERIFY_ARE_EQUAL(commandName, command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::Invalid, actionAndArgs.Action());

            VERIFY_IS_TRUE(command.HasNestedCommands());
            VERIFY_ARE_EQUAL(3u, command.NestedCommands().Size());
            _logCommandNames(command.NestedCommands());
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.ActionAndArgs();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: down, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.ActionAndArgs();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitDirection::Down, realArgs.SplitDirection());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: right, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.ActionAndArgs();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
        }
    }

    void SettingsTests::TestIterableInNestedCommand()
    {
        // This test checks a nested command that includes an iterable command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ New Tab With Profile...
        //    ├─ Profile 1
        //    ├─ Profile 2
        //    └─ Profile 3

        static constexpr std::wstring_view settingsJson{ LR"(
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
            "actions": [
                {
                    "name": "New Tab With Profile...",
                    "commands": [
                        {
                            "iterateOn": "profiles",
                            "command": { "action": "newTab", "profile": "${profile.name}" }
                        }
                    ]
                }
            ]
        })" };

        CascadiaSettings settings{ settingsJson, inboxSettings };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommand = expandedCommands.GetAt(0);
        VERIFY_IS_NOT_NULL(rootCommand);
        VERIFY_ARE_EQUAL(L"New Tab With Profile...", rootCommand.Name());

        auto rootActionAndArgs = rootCommand.ActionAndArgs();
        VERIFY_IS_NOT_NULL(rootActionAndArgs);
        VERIFY_ARE_EQUAL(ShortcutAction::Invalid, rootActionAndArgs.Action());

        VERIFY_ARE_EQUAL(3u, rootCommand.NestedCommands().Size());

        for (auto name : std::vector<std::wstring>({ L"profile0", L"profile1", L"profile2" }))
        {
            winrt::hstring commandName{ fmt::format(L"New tab, profile: {}", name) };
            auto command = rootCommand.NestedCommands().Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

            VERIFY_IS_FALSE(command.HasNestedCommands());
        }
    }
    void SettingsTests::TestMixedNestedAndIterableCommand()
    {
        // This test checks a nested commands that includes an iterable command
        // that includes a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ New Pane...
        //    ├─ profile0...
        //    |  ├─ Split automatically
        //    |  ├─ Split right
        //    |  └─ Split down
        //    ├─ profile1...
        //    |  ├─ Split automatically
        //    |  ├─ Split right
        //    |  └─ Split down
        //    └─ profile2...
        //       ├─ Split automatically
        //       ├─ Split right
        //       └─ Split down

        static constexpr std::wstring_view settingsJson{ LR"(
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
            "actions": [
                {
                    "name": "New Pane...",
                    "commands": [
                        {
                            "iterateOn": "profiles",
                            "name": "${profile.name}...",
                            "commands": [
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "auto" } },
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "right" } },
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "down" } }
                            ]
                        }
                    ]
                }
            ]
        })" };

        CascadiaSettings settings{ settingsJson, inboxSettings };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommand = expandedCommands.GetAt(0);
        VERIFY_IS_NOT_NULL(rootCommand);
        VERIFY_ARE_EQUAL(L"New Pane...", rootCommand.Name());

        VERIFY_IS_NOT_NULL(rootCommand);
        auto rootActionAndArgs = rootCommand.ActionAndArgs();
        VERIFY_IS_NOT_NULL(rootActionAndArgs);
        VERIFY_ARE_EQUAL(ShortcutAction::Invalid, rootActionAndArgs.Action());

        VERIFY_ARE_EQUAL(3u, rootCommand.NestedCommands().Size());

        for (auto name : std::vector<std::wstring>({ L"profile0", L"profile1", L"profile2" }))
        {
            winrt::hstring commandName{ name + L"..." };
            auto command = rootCommand.NestedCommands().Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::Invalid, actionAndArgs.Action());

            VERIFY_IS_TRUE(command.HasNestedCommands());
            VERIFY_ARE_EQUAL(3u, command.NestedCommands().Size());

            _logCommandNames(command.NestedCommands());
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.ActionAndArgs();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: down, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.ActionAndArgs();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitDirection::Down, realArgs.SplitDirection());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: right, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.ActionAndArgs();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
        }
    }

    void SettingsTests::TestIterableColorSchemeCommands()
    {
        // For this test, put an iterable command with a given `name`,
        // containing a ${profile.name} to replace. When we expand it, it should
        // have created one command for each profile.

        static constexpr std::wstring_view settingsJson{ LR"(
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
            "schemes": [
                {
                    "name": "Campbell",
                    "foreground": "#CCCCCC",
                    "background": "#0C0C0C",
                    "cursorColor": "#FFFFFF",
                    "black": "#0C0C0C",
                    "red": "#C50F1F",
                    "green": "#13A10E",
                    "yellow": "#C19C00",
                    "blue": "#0037DA",
                    "purple": "#881798",
                    "cyan": "#3A96DD",
                    "white": "#CCCCCC",
                    "brightBlack": "#767676",
                    "brightRed": "#E74856",
                    "brightGreen": "#16C60C",
                    "brightYellow": "#F9F1A5",
                    "brightBlue": "#3B78FF",
                    "brightPurple": "#B4009E",
                    "brightCyan": "#61D6D6",
                    "brightWhite": "#F2F2F2"
                },
                {
                    "name": "Campbell PowerShell",
                    "foreground": "#CCCCCC",
                    "background": "#012456",
                    "cursorColor": "#FFFFFF",
                    "black": "#0C0C0C",
                    "red": "#C50F1F",
                    "green": "#13A10E",
                    "yellow": "#C19C00",
                    "blue": "#0037DA",
                    "purple": "#881798",
                    "cyan": "#3A96DD",
                    "white": "#CCCCCC",
                    "brightBlack": "#767676",
                    "brightRed": "#E74856",
                    "brightGreen": "#16C60C",
                    "brightYellow": "#F9F1A5",
                    "brightBlue": "#3B78FF",
                    "brightPurple": "#B4009E",
                    "brightCyan": "#61D6D6",
                    "brightWhite": "#F2F2F2"
                },
                {
                    "name": "Vintage",
                    "foreground": "#C0C0C0",
                    "background": "#000000",
                    "cursorColor": "#FFFFFF",
                    "black": "#000000",
                    "red": "#800000",
                    "green": "#008000",
                    "yellow": "#808000",
                    "blue": "#000080",
                    "purple": "#800080",
                    "cyan": "#008080",
                    "white": "#C0C0C0",
                    "brightBlack": "#808080",
                    "brightRed": "#FF0000",
                    "brightGreen": "#00FF00",
                    "brightYellow": "#FFFF00",
                    "brightBlue": "#0000FF",
                    "brightPurple": "#FF00FF",
                    "brightCyan": "#00FFFF",
                    "brightWhite": "#FFFFFF"
                }
            ],
            "actions": [
                {
                    "name": "iterable command ${scheme.name}",
                    "iterateOn": "schemes",
                    "command": { "action": "splitPane", "profile": "${scheme.name}" }
                },
            ]
        })" };

        CascadiaSettings settings{ settingsJson, {} };

        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        auto nameMap{ settings.ActionMap().NameMap() };
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            auto command = nameMap.TryLookup(L"iterable command ${scheme.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${scheme.name}", realArgs.TerminalArgs().Profile());
        }

        const auto& expandedCommands{ settings.GlobalSettings().ActionMap().ExpandedCommands() };
        _logCommands(expandedCommands);

        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        // Yes, this test is testing splitPane with profiles named after each
        // color scheme. These would obviously not work in real life, they're
        // just easy tests to write.

        {
            auto command = expandedCommands.GetAt(0);
            VERIFY_ARE_EQUAL(L"iterable command Campbell", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"Campbell", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.GetAt(1);
            VERIFY_ARE_EQUAL(L"iterable command Campbell PowerShell", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"Campbell PowerShell", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.GetAt(2);
            VERIFY_ARE_EQUAL(L"iterable command Vintage", command.Name());

            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Automatic, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"Vintage", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestElevateArg()
    {
        static constexpr std::wstring_view settingsJson{ LR"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "elevate": true,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "elevate": false,
                    "commandline": "wsl.exe"
                }
            ],
            "keybindings": [
                { "keys": ["ctrl+a"], "command": { "action": "newTab", "profile": "profile0" } },
                { "keys": ["ctrl+b"], "command": { "action": "newTab", "profile": "profile1" } },
                { "keys": ["ctrl+c"], "command": { "action": "newTab", "profile": "profile2" } },

                { "keys": ["ctrl+d"], "command": { "action": "newTab", "profile": "profile0", "elevate": false } },
                { "keys": ["ctrl+e"], "command": { "action": "newTab", "profile": "profile1", "elevate": false } },
                { "keys": ["ctrl+f"], "command": { "action": "newTab", "profile": "profile2", "elevate": false } },

                { "keys": ["ctrl+g"], "command": { "action": "newTab", "profile": "profile0", "elevate": true } },
                { "keys": ["ctrl+h"], "command": { "action": "newTab", "profile": "profile1", "elevate": true } },
                { "keys": ["ctrl+i"], "command": { "action": "newTab", "profile": "profile2", "elevate": true } },
            ]
        })" };

        const winrt::guid guid0{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid1{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid2{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}") };

        CascadiaSettings settings{ settingsJson, {} };

        auto keymap = settings.GlobalSettings().ActionMap();
        VERIFY_ARE_EQUAL(3u, settings.ActiveProfiles().Size());

        const auto profile2Guid = settings.ActiveProfiles().GetAt(2).Guid();
        VERIFY_ARE_NOT_EQUAL(winrt::guid{}, profile2Guid);

        VERIFY_ARE_EQUAL(9u, keymap.KeyBindings().Size());

        {
            Log::Comment(L"profile.elevate=omitted, action.elevate=nullopt: don't auto elevate");

            KeyChord kc{ true, false, false, false, static_cast<int32_t>('A'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NULL(realArgs.TerminalArgs().Elevate());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(false, termSettings.Elevate());
        }
        {
            Log::Comment(L"profile.elevate=true, action.elevate=nullopt: DO auto elevate");

            KeyChord kc{ true, false, false, false, static_cast<int32_t>('B'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NULL(realArgs.TerminalArgs().Elevate());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(true, termSettings.Elevate());
        }
        {
            Log::Comment(L"profile.elevate=false, action.elevate=nullopt: don't auto elevate");

            KeyChord kc{ true, false, false, false, static_cast<int32_t>('C'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NULL(realArgs.TerminalArgs().Elevate());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(false, termSettings.Elevate());
        }

        {
            Log::Comment(L"profile.elevate=omitted, action.elevate=false: don't auto elevate");

            KeyChord kc{ true, false, false, false, static_cast<int32_t>('D'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs().Elevate());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Elevate().Value());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(false, termSettings.Elevate());
        }
        {
            Log::Comment(L"profile.elevate=true, action.elevate=false: don't auto elevate");

            KeyChord kc{ true, false, false, false, static_cast<int32_t>('E'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs().Elevate());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Elevate().Value());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(false, termSettings.Elevate());
        }
        {
            Log::Comment(L"profile.elevate=false, action.elevate=false: don't auto elevate");

            KeyChord kc{ true, false, false, false, static_cast<int32_t>('F'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs().Elevate());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Elevate().Value());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(false, termSettings.Elevate());
        }

        {
            Log::Comment(L"profile.elevate=omitted, action.elevate=true: DO auto elevate");

            KeyChord kc{ true, false, false, false, static_cast<int32_t>('G'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs().Elevate());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Elevate().Value());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(true, termSettings.Elevate());
        }
        {
            Log::Comment(L"profile.elevate=true, action.elevate=true: DO auto elevate");
            KeyChord kc{ true, false, false, false, static_cast<int32_t>('H'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs().Elevate());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Elevate().Value());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(true, termSettings.Elevate());
        }
        {
            Log::Comment(L"profile.elevate=false, action.elevate=true: DO auto elevate");

            KeyChord kc{ true, false, false, false, static_cast<int32_t>('I'), 0 };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs().Elevate());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Elevate().Value());

            const auto termSettingsResult = TerminalSettings::CreateWithNewTerminalArgs(settings, realArgs.TerminalArgs(), nullptr);
            const auto termSettings = termSettingsResult.DefaultSettings();
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(true, termSettings.Elevate());
        }
    }

}
