// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/Theme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../types/inc/colorTable.hpp"
#include "JsonTestClass.h"

#include <defaults.h>

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class ThemeTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(ThemeTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(ParseSimpleTheme);
        TEST_METHOD(ParseThemeWithNullThemeColor);

        // static Core::Color rgb(uint8_t r, uint8_t g, uint8_t b) noexcept
        // {
        //     return Core::Color{ r, g, b, 255 };
        // }
    };

    void ThemeTests::ParseSimpleTheme()
    {
        static constexpr std::string_view orangeTheme{ R"({
            "name": "orange",
            "tabRow":
            {
                "background": "#FFFF8800",
                "unfocusedBackground": "#FF884400"
            },
            "window":
            {
                "applicationTheme": "light",
                "useMica": true
            }
        })" };

        const auto schemeObject = VerifyParseSucceeded(orangeTheme);
        auto theme = Theme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"orange", theme->Name());
        // VERIFY_ARE_EQUAL(til::color(0xf2, 0xf2, 0xf2, 255), til::color{ scheme->Foreground() });
        // VERIFY_ARE_EQUAL(til::color(0x0c, 0x0c, 0x0c, 255), til::color{ scheme->Background() });
        // VERIFY_ARE_EQUAL(til::color(0x13, 0x13, 0x13, 255), til::color{ scheme->SelectionBackground() });
        // VERIFY_ARE_EQUAL(til::color(0xFF, 0xFF, 0xFF, 255), til::color{ scheme->CursorColor() });

        // std::array<COLORREF, COLOR_TABLE_SIZE> expectedCampbellTable;
        // const auto campbellSpan = gsl::make_span(expectedCampbellTable);
        // Utils::InitializeColorTable(campbellSpan);

        // for (size_t i = 0; i < expectedCampbellTable.size(); i++)
        // {
        //     const til::color expected{ expectedCampbellTable.at(i) };
        //     const til::color actual{ scheme->Table().at(static_cast<uint32_t>(i)) };
        //     VERIFY_ARE_EQUAL(expected, actual);
        // }

        // Log::Comment(L"Roundtrip Test for Color Scheme");
        // auto outJson{ scheme->ToJson() };
        // VERIFY_ARE_EQUAL(schemeObject, outJson);
    }

    // void ColorSchemeTests::LayerColorSchemesOnArray()
    // {
    //     static constexpr std::string_view inboxSettings{ R"({
    //         "schemes": [
    //             {
    //                 "background": "#0C0C0C",
    //                 "black": "#0C0C0C",
    //                 "blue": "#0037DA",
    //                 "brightBlack": "#767676",
    //                 "brightBlue": "#3B78FF",
    //                 "brightCyan": "#61D6D6",
    //                 "brightGreen": "#16C60C",
    //                 "brightPurple": "#B4009E",
    //                 "brightRed": "#E74856",
    //                 "brightWhite": "#F2F2F2",
    //                 "brightYellow": "#F9F1A5",
    //                 "cursorColor": "#FFFFFF",
    //                 "cyan": "#3A96DD",
    //                 "foreground": "#CCCCCC",
    //                 "green": "#13A10E",
    //                 "name": "Campbell",
    //                 "purple": "#881798",
    //                 "red": "#C50F1F",
    //                 "selectionBackground": "#FFFFFF",
    //                 "white": "#CCCCCC",
    //                 "yellow": "#C19C00"
    //             }
    //         ]
    //     })" };
    //     static constexpr std::string_view userSettings{ R"({
    //         "profiles": [
    //             {
    //                 "name" : "profile0"
    //             }
    //         ],
    //         "schemes": [
    //             {
    //                 "background": "#121314",
    //                 "black": "#121314",
    //                 "blue": "#121314",
    //                 "brightBlack": "#121314",
    //                 "brightBlue": "#121314",
    //                 "brightCyan": "#121314",
    //                 "brightGreen": "#121314",
    //                 "brightPurple": "#121314",
    //                 "brightRed": "#121314",
    //                 "brightWhite": "#121314",
    //                 "brightYellow": "#121314",
    //                 "cursorColor": "#121314",
    //                 "cyan": "#121314",
    //                 "foreground": "#121314",
    //                 "green": "#121314",
    //                 "name": "Campbell",
    //                 "purple": "#121314",
    //                 "red": "#121314",
    //                 "selectionBackground": "#121314",
    //                 "white": "#121314",
    //                 "yellow": "#121314"
    //             },
    //             {
    //                 "background": "#012456",
    //                 "black": "#0C0C0C",
    //                 "blue": "#0037DA",
    //                 "brightBlack": "#767676",
    //                 "brightBlue": "#3B78FF",
    //                 "brightCyan": "#61D6D6",
    //                 "brightGreen": "#16C60C",
    //                 "brightPurple": "#B4009E",
    //                 "brightRed": "#E74856",
    //                 "brightWhite": "#F2F2F2",
    //                 "brightYellow": "#F9F1A5",
    //                 "cursorColor": "#FFFFFF",
    //                 "cyan": "#3A96DD",
    //                 "foreground": "#CCCCCC",
    //                 "green": "#13A10E",
    //                 "name": "Campbell Powershell",
    //                 "purple": "#881798",
    //                 "red": "#C50F1F",
    //                 "selectionBackground": "#FFFFFF",
    //                 "white": "#CCCCCC",
    //                 "yellow": "#C19C00"
    //             }
    //         ]
    //     })" };

    //     const auto settings = winrt::make_self<CascadiaSettings>(userSettings, inboxSettings);

    //     const auto colorSchemes = settings->GlobalSettings().ColorSchemes();
    //     VERIFY_ARE_EQUAL(2u, colorSchemes.Size());

    //     const auto scheme0 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell"));
    //     VERIFY_ARE_EQUAL(rgb(0x12, 0x13, 0x14), scheme0->Foreground());
    //     VERIFY_ARE_EQUAL(rgb(0x12, 0x13, 0x14), scheme0->Background());

    //     const auto scheme1 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell Powershell"));
    //     VERIFY_ARE_EQUAL(rgb(0xCC, 0xCC, 0xCC), scheme1->Foreground());
    //     VERIFY_ARE_EQUAL(rgb(0x01, 0x24, 0x56), scheme1->Background());
    // }

    void ThemeTests::ParseThemeWithNullThemeColor()
    {
        static constexpr std::string_view settingsString{ R"json({
            "themes": [
                {
                    "name": "backgroundEmpty",
                    "tabRow":
                    {
                    },
                    "window":
                    {
                        "applicationTheme": "light",
                        "useMica": true
                    }
                },
            ]
        })json" };

        // {
        //     "name": "backgroundNull",
        //     "tabRow":
        //     {
        //         "background": null
        //     },
        //     "window":
        //     {
        //         "applicationTheme": "light",
        //         "useMica": true
        //     }
        // },
        // {
        //     "name": "backgroundOmittedEntirely",
        //     "window":
        //     {
        //         "applicationTheme": "light",
        //         "useMica": true
        //     }
        // }

        try
        {
            const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, DefaultJson) };

            const auto& themes{ settings->GlobalSettings().Themes() };
            {
                const auto& backgroundEmpty{ themes.Lookup(L"backgroundEmpty") };
                VERIFY_ARE_EQUAL(L"backgroundEmpty", backgroundEmpty.Name());
            }
            {
                const auto& backgroundNull{ themes.Lookup(L"backgroundNull") };
                VERIFY_ARE_EQUAL(L"backgroundNull", backgroundNull.Name());
            }
            {
                const auto& backgroundOmittedEntirely{ themes.Lookup(L"backgroundOmittedEntirely") };
                VERIFY_ARE_EQUAL(L"backgroundOmittedEntirely", backgroundOmittedEntirely.Name());
            }
        }
        catch (const SettingsException& ex)
        {
            auto loadError = ex.Error();
            loadError;
            DebugBreak();
            throw ex;
        }
        catch (const SettingsTypedDeserializationException& e)
        {
            auto deserializationErrorMessage = til::u8u16(e.what());
            Log::Comment(NoThrowString().Format(deserializationErrorMessage.c_str()));
            DebugBreak();
            throw e;
        }
    }
}
