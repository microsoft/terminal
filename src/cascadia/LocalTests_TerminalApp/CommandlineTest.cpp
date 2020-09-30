// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../TerminalApp/TerminalPage.h"
#include "../TerminalApp/AppCommandlineArgs.h"
#include "../TerminalApp/ActionArgs.h"

using namespace WEX::Logging;
using namespace WEX::Common;
using namespace WEX::TestExecution;

using namespace winrt::TerminalApp;
using namespace ::TerminalApp;

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
        TEST_METHOD(ParseArgumentsWithParsingTerminators);

        TEST_METHOD(ParseNoCommandIsNewTab);

        TEST_METHOD(ValidateFirstCommandIsNewTab);

        TEST_METHOD(CheckTypos);

        TEST_METHOD(TestSimpleExecuteCommandlineAction);
        TEST_METHOD(TestMultipleCommandExecuteCommandlineAction);
        TEST_METHOD(TestInvalidExecuteCommandlineAction);

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
                VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
                VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
                VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"cmd", myArgs.TerminalArgs().Profile());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\Foo", myArgs.TerminalArgs().StartingDirectory());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"powershell.exe", myArgs.TerminalArgs().Commandline());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            auto myCommand = myArgs.TerminalArgs().Commandline();
            VERIFY_ARE_EQUAL(L"powershell.exe \"This is an arg with spaces\"", myCommand);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            auto myCommand = myArgs.TerminalArgs().Commandline();
            VERIFY_ARE_EQUAL(L"powershell.exe \"This is an arg with spaces\" another-arg \"more spaces in this one\"", myCommand);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"Windows PowerShell", myArgs.TerminalArgs().Profile());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"1", myArgs.TerminalArgs().Profile());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"1", myArgs.TerminalArgs().Profile());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"1", myArgs.TerminalArgs().Profile());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_FALSE(myArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"wsl -d Alpine -H", myArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"1", myArgs.TerminalArgs().Profile());
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
        auto args = winrt::make_self<implementation::ExecuteCommandlineArgs>();
        args->Commandline(L"new-tab");
        auto actions = implementation::TerminalPage::ConvertExecuteCommandlineToActions(*args);
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
        VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
        VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
    }

    void CommandlineTest::TestMultipleCommandExecuteCommandlineAction()
    {
        auto args = winrt::make_self<implementation::ExecuteCommandlineArgs>();
        args->Commandline(L"new-tab ; split-pane");
        auto actions = implementation::TerminalPage::ConvertExecuteCommandlineToActions(*args);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
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
            VERIFY_IS_TRUE(myArgs.TerminalArgs().ProfileIndex() == nullptr);
            VERIFY_IS_TRUE(myArgs.TerminalArgs().Profile().empty());
        }
    }

    void CommandlineTest::TestInvalidExecuteCommandlineAction()
    {
        auto args = winrt::make_self<implementation::ExecuteCommandlineArgs>();
        // -H and -V cannot be combined.
        args->Commandline(L"split-pane -H -V");
        auto actions = implementation::TerminalPage::ConvertExecuteCommandlineToActions(*args);
        VERIFY_ARE_EQUAL(0u, actions.size());
    }
}
