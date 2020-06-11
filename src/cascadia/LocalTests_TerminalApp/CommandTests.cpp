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

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

    void CommandTests::ManyCommandsSameAction()
    {
        const std::string commands0String{ R"([ { "name":"action0", "action": "copy" } ])" };
        const std::string commands1String{ R"([ { "name":"action1", "action": "copy" } ])" };
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
}
