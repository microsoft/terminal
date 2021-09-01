// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/Profile.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../LocalTests_SettingsModel/JsonTestClass.h"
#include "../types/inc/colorTable.hpp"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

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

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            // Use 4 spaces to indent instead of \t
            _builder.settings_["indentation"] = "    ";
            return true;
        }

        Json::Value VerifyParseSucceeded(std::string_view content);
        void VerifyParseFailed(std::string_view content);

    private:
        Json::StreamWriterBuilder _builder;
    };

    Json::Value JsonTests::VerifyParseSucceeded(std::string_view content)
    {
        Json::Value root;
        std::string errs;
        const bool parseResult = _reader->parse(content.data(), content.data() + content.size(), &root, &errs);
        VERIFY_IS_TRUE(parseResult, winrt::to_hstring(errs).c_str());
        return root;
    }

    void JsonTests::VerifyParseFailed(std::string_view content)
    {
        Json::Value root;
        std::string errs;
        const bool parseResult = _reader->parse(content.data(), content.data() + content.size(), &root, &errs);
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
                                          "\"white\" : \"#CCCCCC\","
                                          "\"yellow\" : \"#C19C00\""
                                          "}" };

        const auto schemeObject = VerifyParseSucceeded(campbellScheme);
        auto scheme = implementation::ColorScheme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"Campbell", scheme->Name());
        VERIFY_ARE_EQUAL(til::color(0xf2, 0xf2, 0xf2, 255), til::color{ scheme->Foreground() });
        VERIFY_ARE_EQUAL(til::color(0x0c, 0x0c, 0x0c, 255), til::color{ scheme->Background() });
        VERIFY_ARE_EQUAL(til::color(0x13, 0x13, 0x13, 255), til::color{ scheme->SelectionBackground() });
        VERIFY_ARE_EQUAL(til::color(0xFF, 0xFF, 0xFF, 255), til::color{ scheme->CursorColor() });

        std::array<COLORREF, COLOR_TABLE_SIZE> expectedCampbellTable;
        auto campbellSpan = gsl::span<COLORREF>(&expectedCampbellTable[0], COLOR_TABLE_SIZE);
        Utils::InitializeCampbellColorTable(campbellSpan);
        Utils::SetColorTableAlpha(campbellSpan, 0);

        for (size_t i = 0; i < expectedCampbellTable.size(); i++)
        {
            const auto& expected = expectedCampbellTable.at(i);
            const til::color actual{ scheme->Table().at(static_cast<uint32_t>(i)) };
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"Roundtrip Test for Color Scheme");
        Json::Value outJson{ scheme->ToJson() };
        VERIFY_ARE_EQUAL(schemeObject, outJson);
    }

    void JsonTests::ProfileGeneratesGuid()
    {
        // Parse some profiles without guids. We should NOT generate new guids
        // for them. If a profile doesn't have a GUID, we'll leave its _guid
        // set to nullopt. The Profile::Guid() getter will
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

        const auto profile0 = implementation::Profile::FromJson(profile0Json);
        const auto profile1 = implementation::Profile::FromJson(profile1Json);
        const auto profile2 = implementation::Profile::FromJson(profile2Json);
        const auto profile3 = implementation::Profile::FromJson(profile3Json);
        const auto profile4 = implementation::Profile::FromJson(profile4Json);
        const winrt::guid cmdGuid = Utils::GuidFromString(L"{6239a42c-1de4-49a3-80bd-e8fdd045185c}");
        const winrt::guid nullGuid{};

        VERIFY_IS_FALSE(profile0->HasGuid());
        VERIFY_IS_FALSE(profile1->HasGuid());
        VERIFY_IS_FALSE(profile2->HasGuid());
        VERIFY_IS_TRUE(profile3->HasGuid());
        VERIFY_IS_TRUE(profile4->HasGuid());

        VERIFY_ARE_EQUAL(profile3->Guid(), nullGuid);
        VERIFY_ARE_EQUAL(profile4->Guid(), cmdGuid);
    }
}
