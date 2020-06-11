// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

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

    class CommandTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(CommandTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(ManyCommandsSameAction);
        TEST_METHOD(LayerCommand);
        TEST_METHOD(TestSplitPaneArgs);
        TEST_METHOD(TestResourceKeyName);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

    void CommandTests::ManyCommandsSameAction()
    {
        const std::string commands0String{ R"([ { "name":"action0", "action": "copy" } ])" };
        const std::string commands1String{ R"([ { "name":"action1", "action": { "action": "copy", "singleLine": false } } ])" };
        const std::string commands2String{ R"([
            { "name":"action2", "action": "paste" },
            { "name":"action3", "action": "paste" }
        ])" };

        const auto commands0Json = VerifyParseSucceeded(commands0String);
        const auto commands1Json = VerifyParseSucceeded(commands1String);
        const auto commands2Json = VerifyParseSucceeded(commands2String);

        std::map<winrt::hstring, Command> commands;
        VERIFY_ARE_EQUAL(0u, commands.size());
        {
            auto warnings = implementation::Command::LayerJson(commands, commands0Json);
            VERIFY_ARE_EQUAL(0u, warnings.size());
        }
        VERIFY_ARE_EQUAL(1u, commands.size());

        {
            auto warnings = implementation::Command::LayerJson(commands, commands1Json);
            VERIFY_ARE_EQUAL(0u, warnings.size());
        }
        VERIFY_ARE_EQUAL(2u, commands.size());

        {
            auto warnings = implementation::Command::LayerJson(commands, commands2Json);
            VERIFY_ARE_EQUAL(0u, warnings.size());
        }
        VERIFY_ARE_EQUAL(4u, commands.size());
    }

    void CommandTests::LayerCommand()
    {
        // Each one of the commands in this test should layer upon the previous, overriding the action.
        const std::string commands0String{ R"([ { "name":"action0", "action": "copy" } ])" };
        const std::string commands1String{ R"([ { "name":"action0", "action": "paste" } ])" };
        const std::string commands2String{ R"([ { "name":"action0", "action": "newTab" } ])" };
        const std::string commands3String{ R"([ { "name":"action0", "action": null } ])" };

        const auto commands0Json = VerifyParseSucceeded(commands0String);
        const auto commands1Json = VerifyParseSucceeded(commands1String);
        const auto commands2Json = VerifyParseSucceeded(commands2String);
        const auto commands3Json = VerifyParseSucceeded(commands3String);

        std::map<winrt::hstring, Command> commands;
        VERIFY_ARE_EQUAL(0u, commands.size());
        {
            auto warnings = implementation::Command::LayerJson(commands, commands0Json);
            VERIFY_ARE_EQUAL(0u, warnings.size());
            VERIFY_ARE_EQUAL(1u, commands.size());
            auto command = commands.at(L"action0");
            VERIFY_IS_NOT_NULL(command);
            VERIFY_IS_NOT_NULL(command.Action());
            VERIFY_ARE_EQUAL(ShortcutAction::CopyText, command.Action().Action());
            const auto& realArgs = command.Action().Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
        }
        {
            auto warnings = implementation::Command::LayerJson(commands, commands1Json);
            VERIFY_ARE_EQUAL(0u, warnings.size());
            VERIFY_ARE_EQUAL(1u, commands.size());
            auto command = commands.at(L"action0");
            VERIFY_IS_NOT_NULL(command);
            VERIFY_IS_NOT_NULL(command.Action());
            VERIFY_ARE_EQUAL(ShortcutAction::PasteText, command.Action().Action());
            VERIFY_IS_NULL(command.Action().Args());
        }
        {
            auto warnings = implementation::Command::LayerJson(commands, commands2Json);
            VERIFY_ARE_EQUAL(0u, warnings.size());
            VERIFY_ARE_EQUAL(1u, commands.size());
            auto command = commands.at(L"action0");
            VERIFY_IS_NOT_NULL(command);
            VERIFY_IS_NOT_NULL(command.Action());
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, command.Action().Action());
            const auto& realArgs = command.Action().Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
        }
        {
            // This last command should "unbind" the action.
            auto warnings = implementation::Command::LayerJson(commands, commands3Json);
            VERIFY_ARE_EQUAL(0u, warnings.size());
            VERIFY_ARE_EQUAL(0u, commands.size());
        }
    }

    void CommandTests::TestSplitPaneArgs()
    {
        // This is the same as KeyBindingsTests::TestSplitPaneArgs, but with
        // looking up the action and it's args from a map of commands, instead
        // of from keybindings.

        const std::string commands0String{ R"([
            { "name": "command0", "action": { "action": "splitPane", "split": null } },
            { "name": "command1", "action": { "action": "splitPane", "split": "vertical" } },
            { "name": "command2", "action": { "action": "splitPane", "split": "horizontal" } },
            { "name": "command3", "action": { "action": "splitPane", "split": "none" } },
            { "name": "command4", "action": { "action": "splitPane" } },
            { "name": "command5", "action": { "action": "splitPane", "split": "auto" } },
            { "name": "command6", "action": { "action": "splitPane", "split": "foo" } }
        ])" };

        const auto commands0Json = VerifyParseSucceeded(commands0String);

        std::map<winrt::hstring, Command> commands;
        VERIFY_ARE_EQUAL(0u, commands.size());
        auto warnings = implementation::Command::LayerJson(commands, commands0Json);
        VERIFY_ARE_EQUAL(0u, warnings.size());
        VERIFY_ARE_EQUAL(7u, commands.size());

        {
            auto command = commands.at(L"command0");
            VERIFY_IS_NOT_NULL(command);
            VERIFY_IS_NOT_NULL(command.Action());
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, command.Action().Action());
            const auto& realArgs = command.Action().Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
        }
        {
            auto command = commands.at(L"command1");
            VERIFY_IS_NOT_NULL(command);
            VERIFY_IS_NOT_NULL(command.Action());
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, command.Action().Action());
            const auto& realArgs = command.Action().Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
        }
    }
    void CommandTests::TestResourceKeyName()
    {
        // Each one of the commands in this test should layer upon the previous, overriding the action.
        const std::string commands0String{ R"([ { "name": { "key": "DuplicateTabCommandKey"}, "action": "copy" } ])" };
        const auto commands0Json = VerifyParseSucceeded(commands0String);

        std::map<winrt::hstring, Command> commands;
        VERIFY_ARE_EQUAL(0u, commands.size());
        {
            auto warnings = implementation::Command::LayerJson(commands, commands0Json);
            VERIFY_ARE_EQUAL(0u, warnings.size());
            VERIFY_ARE_EQUAL(1u, commands.size());

            // NOTE: We're relying on DuplicateTabCommandKey being defined as
            // "Duplicate Tab" here. If that string changes in our resources,
            // this test will break.
            auto command = commands.at(L"Duplicate Tab");
            VERIFY_IS_NOT_NULL(command);
            VERIFY_IS_NOT_NULL(command.Action());
            VERIFY_ARE_EQUAL(ShortcutAction::CopyText, command.Action().Action());
            const auto& realArgs = command.Action().Args().try_as<CopyTextArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
        }
    }

}
