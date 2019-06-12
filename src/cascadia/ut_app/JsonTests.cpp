// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppUnitTests
{
    class JsonTests
    {
        BEGIN_TEST_CLASS(JsonTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(ParseInvalidJson);
        TEST_METHOD(ParseSimpleColorScheme);

        TEST_CLASS_SETUP(ClassSetup)
        {
            reader = std::unique_ptr<Json::CharReader>(Json::CharReaderBuilder::CharReaderBuilder().newCharReader());
            return true;
        }

        Json::Value VerifyParseSucceeded(std::string content);
        void VerifyParseFailed(std::string content);

    private:
        std::unique_ptr<Json::CharReader> reader;
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
                                          "\"white\" : \"#CCCCCC\","
                                          "\"yellow\" : \"#C19C00\""
                                          "}" };

        const auto schemeObject = VerifyParseSucceeded(campbellScheme);
        auto scheme = ColorScheme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"Campbell", scheme.GetName());
        VERIFY_ARE_EQUAL(ARGB(0, 0xf2, 0xf2, 0xf2), scheme.GetForeground());
        VERIFY_ARE_EQUAL(ARGB(0, 0x0c, 0x0c, 0x0c), scheme.GetBackground());

        std::array<COLORREF, COLOR_TABLE_SIZE> expectedCampbellTable;
        auto campbellSpan = gsl::span<COLORREF>(&expectedCampbellTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
        Utils::InitializeCampbellColorTable(campbellSpan);

        for (size_t i = 0; i < expectedCampbellTable.size(); i++)
        {
            const auto& expected = expectedCampbellTable.at(i);
            const auto& actual = scheme.GetTable().at(i);
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }
}
