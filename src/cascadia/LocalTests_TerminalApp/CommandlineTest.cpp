// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../TerminalApp/AppCommandline.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace winrt::TerminalApp;

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
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TerminalApp.LocalTests.AppxManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(TryCreateWinRTType);
        TEST_METHOD(ParseSimpleCommandline);
        TEST_METHOD(ParseTrickyCommandlines);
        TEST_METHOD(TestEscapeDelimiters);

        TEST_METHOD(ParseBasicCommandlineIntoArgs);
        TEST_METHOD(ParseNewTabCommand);
        TEST_METHOD(ParseSplitPaneIntoArgs);
        TEST_METHOD(ParseComboCommandlineIntoArgs);

        TEST_METHOD(ParseNoCommandIsNewTab);

    private:
        void _buildCommandlinesHelper(AppCommandline& appArgs,
                                      const size_t expectedSubcommands,
                                      std::vector<const wchar_t*>& rawCommands)
        {
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(expectedSubcommands, commandlines.size());
            for (auto& cmdBlob : commandlines)
            {
                cmdBlob.BuildArgv();
                const auto result = appArgs.ParseCommand(cmdBlob);
                VERIFY_ARE_EQUAL(0, result);
            }
        }
    };

    void CommandlineTest::TryCreateWinRTType()
    {
        ActionAndArgs newTabAction{};
        VERIFY_ARE_NOT_EQUAL(ShortcutAction::NewTab, newTabAction.Action());

        newTabAction.Action(ShortcutAction::NewTab);
        newTabAction.Args(NewTabArgs{});

        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, newTabAction.Action());
    }

    void CommandlineTest::ParseSimpleCommandline()
    {
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe" };
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"an arg with spaces" };
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--parameter", L"an arg with spaces" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(3u, commandlines.at(0).Argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L";" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(L"new-tab", commandlines.at(0).Wargs().at(1));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L";" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L";", L";" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(3u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(2).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(2).Wargs().at(0));
        }
    }

    void CommandlineTest::ParseTrickyCommandlines()
    {
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab;" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(L"new-tab", commandlines.at(0).Wargs().at(1));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L";new-tab;" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(3u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(2u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
            VERIFY_ARE_EQUAL(L"new-tab", commandlines.at(1).Wargs().at(1));
            VERIFY_ARE_EQUAL(1u, commandlines.at(2).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(2).Wargs().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe;" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe;;" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(3u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(2).Argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(2).Wargs().at(0));
        }
    }

    void CommandlineTest::TestEscapeDelimiters()
    {
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"powershell.exe", L"This is an arg ; with spaces" };
            _buildCommandlinesHelper(appArgs, 2u, rawCommands);

            VERIFY_ARE_EQUAL(2, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"powershell.exe", L"This is an arg \\; with spaces" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
        AppCommandline appArgs{};
        std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab" };
        auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());

        _buildCommandlinesHelper(appArgs, 1u, rawCommands);

        VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
    }

    void CommandlineTest::ParseNewTabCommand()
    {
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"--profile", L"cmd" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"--startingDirectory", L"c:\\Foo" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"powershell.exe" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"powershell.exe", L"This is an arg with spaces" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L"powershell.exe", L"This is an arg with spaces", L"another-arg", L"more spaces in this one" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
    }

    void CommandlineTest::ParseSplitPaneIntoArgs()
    {
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"split-pane" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Vertical, myArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"split-pane", L"-H" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Horizontal, myArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"split-pane", L"-V" };
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

            auto actionAndArgs = appArgs._startupActions.at(0);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            VERIFY_IS_NOT_NULL(actionAndArgs.Args());
            auto myArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(myArgs);
            VERIFY_ARE_EQUAL(SplitState::Vertical, myArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(myArgs.TerminalArgs());
        }
    }

    void CommandlineTest::ParseComboCommandlineIntoArgs()
    {
        AppCommandline appArgs{};
        std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L";", L"split-pane" };
        auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
        _buildCommandlinesHelper(appArgs, 2u, rawCommands);

        VERIFY_ARE_EQUAL(2, appArgs._startupActions.size());

        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
        VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, appArgs._startupActions.at(1).Action());
    }

    void CommandlineTest::ParseNoCommandIsNewTab()
    {
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--profile", L"cmd" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--startingDirectory", L"c:\\Foo" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"powershell.exe" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"powershell.exe", L"This is an arg with spaces" };
            _buildCommandlinesHelper(appArgs, 1u, rawCommands);

            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());

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

}
