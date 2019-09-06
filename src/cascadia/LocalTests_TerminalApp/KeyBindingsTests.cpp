// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppLocalTests
{
    // Unfortunately, these tests _WILL NOT_ work in our CI, until we have a lab
    // machine available that can run Windows version 18362.

    class KeyBindingsTests : public JsonTestClass
    {
        // Use a custom manifest to ensure that we can activate winrt types from
        // our test. This property will tell taef to manually use this as the
        // sxs manifest during this test class. It includes all the cppwinrt
        // types we've defined, so if your test is crashing for an unknown
        // reason, make sure it's included in that file.
        // If you want to do anything XAML-y, you'll need to run your test in a
        // packaged context. See TabTests.cpp for more details on that.
        BEGIN_TEST_CLASS(KeyBindingsTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.LocalTests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(ManyKeysSameAction);
        TEST_METHOD(LayerKeybindings);
        TEST_METHOD(UnbindKeybindings);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

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

        auto appKeyBindings = winrt::make_self<winrt::TerminalApp::implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings1Json);
        VERIFY_ARE_EQUAL(2u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings2Json);
        VERIFY_ARE_EQUAL(4u, appKeyBindings->_keyShortcuts.size());
    }

    void KeyBindingsTests::LayerKeybindings()
    {
        const std::string bindings0String{ R"([ { "command": "copy", "keys": ["ctrl+c"] } ])" };
        const std::string bindings1String{ R"([ { "command": "paste", "keys": ["ctrl+c"] } ])" };
        const std::string bindings2String{ R"([ { "command": "copy", "keys": ["enter"] } ])" };

        const auto bindings0Json = VerifyParseSucceeded(bindings0String);
        const auto bindings1Json = VerifyParseSucceeded(bindings1String);
        const auto bindings2Json = VerifyParseSucceeded(bindings2String);

        auto appKeyBindings = winrt::make_self<winrt::TerminalApp::implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings1Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings2Json);
        VERIFY_ARE_EQUAL(2u, appKeyBindings->_keyShortcuts.size());
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

        auto appKeyBindings = winrt::make_self<winrt::TerminalApp::implementation::AppKeyBindings>();
        VERIFY_IS_NOT_NULL(appKeyBindings);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        appKeyBindings->LayerJson(bindings1Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using `\"unbound\"` to unbind the key"));
        appKeyBindings->LayerJson(bindings2Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using `null` to unbind the key"));
        // First add back a good binding
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());
        // Then try layering in the bad setting
        appKeyBindings->LayerJson(bindings3Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using an unrecognized command to unbind the key"));
        // First add back a good binding
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());
        // Then try layering in the bad setting
        appKeyBindings->LayerJson(bindings4Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key using a straight up invalid value to unbind the key"));
        // First add back a good binding
        appKeyBindings->LayerJson(bindings0Json);
        VERIFY_ARE_EQUAL(1u, appKeyBindings->_keyShortcuts.size());
        // Then try layering in the bad setting
        appKeyBindings->LayerJson(bindings5Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());

        Log::Comment(NoThrowString().Format(
            L"Try unbinding a key that wasn't bound at all"));
        appKeyBindings->LayerJson(bindings2Json);
        VERIFY_ARE_EQUAL(0u, appKeyBindings->_keyShortcuts.size());
    }
}
