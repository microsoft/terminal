/*
* Copyright (c) Microsoft Corporation.
* Licensed under the MIT license.
*
* Class Name: JsonTests
*/
#include "precomp.h"
#include "../../../inc/argb.h"
#include "../../../inc/conattrs.hpp"
#include "../../../types/inc/utils.hpp"
#include "../../../inc/DefaultSettings.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/Tab.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppUnitTests
{
    class JsonTests
    {
        TEST_CLASS(JsonTests);
        TEST_METHOD(ParseInvalidJson);
        TEST_METHOD(ParseSimpleColorScheme);
        TEST_METHOD(CreateDummyTab);

        TEST_CLASS_SETUP(ClassSetup)
        {
            reader = std::unique_ptr<Json::CharReader>(Json::CharReaderBuilder::CharReaderBuilder().newCharReader());
            DebugBreak();

            winrt::init_apartment(winrt::apartment_type::single_threaded);
            // Initialize the Xaml Hosting Manager
            auto manager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            _source = winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource{};
            return true;
        }

        Json::Value VerifyParseSucceeded(std::string content);
        void VerifyParseFailed(std::string content);

    private:
        std::unique_ptr<Json::CharReader> reader;
        winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _source{ nullptr };
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
        const std::string badJson000{ "{ foo : bar : baz }" };
        VerifyParseFailed(badJson000);
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
        // Utils::SetColorTableAlpha(campbellSpan, 0xff);
        for (size_t i = 0; i < expectedCampbellTable.size(); i++)
        {
            const auto& expected = expectedCampbellTable.at(i);
            const auto& actual = scheme.GetTable().at(i);
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    void JsonTests::CreateDummyTab()
    {
        const auto profileGuid{ Utils::CreateGuid() };
        winrt::Microsoft::Terminal::TerminalControl::TermControl term{ nullptr };

        auto newTab = std::make_shared<Tab>(profileGuid, term);

        VERIFY_IS_NOT_NULL(newTab);
    }
}
