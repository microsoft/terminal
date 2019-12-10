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
        TEST_METHOD(ParseSimmpleCommandline);

        TEST_METHOD(ParseBasicCommandlineIntoArgs);
        TEST_METHOD(ParseNewPaneIntoArgs);
        TEST_METHOD(ParseComboCommandlineIntoArgs);
    };

    void CommandlineTest::TryCreateWinRTType()
    {
        ActionAndArgs newTabAction{};
        VERIFY_ARE_NOT_EQUAL(ShortcutAction::NewTab, newTabAction.Action());

        newTabAction.Action(ShortcutAction::NewTab);
        newTabAction.Args(NewTabArgs{});

        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, newTabAction.Action());
    }

    void CommandlineTest::ParseSimmpleCommandline()
    {
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe" };
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"an arg with spaces" };
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"--parameter", L"an arg with spaces" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(3u, commandlines.at(0).argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).argc());
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L";" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(2u, commandlines.at(0).argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(L"new-tab", commandlines.at(0).Wargs().at(1));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L";" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(2u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
        }
        {
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L";", L";" };

            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(3u, commandlines.size());
            VERIFY_ARE_EQUAL(1u, commandlines.at(0).argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(0).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(1).argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(1).Wargs().at(0));
            VERIFY_ARE_EQUAL(1u, commandlines.at(2).argc());
            VERIFY_ARE_EQUAL(L"wt.exe", commandlines.at(2).Wargs().at(0));
        }
    }

    void CommandlineTest::ParseBasicCommandlineIntoArgs()
    {
        AppCommandline appArgs{};
        std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab" };
        auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
        VERIFY_ARE_EQUAL(1u, commandlines.size());
        for (auto& cmdBlob : commandlines)
        {
            cmdBlob.BuildArgv();
            const auto result = appArgs.ParseCommand(cmdBlob);
            VERIFY_ARE_EQUAL(0, result);
        }
        VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());
        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
    }

    void CommandlineTest::ParseNewPaneIntoArgs()
    {
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-pane" };
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            for (auto& cmdBlob : commandlines)
            {
                cmdBlob.BuildArgv();
                const auto result = appArgs.ParseCommand(cmdBlob);
                VERIFY_ARE_EQUAL(0, result);
            }
            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());
            VERIFY_ARE_EQUAL(ShortcutAction::SplitVertical, appArgs._startupActions.at(0).Action());
        }
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-pane", L"-H" };
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            for (auto& cmdBlob : commandlines)
            {
                cmdBlob.BuildArgv();
                const auto result = appArgs.ParseCommand(cmdBlob);
                VERIFY_ARE_EQUAL(0, result);
            }
            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());
            VERIFY_ARE_EQUAL(ShortcutAction::SplitHorizontal, appArgs._startupActions.at(0).Action());
        }
        {
            AppCommandline appArgs{};
            std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-pane", L"-V" };
            auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
            VERIFY_ARE_EQUAL(1u, commandlines.size());
            for (auto& cmdBlob : commandlines)
            {
                cmdBlob.BuildArgv();
                const auto result = appArgs.ParseCommand(cmdBlob);
                VERIFY_ARE_EQUAL(0, result);
            }
            VERIFY_ARE_EQUAL(1, appArgs._startupActions.size());
            VERIFY_ARE_EQUAL(ShortcutAction::SplitVertical, appArgs._startupActions.at(0).Action());
        }
    }

    void CommandlineTest::ParseComboCommandlineIntoArgs()
    {
        AppCommandline appArgs{};
        std::vector<const wchar_t*> rawCommands{ L"wt.exe", L"new-tab", L";", L"new-pane" };
        auto commandlines = AppCommandline::BuildCommands(static_cast<int>(rawCommands.size()), rawCommands.data());
        VERIFY_ARE_EQUAL(2u, commandlines.size());
        for (auto& cmdBlob : commandlines)
        {
            cmdBlob.BuildArgv();
            const auto result = appArgs.ParseCommand(cmdBlob);
            VERIFY_ARE_EQUAL(0, result);
        }
        VERIFY_ARE_EQUAL(2, appArgs._startupActions.size());
        VERIFY_ARE_EQUAL(ShortcutAction::NewTab, appArgs._startupActions.at(0).Action());
        VERIFY_ARE_EQUAL(ShortcutAction::SplitVertical, appArgs._startupActions.at(1).Action());
    }
}
