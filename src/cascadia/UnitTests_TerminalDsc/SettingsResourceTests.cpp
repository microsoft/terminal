// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalDsc/Resource/JsonUtils.h"
#include "../TerminalDsc/Resources/Settings/SettingsResource.h"

using namespace Microsoft::Console;
using namespace Microsoft::Terminal::Dsc;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings;

namespace TerminalDscUnitTests
{
    class SettingsResourceTests
    {
        TEST_CLASS(SettingsResourceTests);

        TEST_METHOD(ReadStateReportsConfiguredValues);
        TEST_METHOD(ReadStateReportsThemePair);
        TEST_METHOD(ReadStateReportsDefaultTheme);
        TEST_METHOD(ApplyStateUpdatesSettings);
        TEST_METHOD(ApplyStateWithEmptyObjectChangesNothing);
        TEST_METHOD(ApplyStateRejectsUnknownProperties);
        TEST_METHOD(ApplyStateRejectsWrongTypes);
        TEST_METHOD(SerializedJsonIsASingleLine);
        TEST_METHOD(ParseJsonRejectsMalformedInput);

    private:
        // The settings loader rejects a document without profiles, so wrap the
        // globals under test in a minimal valid settings document.
        static winrt::com_ptr<implementation::CascadiaSettings> createSettings(const std::string_view& globalsJSON)
        {
            std::string json{ R"({ "profiles": [ { "name": "profile0" } ])" };
            if (!globalsJSON.empty())
            {
                json += ", ";
                json += globalsJSON;
            }
            json += " }";
            return winrt::make_self<implementation::CascadiaSettings>(json);
        }
    };

    void SettingsResourceTests::ReadStateReportsConfiguredValues()
    {
        // The referenced theme must exist, or settings validation resets the
        // setting to "system" (the built-in themes live in the defaults.json
        // layer, which these unit tests do not load). User themes with the
        // reserved built-in names are ignored, so use a distinct name.
        static constexpr std::string_view userSettings{ R"(
            "themes": [ { "name": "testLight" } ],
            "theme": "testLight",
            "copyOnSelect": true,
            "initialCols": 42
        )" };

        const auto settings{ createSettings(userSettings) };
        const auto state{ SettingsResource::ReadState(settings->GlobalSettings()) };

        VERIFY_ARE_EQUAL(std::string{ "testLight" }, state["theme"].asString());
        VERIFY_IS_TRUE(state["copyOnSelect"].asBool());
        VERIFY_ARE_EQUAL(42, state["initialCols"].asInt());
    }

    void SettingsResourceTests::ReadStateReportsThemePair()
    {
        static constexpr std::string_view userSettings{ R"(
            "themes": [ { "name": "testDark" }, { "name": "testLight" } ],
            "theme": { "dark": "testDark", "light": "testLight" }
        )" };

        const auto settings{ createSettings(userSettings) };
        const auto state{ SettingsResource::ReadState(settings->GlobalSettings()) };

        VERIFY_IS_TRUE(state["theme"].isObject());
        VERIFY_ARE_EQUAL(std::string{ "testDark" }, state["theme"]["dark"].asString());
        VERIFY_ARE_EQUAL(std::string{ "testLight" }, state["theme"]["light"].asString());
    }

    void SettingsResourceTests::ReadStateReportsDefaultTheme()
    {
        const auto settings{ createSettings("") };
        const auto state{ SettingsResource::ReadState(settings->GlobalSettings()) };

        // When no theme is configured, settings validation falls back to
        // "system"; the scalar settings always have a value.
        VERIFY_ARE_EQUAL(std::string{ "system" }, state["theme"].asString());
        VERIFY_IS_TRUE(state.isMember("copyOnSelect"));
        VERIFY_IS_TRUE(state.isMember("initialCols"));
    }

    void SettingsResourceTests::ApplyStateUpdatesSettings()
    {
        const auto settings{ createSettings("") };
        const auto globals{ settings->GlobalSettings() };

        const auto desired{ ParseJson(R"({ "copyOnSelect": true, "initialCols": 200 })") };
        VERIFY_IS_TRUE(SettingsResource::ApplyState(globals, desired));

        VERIFY_IS_TRUE(globals.CopyOnSelect());
        VERIFY_ARE_EQUAL(200, globals.InitialCols());

        const auto state{ SettingsResource::ReadState(globals) };
        VERIFY_IS_TRUE(state["copyOnSelect"].asBool());
        VERIFY_ARE_EQUAL(200, state["initialCols"].asInt());
    }

    void SettingsResourceTests::ApplyStateWithEmptyObjectChangesNothing()
    {
        const auto settings{ createSettings("") };
        VERIFY_IS_FALSE(SettingsResource::ApplyState(settings->GlobalSettings(), ParseJson("{}")));
    }

    void SettingsResourceTests::ApplyStateRejectsUnknownProperties()
    {
        const auto settings{ createSettings("") };
        const auto globals{ settings->GlobalSettings() };

        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"({ "initialRows": 5 })")), const DscInputError, [](const auto&) { return true; });
        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"([ "not", "an", "object" ])")), const DscInputError, [](const auto&) { return true; });
    }

    void SettingsResourceTests::ApplyStateRejectsWrongTypes()
    {
        const auto settings{ createSettings("") };
        const auto globals{ settings->GlobalSettings() };

        // None of these throw until after validation, so the settings object is never touched.
        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"({ "copyOnSelect": "yes" })")), const DscInputError, [](const auto&) { return true; });
        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"({ "initialCols": 0 })")), const DscInputError, [](const auto&) { return true; });
        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"({ "initialCols": 1000 })")), const DscInputError, [](const auto&) { return true; });
        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"({ "initialCols": true })")), const DscInputError, [](const auto&) { return true; });
        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"({ "theme": 42 })")), const DscInputError, [](const auto&) { return true; });
        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"({ "theme": { "dark": "dark" } })")), const DscInputError, [](const auto&) { return true; });
        VERIFY_THROWS_SPECIFIC(SettingsResource::ApplyState(globals, ParseJson(R"({ "theme": { "dark": "dark", "light": "light", "other": "x" } })")), const DscInputError, [](const auto&) { return true; });
    }

    void SettingsResourceTests::SerializedJsonIsASingleLine()
    {
        const auto text{ SerializeJson(SettingsResource::StateSchema()) };
        VERIFY_ARE_EQUAL(std::string::npos, text.find('\n'));
    }

    void SettingsResourceTests::ParseJsonRejectsMalformedInput()
    {
        VERIFY_THROWS_SPECIFIC(ParseJson("{ not json"), const DscInputError, [](const auto&) { return true; });
    }
}
