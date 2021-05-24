// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../types/inc/utils.hpp"
#include "../TerminalApp/TerminalPage.h"
#include "../TerminalApp/AppLogic.h"
#include "../TerminalApp/AppCommandlineArgs.h"
#include "../inc/WindowingBehavior.h"

using namespace WEX::Logging;
using namespace WEX::Common;
using namespace WEX::TestExecution;

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::TerminalApp;
using namespace ::TerminalApp;

namespace winrt
{
    namespace appImpl = TerminalApp::implementation;
}

namespace TerminalAppLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.
    class CommandlineTest
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(CommandlineTest)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(ParseSimpleCommandline);
        TEST_METHOD(ParseTrickyCommandlines);
        TEST_METHOD(TestEscapeDelimiters);

        TEST_METHOD(ParseSimpleHelp);
        TEST_METHOD(ParseBadOptions);
        TEST_METHOD(ParseSubcommandHelp);

        TEST_METHOD(ParseBasicCommandlineIntoArgs);
        TEST_METHOD(ParseNewTabCommand);
        TEST_METHOD(ParseSplitPaneIntoArgs);
        TEST_METHOD(ParseComboCommandlineIntoArgs);
        TEST_METHOD(ParseFocusTabArgs);
        TEST_METHOD(ParseMoveFocusArgs);
        TEST_METHOD(ParseArgumentsWithParsingTerminators);
        TEST_METHOD(ParseFocusPaneArgs);

        TEST_METHOD(ParseNoCommandIsNewTab);

        TEST_METHOD(ValidateFirstCommandIsNewTab);

        TEST_METHOD(CheckTypos);

        TEST_METHOD(TestSimpleExecuteCommandlineAction);
        TEST_METHOD(TestMultipleCommandExecuteCommandlineAction);
        TEST_METHOD(TestInvalidExecuteCommandlineAction);
        TEST_METHOD(TestLaunchMode);
        TEST_METHOD(TestLaunchModeWithNoCommand);

        TEST_METHOD(TestMultipleSplitPaneSizes);

        TEST_METHOD(TestFindTargetWindow);
        TEST_METHOD(TestFindTargetWindowHelp);
        TEST_METHOD(TestFindTargetWindowVersion);

    private:
        void _buildCommandlinesHelper(AppCommandlineArgs& appArgs,
                                      const size_t expectedSubcommands,
                                      std::vector<const wchar_t*>& rawCommands)
        {
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(expectedSubcommands, commandlines.size());
            for (auto& cmdBlob : commandlines)
            {
                const auto result = appArgs.ParseCommand(cmdBlob);
                VERIFY_ARE_EQUAL(0, result);
            }
            appArgs.ValidateStartupCommands();
        }

        void _buildCommandlinesExpectFailureHelper(AppCommandlineArgs& appArgs,
                                                   const size_t expectedSubcommands,
                                                   std::vector<const wchar_t*>& rawCommands)
        {
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(expectedSubcommands, commandlines.size());
            for (auto& cmdBlob : commandlines)
            {
                const auto result = appArgs.ParseCommand(cmdBlob);
                VERIFY_ARE_NOT_EQUAL(0, result);
                VERIFY_ARE_NOT_EQUAL("", appArgs._exitMessage);
                Log::Comment(NoThrowString().Format(
                    L"Exit Message:\n%hs",
                    appArgs._exitMessage.c_str()));
            }
        }

        void _logCommandline(std::vector<const wchar_t*>& rawCommands)
        {
            std::wstring buffer;
            for (const auto s : rawCommands)
            {
                buffer += s;
                buffer += L" ";
            }
            Log::Comment(NoThrowString().Format(L"%s", buffer.c_str()));
        }
    };

    void CommandlineTest::ParseSimpleCommandline()
    {
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"an arg with spaces" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--parameter", L"an arg with spaces" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(3u, commandlines.at(0).Argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L";" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));
            VERIFY_ARE_EQUAL("new-tab", commandlines.at(0).Args().at(1));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(1).Args().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L";" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(1).Args().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L";", L";" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(3u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(1).Args().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(2).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(2).Args().at(0));
        }
    }

    void CommandlineTest::ParseSimpleHelp()
    {
        static std::vector<std::vector<const wchar_t*>> commandsToTest{
            { L"wt.exe", L"/?" },
            { L"wt.exe", L"-?" },
            { L"wt.exe", L"-h" },
            { L"wt.exe", L"--help" }
        };
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:testPass", L"{0, 1, 2, 3}")
        END_TEST_METHOD_PROPERTIES();
        int testPass;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"testPass", testPass), L"Get a commandline to test");

        AppCommandlineArgs appArgs{};
        std::vector<const wchar_t*>& rawCommands{ commandsToTest.at(testPass) };
        _logCommandline(rawCommands);

        auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
        VERIFY_ARE_EQUAL(1u, commandlines.size());
        VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
        VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));

        for (auto& cmdBlob : commandlines)
        {
            const auto result = appArgs.ParseCommand(cmdBlob);
            Log::Comment(NoThrowString().Format(
                L"Exit Message:\n%hs",
                appArgs._exitMessage.c_str()));
            VERIFY_ARE_EQUAL(0, result);
            VERIFY_ARE_NOT_EQUAL("", appArgs._exitMessage);
        }
    }

    void CommandlineTest::ParseBadOptions()
    {
        static std::vector<std::vector<const wchar_t*>> commandsToTest{
            { L"wt.exe", L"/Z" },
            { L"wt.exe", L"-q" },
            { L"wt.exe", L"--bar" }
        };
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:testPass", L"{0, 1, 2}")
        END_TEST_METHOD_PROPERTIES();
        int testPass;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"testPass", testPass), L"Get a commandline to test");

        AppCommandlineArgs appArgs{};
        std::vector<const wchar_t*>& rawCommands{ commandsToTest.at(testPass) };
        _logCommandline(rawCommands);

        auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
        VERIFY_ARE_EQUAL(1u, commandlines.size());
        VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
        VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));

        for (auto& cmdBlob : commandlines)
        {
            const auto result = appArgs.ParseCommand(cmdBlob);
            Log::Comment(NoThrowString().Format(
                L"Exit Message:\n%hs",
                appArgs._exitMessage.c_str()));
            VERIFY_ARE_NOT_EQUAL(0, result);
            VERIFY_ARE_NOT_EQUAL("", appArgs._exitMessage);
        }
    }

    void CommandlineTest::ParseSubcommandHelp()
    {
        static std::vector<std::vector<const wchar_t*>> commandsToTest{
            { L"wt.exe", L"new-tab", L"-h" },
            { L"wt.exe", L"new-tab", L"--help" },
            { L"wt.exe", L"split-pane", L"-h" },
            { L"wt.exe", L"split-pane", L"--help" }
        };
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:testPass", L"{0, 1, 2, 3}")
        END_TEST_METHOD_PROPERTIES();
        int testPass;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"testPass", testPass), L"Get a commandline to test");

        AppCommandlineArgs appArgs{};
        std::vector<const wchar_t*>& rawCommands{ commandsToTest.at(testPass) };
        _logCommandline(rawCommands);

        auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
        VERIFY_ARE_EQUAL(1u, commandlines.size());
        VERIFY_ARE_EQUAL(3u, commandlines.at(0).Argc());
        VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));

        for (auto& cmdBlob : commandlines)
        {
            const auto result = appArgs.ParseCommand(cmdBlob);
            Log::Comment(NoThrowString().Format(
                L"Exit Message:\n%hs",
                appArgs._exitMessage.c_str()));
            VERIFY_ARE_EQUAL(0, result);
            VERIFY_ARE_NOT_EQUAL("", appArgs._exitMessage);
        }
    }

    void CommandlineTest::ParseTrickyCommandlines()
    {
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab;" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));
            VERIFY_ARE_EQUAL("new-tab", commandlines.at(0).Args().at(1));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(1).Args().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L";new-tab;" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(3u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));
            VERIFY_ARE_EQUAL(2u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(1).Args().at(0));
            VERIFY_ARE_EQUAL("new-tab", commandlines.at(1).Args().at(1));
            VERIFY_ARE_EQUAL(1u, commandlines.at(2).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(2).Args().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe;" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(1).Args().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe;;" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(3u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(1).Args().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(2).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(2).Args().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe;foo;bar;baz" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(4u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));
            VERIFY_ARE_EQUAL(2u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(1).Args().at(0));
            VERIFY_ARE_EQUAL("foo", commandlines.at(1).Args().at(1));
            VERIFY_ARE_EQUAL(2u, commandlines.at(2).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(2).Args().at(0));
            VERIFY_ARE_EQUAL("bar", commandlines.at(2).Args().at(1));
            VERIFY_ARE_EQUAL(2u, commandlines.at(3).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(3).Args().at(0));
            VERIFY_ARE_EQUAL("baz", commandlines.at(3).Args().at(1));
        }
    }

    void CommandlineTest::TestEscapeDelimiters()
    {
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"powershell.exe", L"This is an arg ; with spaces" };
            _buildCommandlinesHelper(appArgs, 2u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            {
                auto actionAndArgs = appArgs._startupActions.at(0);
                VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
                VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
                VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
                auto myCommand = myArgs.TerminalArgs().Commandline();
                VERIFY_ARE_EQUAL(L"powershell.exe \"This is an arg \"", myCommand);
            }
            {
                auto actionAndArgs = appArgs._startupActions.at(1);
                VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
                VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
                VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
                auto myCommand = myArgs.TerminalArgs().Commandline();
                VERIFY_ARE_EQUAL(L"\" with spaces\"", myCommand);
            }
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"powershell.exe", L"This is an arg \\; with spaces" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            {
                auto actionAndArgs = appArgs._startupActions.at(0);
                VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
                VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
                VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
                VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
                auto myCommand = myArgs.TerminalArgs().Commandline();
                VERIFY_ARE_EQUAL(L"powershell.exe \"This is an arg ; with spaces\"", myCommand);
            }
        }
    }

    void CommandlineTest::ParseBasicCommandlineIntoArgs()
    {
        AppCommandlineArgs appArgs{};
        std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab" };
        auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);

        _buildCommandlinesHelper(appArgs, 1u, rawCommands);

        VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
    }

    void CommandlineTest::ParseNewTabCommand()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:useShortForm", L"{false, true}")
        END_TEST_METHOD_PROPERTIES()

        INIT_TEST_PROPERTY(bool, useShortForm, L"If true, use `nt` instead of `new-tab`");
        const wchar_t* subcommand = useShortForm ? L"nt" : L"new-tab";

        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"--profile", L"cmd" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"cmd", myArgs.TerminalArgs().Profile());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"--startingDirectory", L"c:\\Foo" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\Foo", myArgs.TerminalArgs().StartingDirectory());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"powershell.exe" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"powershell.exe", myArgs.TerminalArgs().Commandline());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"powershell.exe", L"This is an arg with spaces" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            auto myCommand = myArgs.TerminalArgs().Commandline();
            VERIFY_ARE_EQUAL(L"powershell.exe \"This is an arg with spaces\"", myCommand);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"powershell.exe", L"This is an arg with spaces", L"another-arg", L"more spaces in this one" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            auto myCommand = myArgs.TerminalArgs().Commandline();
            VERIFY_ARE_EQUAL(L"powershell.exe \"This is an arg with spaces\" another-arg \"more spaces in this one\"", myCommand);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-p", L"Windows PowerShell" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"Windows PowerShell", myArgs.TerminalArgs().Profile());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"wsl", L"-d", L"Alpine" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-p", L"1", L"wsl", L"-d", L"Alpine" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"1", myArgs.TerminalArgs().Profile());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"--tabColor", L"#009999" };
            const auto expectedColor = ::Microsoft::Console::Utils::ColorFromHexString("#009999");

            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_ARE_EQUAL(til::color(myArgs.TerminalArgs().TabColor().Value()), expectedColor);
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"--colorScheme", L"Vintage" };
            const winrt::hstring expectedScheme{ L"Vintage" };

            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().ColorScheme().empty());
            VERIFY_ARE_EQUAL(expectedScheme, myArgs.TerminalArgs().ColorScheme());
        }
    }

    void CommandlineTest::ParseSplitPaneIntoArgs()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:useShortForm", L"{false, true}")
        END_TEST_METHOD_PROPERTIES()

        INIT_TEST_PROPERTY(bool, useShortForm, L"If true, use `sp` instead of `split-pane`");
        const wchar_t* subcommand = useShortForm ? L"sp" : L"split-pane";

        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            // The one we actually want to test here is the SplitPane action we created
            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
            VERIFY_ARE_EQUAL(SplitType::Manual, myArgs.SplitMode());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-H" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            // The one we actually want to test here is the SplitPane action we created
            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Horizontal, myArgs.SplitStyle());
            VERIFY_ARE_EQUAL(SplitType::Manual, myArgs.SplitMode());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-V" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            // The one we actually want to test here is the SplitPane action we created
            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Vertical, myArgs.SplitStyle());
            VERIFY_ARE_EQUAL(SplitType::Manual, myArgs.SplitMode());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-D" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            // The one we actually want to test here is the SplitPane action we created
            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitType::Duplicate, myArgs.SplitMode());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-p", L"1", L"wsl", L"-d", L"Alpine" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"1", myArgs.TerminalArgs().Profile());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-p", L"1", L"-H", L"wsl", L"-d", L"Alpine" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Horizontal, myArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"1", myArgs.TerminalArgs().Profile());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-p", L"1", L"wsl", L"-d", L"Alpine", L"-H" };
            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine -H", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"1", myArgs.TerminalArgs().Profile());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ColorScheme().empty());
        }
    }

    void CommandlineTest::ParseComboCommandlineIntoArgs()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:useShortFormNewTab", L"{false, true}")
            TEST_METHOD_PROPERTY(L"Data:useShortFormSplitPane", L"{false, true}")
        END_TEST_METHOD_PROPERTIES()

        INIT_TEST_PROPERTY(bool, useShortFormNewTab, L"If true, use `nt` instead of `new-tab`");
        INIT_TEST_PROPERTY(bool, useShortFormSplitPane, L"If true, use `sp` instead of `split-pane`");
        const wchar_t* ntSubcommand = useShortFormNewTab ? L"nt" : L"new-tab";
        const wchar_t* spSubcommand = useShortFormSplitPane ? L"sp" : L"split-pane";

        AppCommandlineArgs appArgs{};
        std::vector<const wchar_t*> rawCommands{ L"wt.exe", ntSubcommand, L";", spSubcommand };
        auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
        _buildCommandlinesHelper(appArgs, 2u, rawCommands);

        VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
        VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, appArgs._startupActions.at(1).Action());
    }

    void CommandlineTest::ParseNoCommandIsNewTab()
    {
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--profile", L"cmd" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"cmd", myArgs.TerminalArgs().Profile());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--startingDirectory", L"c:\\Foo" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\Foo", myArgs.TerminalArgs().StartingDirectory());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"powershell.exe" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"powershell.exe", myArgs.TerminalArgs().Commandline());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"powershell.exe", L"This is an arg with spaces" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"powershell.exe \"This is an arg with spaces\"", myArgs.TerminalArgs().Commandline());
        }
    }

    void CommandlineTest::ParseFocusTabArgs()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:useShortForm", L"{false, true}")
        END_TEST_METHOD_PROPERTIES()

        INIT_TEST_PROPERTY(bool, useShortForm, L"If true, use `ft` instead of `focus-tab`");
        const wchar_t* subcommand = useShortForm ? L"ft" : L"focus-tab";

        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-n" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::NextTab, actionAndArgs.Action());
            VERIFY_IS_NULL(actionAndArgs.Args());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-p" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::PrevTab, actionAndArgs.Action());
            VERIFY_IS_NULL(actionAndArgs.Args());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-t", L"2" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SwitchToTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SwitchToTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(static_cast<uint32_t>(2), myArgs.TabIndex());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Attempt an invalid combination of flags"));
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-p", L"-n" };

            auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(4u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL("wt.exe", commandlines.at(0).Args().at(0));

            for (auto& cmdBlob : commandlines)
            {
                const auto result = appArgs.ParseCommand(cmdBlob);
                Log::Comment(NoThrowString().Format(
                    L"Exit Message:\n%hs",
                    appArgs._exitMessage.c_str()));
                VERIFY_ARE_NOT_EQUAL(0, result);
                VERIFY_ARE_NOT_EQUAL("", appArgs._exitMessage);
            }
        }
    }

    void CommandlineTest::ParseMoveFocusArgs()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:useShortForm", L"{false, true}")
        END_TEST_METHOD_PROPERTIES()

        INIT_TEST_PROPERTY(bool, useShortForm, L"If true, use `mf` instead of `move-focus`");
        const wchar_t* subcommand = useShortForm ? L"mf" : L"move-focus";

        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand };
            Log::Comment(NoThrowString().Format(
                L"Just the subcommand, without a direction, should fail."));

            _buildCommandlinesExpectFailureHelper(appArgs, 1u, rawCommands);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"left" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::MoveFocus, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<MoveFocusArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(FocusDirection::Left, myArgs.FocusDirection());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"right" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::MoveFocus, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<MoveFocusArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(FocusDirection::Right, myArgs.FocusDirection());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"up" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::MoveFocus, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<MoveFocusArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(FocusDirection::Up, myArgs.FocusDirection());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"down" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::MoveFocus, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<MoveFocusArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(FocusDirection::Down, myArgs.FocusDirection());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"badDirection" };
            Log::Comment(NoThrowString().Format(
                L"move-focus with an invalid direction should fail."));
            _buildCommandlinesExpectFailureHelper(appArgs, 1u, rawCommands);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"left", L";", subcommand, L"right" };
            _buildCommandlinesHelper(appArgs, 2u, rawCommands);

            VERIFY_ARE_EQUAL(3u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::MoveFocus, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<MoveFocusArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(FocusDirection::Left, myArgs.FocusDirection());

            actionAndArgs = appArgs._startupActions.at(2);
            VERIFY_ARE_EQUAL(ShortcutAction::MoveFocus, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            myArgs = actionAndArgs.Args().try_as<MoveFocusArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(FocusDirection::Right, myArgs.FocusDirection());
        }
    }

    void CommandlineTest::ParseFocusPaneArgs()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:useShortForm", L"{false, true}")
        END_TEST_METHOD_PROPERTIES()

        INIT_TEST_PROPERTY(bool, useShortForm, L"If true, use `fp` instead of `focus-pane`");
        const wchar_t* subcommand = useShortForm ? L"fp" : L"focus-pane";

        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand };
            Log::Comment(NoThrowString().Format(
                L"Just the subcommand, without a target, should fail."));

            _buildCommandlinesExpectFailureHelper(appArgs, 1u, rawCommands);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"left" };

            Log::Comment(NoThrowString().Format(
                L"focus-pane without a  target should fail."));
            _buildCommandlinesExpectFailureHelper(appArgs, 1u, rawCommands);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"1" };

            Log::Comment(NoThrowString().Format(
                L"focus-pane without a target should fail."));
            _buildCommandlinesExpectFailureHelper(appArgs, 1u, rawCommands);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"--target", L"0" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::FocusPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<FocusPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(0u, myArgs.Id());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-t", L"100" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::FocusPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<FocusPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(100u, myArgs.Id());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"--target", L"-1" };
            Log::Comment(NoThrowString().Format(
                L"focus-pane with an invalid target should fail."));
            _buildCommandlinesExpectFailureHelper(appArgs, 1u, rawCommands);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"move-focus", L"left", L";", subcommand, L"-t", L"1" };
            _buildCommandlinesHelper(appArgs, 2u, rawCommands);

            VERIFY_ARE_EQUAL(3u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
            {
                auto actionAndArgs = appArgs._startupActions.at(1);
                VERIFY_ARE_EQUAL(ShortcutAction::MoveFocus, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<MoveFocusArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_ARE_EQUAL(FocusDirection::Left, myArgs.FocusDirection());
            }
            {
                auto actionAndArgs = appArgs._startupActions.at(2);
                VERIFY_ARE_EQUAL(ShortcutAction::FocusPane, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<FocusPaneArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_ARE_EQUAL(1u, myArgs.Id());
            }
        }
    }

    void CommandlineTest::ValidateFirstCommandIsNewTab()
    {
        AppCommandlineArgs appArgs{};
        std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"split-pane", L";", L"split-pane" };
        auto commandlines = AppCommandlineArgs::BuildCommands(rawCommands);
        _buildCommandlinesHelper(appArgs, 2u, rawCommands);

        VERIFY_ARE_EQUAL(3u, appArgs._startupActions.size());

        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
        VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, appArgs._startupActions.at(1).Action());
        VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, appArgs._startupActions.at(2).Action());
    }

    void CommandlineTest::CheckTypos()
    {
        Log::Comment(NoThrowString().Format(
            L"Check what happens when the user typos a subcommand. It should "
            L"be treated as a commandline"));

        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L";", L"slpit-pane" };
            _buildCommandlinesHelper(appArgs, 2u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());

            actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"slpit-pane", myArgs.TerminalArgs().Commandline());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Pass a flag that would be accepted by split-pane, but isn't accepted by new-tab"));
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"slpit-pane", L"-H" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);

            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(L"slpit-pane -H", myArgs.TerminalArgs().Commandline());
        }
    }

    void CommandlineTest::ParseArgumentsWithParsingTerminators()
    {
        { // one parsing terminator, new-tab command
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"-d", L"C:\\", L"--", L"wsl", L"-d", L"Alpine" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"C:\\", myArgs.TerminalArgs().StartingDirectory());
        }
        { // two parsing terminators, new-tab command
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"-d", L"C:\\", L"--", L"wsl", L"-d", L"Alpine", L"--", L"sleep", L"10" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine -- sleep 10", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"C:\\", myArgs.TerminalArgs().StartingDirectory());
        }
        { // two parsing terminators, *no* command
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"-d", L"C:\\", L"--", L"wsl", L"-d", L"Alpine", L"--", L"sleep", L"10" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine -- sleep 10", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"C:\\", myArgs.TerminalArgs().StartingDirectory());
        }
    }

    void CommandlineTest::TestSimpleExecuteCommandlineAction()
    {
        ExecuteCommandlineArgs args{ L"new-tab" };
        auto actions = winrt::TerminalApp::implementation::TerminalPage::ConvertExecuteCommandlineToActions(args);
        VERIFY_ARE_EQUAL(1u, actions.size());
        auto actionAndArgs = actions.at(0);
        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
        VERIFY_IS_NOT_NULL(actionAndArgs.Args());
        auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(myArgs);
        VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
        VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
        VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
        VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
        VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
        VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
    }

    void CommandlineTest::TestMultipleCommandExecuteCommandlineAction()
    {
        ExecuteCommandlineArgs args{ L"new-tab ; split-pane" };
        auto actions = winrt::TerminalApp::implementation::TerminalPage::ConvertExecuteCommandlineToActions(args);
        VERIFY_ARE_EQUAL(2u, actions.size());
        {
            auto actionAndArgs = actions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
        }
        {
            auto actionAndArgs = actions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_NULL(myArgs.TerminalArgs().TabColor());
            VERIFY_IS_NULL(myArgs.TerminalArgs().ProfileIndex());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
        }
    }

    void CommandlineTest::TestInvalidExecuteCommandlineAction()
    {
        // -H and -V cannot be combined.
        ExecuteCommandlineArgs args{ L"split-pane -H -V" };
        auto actions = winrt::TerminalApp::implementation::TerminalPage::ConvertExecuteCommandlineToActions(args);
        VERIFY_ARE_EQUAL(0u, actions.size());
    }

    void CommandlineTest::TestLaunchMode()
    {
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_FALSE(appArgs.GetLaunchMode().has_value());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"-F" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::FullscreenMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--fullscreen" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::FullscreenMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"-M" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::MaximizedMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--maximized" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::MaximizedMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"-f" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::FocusMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--focus" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::FocusMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"-fM" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::MaximizedFocusMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--maximized", L"--focus" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::MaximizedFocusMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--maximized", L"--focus", L"--focus" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::MaximizedFocusMode);
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--maximized", L"--focus", L"--maximized" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::MaximizedFocusMode);
        }
    }

    void CommandlineTest::TestLaunchModeWithNoCommand()
    {
        {
            Log::Comment(NoThrowString().Format(L"Pass a launch mode and profile"));

            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"-M", L"--profile", L"cmd" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::MaximizedMode);
            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"cmd", myArgs.TerminalArgs().Profile());
        }
        {
            Log::Comment(NoThrowString().Format(L"Pass a launch mode and command line"));

            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"-M", L"powershell.exe" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_IS_TRUE(appArgs.GetLaunchMode().has_value());
            VERIFY_ARE_EQUAL(appArgs.GetLaunchMode().value(), LaunchMode::MaximizedMode);
            VERIFY_ARE_EQUAL(1u, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"powershell.exe", myArgs.TerminalArgs().Commandline());
        }
    }

    void CommandlineTest::TestMultipleSplitPaneSizes()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:useShortForm", L"{false, true}")
        END_TEST_METHOD_PROPERTIES()

        INIT_TEST_PROPERTY(bool, useShortForm, L"If true, use `sp` instead of `split-pane`");
        const wchar_t* subcommand = useShortForm ? L"sp" : L"split-pane";

        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            // The one we actually want to test here is the SplitPane action we created
            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
            VERIFY_ARE_EQUAL(0.5f, myArgs.SplitSize());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-s", L".3" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(2u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            // The one we actually want to test here is the SplitPane action we created
            auto actionAndArgs = appArgs._startupActions.at(1);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
            VERIFY_ARE_EQUAL(0.3f, myArgs.SplitSize());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-s", L".3", L";", subcommand };
            _buildCommandlinesHelper(appArgs, 2u, rawCommands);

            VERIFY_ARE_EQUAL(3u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            {
                // The one we actually want to test here is the SplitPane action we created
                auto actionAndArgs = appArgs._startupActions.at(1);
                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
                VERIFY_ARE_EQUAL(0.3f, myArgs.SplitSize());
                VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            }
            {
                auto actionAndArgs = appArgs._startupActions.at(2);
                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
                VERIFY_ARE_EQUAL(0.5f, myArgs.SplitSize());
                VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            }
        }
        {
            AppCommandlineArgs appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", subcommand, L"-s", L".3", L";", subcommand, L"-s", L".7" };
            _buildCommandlinesHelper(appArgs, 2u, rawCommands);

            VERIFY_ARE_EQUAL(3u, appArgs._startupActions.size());

            // The first action is going to always be a new-tab action
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());

            {
                // The one we actually want to test here is the SplitPane action we created
                auto actionAndArgs = appArgs._startupActions.at(1);
                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
                VERIFY_ARE_EQUAL(0.3f, myArgs.SplitSize());
                VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            }
            {
                auto actionAndArgs = appArgs._startupActions.at(2);
                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
                VERIFY_IS_NOT_NULL(actionAndArgs.Args());
                auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(myArgs);
                VERIFY_ARE_EQUAL(SplitState::Automatic, myArgs.SplitStyle());
                VERIFY_ARE_EQUAL(0.7f, myArgs.SplitSize());
                VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
            }
        }
    }

    void CommandlineTest::TestFindTargetWindow()
    {
        {
            Log::Comment(L"wt.exe with no args should always use the value from"
                         L" the settings (passed as the second argument).");

            std::vector<winrt::hstring> args{ L"wt.exe" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseExisting, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseAnyExisting, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }
        {
            Log::Comment(L"-w -1 should always result in a new window");

            std::vector<winrt::hstring> args{ L"wt.exe", L"-w", L"-1" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }
        {
            Log::Comment(L"\"new\" should always result in a new window");

            std::vector<winrt::hstring> args{ L"wt.exe", L"-w", L"new" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }
        {
            Log::Comment(L"-w with a negative number should always result in a "
                         L"new window");

            std::vector<winrt::hstring> args{ L"wt.exe", L"-w", L"-12345" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }
        {
            Log::Comment(L"-w with a positive number should result in us trying"
                         L" to either make a new one or find an existing one "
                         L"with that ID, depending on the provided argument");

            std::vector<winrt::hstring> args{ L"wt.exe", L"-w", L"12345" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(12345, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(12345, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(12345, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }
        {
            Log::Comment(L"-w 0 should always use the \"current\" window");

            std::vector<winrt::hstring> args{ L"wt.exe", L"-w", L"0" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseCurrent, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseCurrent, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseCurrent, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }
        {
            Log::Comment(L"-w last should always use the most recent window on "
                         L"this desktop");

            std::vector<winrt::hstring> args{ L"wt.exe", L"-w", L"last" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseExisting, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseExisting, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseExisting, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }
        {
            Log::Comment(L"Make sure we follow the provided argument when a "
                         L"--window-id wasn't explicitly provided");

            std::vector<winrt::hstring> args{ L"wt.exe", L"new-tab" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseExisting, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseAnyExisting, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }
        {
            Log::Comment(L"Even if someone uses a subcommand as a window name, "
                         L"that should work");

            std::vector<winrt::hstring> args{ L"wt.exe", L"-w", L"new-tab" };
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseName, result.WindowId());
            VERIFY_ARE_EQUAL(L"new-tab", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseName, result.WindowId());
            VERIFY_ARE_EQUAL(L"new-tab", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseName, result.WindowId());
            VERIFY_ARE_EQUAL(L"new-tab", result.WindowName());
        }
    }

    void CommandlineTest::TestFindTargetWindowHelp()
    {
        Log::Comment(L"--help should always create a new window");

        // This is a little helper to make sure that these args _always_ return
        // UseNew, regardless of the windowing behavior.
        auto testHelper = [](auto&& args) {
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        };

        testHelper(std::vector<winrt::hstring>{ L"wt.exe", L"--help" });
        testHelper(std::vector<winrt::hstring>{ L"wt.exe", L"new-tab", L"--help" });
        testHelper(std::vector<winrt::hstring>{ L"wt.exe", L"-w", L"0", L"new-tab", L"--help" });
        testHelper(std::vector<winrt::hstring>{ L"wt.exe", L"-w", L"foo", L"new-tab", L"--help" });
        testHelper(std::vector<winrt::hstring>{ L"wt.exe", L"new-tab", L";", L"--help" });
    }

    void CommandlineTest::TestFindTargetWindowVersion()
    {
        Log::Comment(L"--version should always create a new window");

        // This is a little helper to make sure that these args _always_ return
        // UseNew, regardless of the windowing behavior.
        auto testHelper = [](auto&& args) {
            auto result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseNew);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());

            result = appImpl::AppLogic::_doFindTargetWindow({ args }, WindowingMode::UseAnyExisting);
            VERIFY_ARE_EQUAL(WindowingBehaviorUseNew, result.WindowId());
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        };

        testHelper(std::vector<winrt::hstring>{ L"wt.exe", L"--version" });
    }
}
