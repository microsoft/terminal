// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/Profile.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "../LocalTests_TerminalApp/JsonTestClass.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;

namespace TerminalAppUnitTests
{
    class JsonTests : public JsonTestClass
    {
        BEGIN_TEST_CLASS(JsonTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(ParseInvalidJson);
        TEST_METHOD(ParseSimpleColorScheme);
        TEST_METHOD(ProfileGeneratesGuid);

        TEST_METHOD(TestWrongValueType);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            // Use 4 spaces to indent instead of \t
            _builder.settings_["indentation"] = "    ";
            return true;
        }

        Json::Value VerifyParseSucceeded(std::string content);
        void VerifyParseFailed(std::string content);

    private:
        Json::StreamWriterBuilder _builder;
    };

    Json::Value JsonTests::VerifyParseSucceeded(std::string content)
    {
        Json::Value root;
        std::string errs;
        const bool parseResult = _reader->parse(content.c_str(), content.c_str() + content.size(), &root, &errs);
        VERIFY_IS_TRUE(parseResult, winrt::to_hstring(errs).c_str());
        return root;
    }
    void JsonTests::VerifyParseFailed(std::string content)
    {
        Json::Value root;
        std::string errs;
        const bool parseResult = _reader->parse(content.c_str(), content.c_str() + content.size(), &root, &errs);
        VERIFY_IS_FALSE(parseResult);
    }

    void JsonTests::ParseInvalidJson()
    {
        const std::string badJson{ "{ foo : bar : baz }" };
        VerifyParseFailed(badJson);
    }

    void JsonTests::ParseSimpleColorScheme()
    {
        const std::string campbellScheme{ "{"
                                          "\"background\" : \"#0C0C0C\","
                                          "\"black\" : \"#0C0C0C\","
                                          "\"blue\" : \"#0037DA\","
                                          "\"brightBlack\" : \"#767676\","
                                          "\"brightBlue\" : \"#3B78FF\","
                                          "\"brightCyan\" : \"#61D6D6\","
                                          "\"brightGreen\" : \"#16C60C\","
                                          "\"brightPurple\" : \"#B4009E\","
                                          "\"brightRed\" : \"#E74856\","
                                          "\"brightWhite\" : \"#F2F2F2\","
                                          "\"brightYellow\" : \"#F9F1A5\","
                                          "\"cursorColor\" : \"#FFFFFF\","
                                          "\"cyan\" : \"#3A96DD\","
                                          "\"foreground\" : \"#F2F2F2\","
                                          "\"green\" : \"#13A10E\","
                                          "\"name\" : \"Campbell\","
                                          "\"purple\" : \"#881798\","
                                          "\"red\" : \"#C50F1F\","
                                          "\"selectionBackground\" : \"#131313\","
                                          "\"white\" : \"#CCC\","
                                          "\"yellow\" : \"#C19C00\""
                                          "}" };

        const auto schemeObject = VerifyParseSucceeded(campbellScheme);
        auto scheme = ColorScheme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"Campbell", scheme.GetName());
        VERIFY_ARE_EQUAL(ARGB(0, 0xf2, 0xf2, 0xf2), scheme.GetForeground());
        VERIFY_ARE_EQUAL(ARGB(0, 0x0c, 0x0c, 0x0c), scheme.GetBackground());
        VERIFY_ARE_EQUAL(ARGB(0, 0x13, 0x13, 0x13), scheme.GetSelectionBackground());
        VERIFY_ARE_EQUAL(ARGB(0, 0xFF, 0xFF, 0xFF), scheme.GetCursorColor());

        std::array<COLORREF, COLOR_TABLE_SIZE> expectedCampbellTable;
        auto campbellSpan = gsl::span<COLORREF>(&expectedCampbellTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
        Utils::InitializeCampbellColorTable(campbellSpan);
        Utils::SetColorTableAlpha(campbellSpan, 0);

        for (size_t i = 0; i < expectedCampbellTable.size(); i++)
        {
            const auto& expected = expectedCampbellTable.at(i);
            const auto& actual = scheme.GetTable().at(i);
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    void JsonTests::ProfileGeneratesGuid()
    {
        // Parse some profiles without guids. We should NOT generate new guids
        // for them. If a profile doesn't have a GUID, we'll leave its _guid
        // set to nullopt. CascadiaSettings::_ValidateProfilesHaveGuid will
        // ensure all profiles have a GUID that's actually set.
        // The null guid _is_ a valid guid, so we won't re-generate that
        // guid. null is _not_ a valid guid, so we'll leave that nullopt

        // See SettingsTests::ValidateProfilesGenerateGuids for a version of
        // this test that includes synthesizing GUIDS for profiles without GUIDs
        // set

        const std::string profileWithoutGuid{ R"({
                                              "name" : "profile0"
                                              })" };
        const std::string secondProfileWithoutGuid{ R"({
                                              "name" : "profile1"
                                              })" };
        const std::string profileWithNullForGuid{ R"({
                                              "name" : "profile2",
                                              "guid" : null
                                              })" };
        const std::string profileWithNullGuid{ R"({
                                              "name" : "profile3",
                                              "guid" : "{00000000-0000-0000-0000-000000000000}"
                                              })" };
        const std::string profileWithGuid{ R"({
                                              "name" : "profile4",
                                              "guid" : "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
                                              })" };

        const auto profile0Json = VerifyParseSucceeded(profileWithoutGuid);
        const auto profile1Json = VerifyParseSucceeded(secondProfileWithoutGuid);
        const auto profile2Json = VerifyParseSucceeded(profileWithNullForGuid);
        const auto profile3Json = VerifyParseSucceeded(profileWithNullGuid);
        const auto profile4Json = VerifyParseSucceeded(profileWithGuid);

        const auto profile0 = Profile::FromJson(profile0Json);
        const auto profile1 = Profile::FromJson(profile1Json);
        const auto profile2 = Profile::FromJson(profile2Json);
        const auto profile3 = Profile::FromJson(profile3Json);
        const auto profile4 = Profile::FromJson(profile4Json);
        const GUID cmdGuid = Utils::GuidFromString(L"{6239a42c-1de4-49a3-80bd-e8fdd045185c}");
        const GUID nullGuid{ 0 };

        VERIFY_IS_FALSE(profile0._guid.has_value());
        VERIFY_IS_FALSE(profile1._guid.has_value());
        VERIFY_IS_FALSE(profile2._guid.has_value());
        VERIFY_IS_TRUE(profile3._guid.has_value());
        VERIFY_IS_TRUE(profile4._guid.has_value());

        VERIFY_ARE_EQUAL(profile3.GetGuid(), nullGuid);
        VERIFY_ARE_EQUAL(profile4.GetGuid(), cmdGuid);
    }

    void JsonTests::TestWrongValueType()
    {
        // This json blob has a whole bunch of settings with the wrong value
        // types - strings for int values, ints for strings, floats for ints,
        // etc. When we encounter data that's the wrong data type, we should
        // gracefully ignore it, as opposed to throwing an exception, causing us
        // to fail to load the settings at all.

        const std::string settings0String{ R"(
        {
            "defaultProfile" : "{00000000-1111-0000-0000-000000000000}",
            "profiles": [
                {
                    "guid" : "{00000000-1111-0000-0000-000000000000}",
                    "acrylicOpacity" : "0.5",
                    "closeOnExit" : "true",
                    "fontSize" : "10",
                    "historySize" : 1234.5678,
                    "padding" : 20,
                    "snapOnInput" : "false",
                    "icon" : 4,
                    "backgroundImageOpacity": false,
                    "useAcrylic" : 14
                }
            ]
        })" };

        const auto settings0Json = VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;

        settings._ParseJsonString(settings0String, false);
        // We should not throw an exception trying to parse the settings here.
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(1u, settings._profiles.size());
        auto& profile = settings._profiles.at(0);
        Profile defaults{};

        VERIFY_ARE_EQUAL(defaults._acrylicTransparency, profile._acrylicTransparency);
        VERIFY_ARE_EQUAL(defaults._closeOnExitMode, profile._closeOnExitMode);
        VERIFY_ARE_EQUAL(defaults._fontSize, profile._fontSize);
        VERIFY_ARE_EQUAL(defaults._historySize, profile._historySize);
        // A 20 as an int can still be treated as a json string
        VERIFY_ARE_EQUAL(L"20", profile._padding);
        VERIFY_ARE_EQUAL(defaults._snapOnInput, profile._snapOnInput);
        // 4 is a valid string value
        VERIFY_ARE_EQUAL(L"4", profile._icon);
        // false is not a valid optional<double>
        VERIFY_IS_FALSE(profile._backgroundImageOpacity.has_value());
        VERIFY_ARE_EQUAL(defaults._useAcrylic, profile._useAcrylic);
    }

}
