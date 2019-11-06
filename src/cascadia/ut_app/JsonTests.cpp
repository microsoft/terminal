// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/Profile.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppUnitTests
{
    class JsonTests
    {
        BEGIN_TEST_CLASS(JsonTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(ParseInvalidJson);
        TEST_METHOD(ParseSimpleColorScheme);
        TEST_METHOD(ProfileGeneratesGuid);
        TEST_METHOD(DiffProfile);
        TEST_METHOD(DiffProfileWithNull);

        TEST_CLASS_SETUP(ClassSetup)
        {
            reader = std::unique_ptr<Json::CharReader>(Json::CharReaderBuilder::CharReaderBuilder().newCharReader());

            // Use 4 spaces to indent instead of \t
            _builder.settings_["indentation"] = "    ";
            return true;
        }

        Json::Value VerifyParseSucceeded(std::string content);
        void VerifyParseFailed(std::string content);

    private:
        std::unique_ptr<Json::CharReader> reader;
        Json::StreamWriterBuilder _builder;
    };

    Json::Value JsonTests::VerifyParseSucceeded(std::string content)
    {
        Json::Value root;
        std::string errs;
        const bool parseResult = reader->parse(content.c_str(), content.c_str() + content.size(), &root, &errs);
        VERIFY_IS_TRUE(parseResult, winrt::to_hstring(errs).c_str());
        return root;
    }
    void JsonTests::VerifyParseFailed(std::string content)
    {
        Json::Value root;
        std::string errs;
        const bool parseResult = reader->parse(content.c_str(), content.c_str() + content.size(), &root, &errs);
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

    void JsonTests::DiffProfile()
    {
        Profile profile0;
        Profile profile1;

        Log::Comment(NoThrowString().Format(
            L"Both these profiles are the same, their diff should have _no_ values"));

        auto diff = profile1.DiffToJson(profile0);

        Log::Comment(NoThrowString().Format(L"diff:%hs", Json::writeString(_builder, diff).c_str()));

        VERIFY_ARE_EQUAL(0u, diff.getMemberNames().size());

        profile1._name = L"profile1";
        diff = profile1.DiffToJson(profile0);
        Log::Comment(NoThrowString().Format(L"diff:%hs", Json::writeString(_builder, diff).c_str()));
        VERIFY_ARE_EQUAL(1u, diff.getMemberNames().size());
    }

    void JsonTests::DiffProfileWithNull()
    {
        Profile profile0;
        Profile profile1;

        profile0._icon = L"foo";

        Log::Comment(NoThrowString().Format(
            L"Case 1: Base object has an optional that the derived does not - diff will have null for that value"));
        auto diff = profile1.DiffToJson(profile0);

        Log::Comment(NoThrowString().Format(L"diff:%hs", Json::writeString(_builder, diff).c_str()));

        VERIFY_ARE_EQUAL(1u, diff.getMemberNames().size());
        VERIFY_IS_TRUE(diff.isMember("icon"));
        VERIFY_IS_TRUE(diff["icon"].isNull());

        Log::Comment(NoThrowString().Format(
            L"Case 2: Add an optional to the derived object that's not present in the root."));

        profile0._icon = std::nullopt;
        profile1._icon = L"bar";

        diff = profile1.DiffToJson(profile0);

        Log::Comment(NoThrowString().Format(L"diff:%hs", Json::writeString(_builder, diff).c_str()));

        VERIFY_ARE_EQUAL(1u, diff.getMemberNames().size());
        VERIFY_IS_TRUE(diff.isMember("icon"));
        VERIFY_IS_TRUE(diff["icon"].isString());
        VERIFY_IS_TRUE("bar" == diff["icon"].asString());
    }

}
