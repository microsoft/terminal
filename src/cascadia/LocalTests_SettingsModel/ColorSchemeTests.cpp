// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../types/inc/colorTable.hpp"
#include "JsonTestClass.h"

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

    class ColorSchemeTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(ColorSchemeTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(ParseSimpleColorScheme);
        TEST_METHOD(LayerColorSchemesOnArray);
        TEST_METHOD(UpdateSchemeReferences);

        static Core::Color rgb(uint8_t r, uint8_t g, uint8_t b) noexcept
        {
            return Core::Color{ r, g, b, 255 };
        }
    };

    void ColorSchemeTests::ParseSimpleColorScheme()
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
        auto scheme = ColorScheme::FromJson(schemeObject);
        VERIFY_ARE_EQUAL(L"Campbell", scheme->Name());
        VERIFY_ARE_EQUAL(til::color(0xf2, 0xf2, 0xf2, 255), til::color{ scheme->Foreground() });
        VERIFY_ARE_EQUAL(til::color(0x0c, 0x0c, 0x0c, 255), til::color{ scheme->Background() });
        VERIFY_ARE_EQUAL(til::color(0x13, 0x13, 0x13, 255), til::color{ scheme->SelectionBackground() });
        VERIFY_ARE_EQUAL(til::color(0xFF, 0xFF, 0xFF, 255), til::color{ scheme->CursorColor() });

        std::array<COLORREF, COLOR_TABLE_SIZE> expectedCampbellTable;
        const auto campbellSpan = std::span{ expectedCampbellTable };
        Utils::InitializeColorTable(campbellSpan);

        for (size_t i = 0; i < expectedCampbellTable.size(); i++)
        {
            const til::color expected{ expectedCampbellTable.at(i) };
            const til::color actual{ scheme->Table().at(static_cast<uint32_t>(i)) };
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"Roundtrip Test for Color Scheme");
        auto outJson{ scheme->ToJson() };
        VERIFY_ARE_EQUAL(schemeObject, outJson);
    }

    void ColorSchemeTests::LayerColorSchemesOnArray()
    {
        static constexpr std::string_view inboxSettings{ R"({
            "schemes": [
                {
                    "background": "#0C0C0C",
                    "black": "#0C0C0C",
                    "blue": "#0037DA",
                    "brightBlack": "#767676",
                    "brightBlue": "#3B78FF",
                    "brightCyan": "#61D6D6",
                    "brightGreen": "#16C60C",
                    "brightPurple": "#B4009E",
                    "brightRed": "#E74856",
                    "brightWhite": "#F2F2F2",
                    "brightYellow": "#F9F1A5",
                    "cursorColor": "#FFFFFF",
                    "cyan": "#3A96DD",
                    "foreground": "#CCCCCC",
                    "green": "#13A10E",
                    "name": "Campbell",
                    "purple": "#881798",
                    "red": "#C50F1F",
                    "selectionBackground": "#FFFFFF",
                    "white": "#CCCCCC",
                    "yellow": "#C19C00"
                }
            ]
        })" };
        static constexpr std::string_view userSettings{ R"({
            "profiles": [
                {
                    "name" : "profile0"
                }
            ],
            "schemes": [
                {
                    "background": "#121314",
                    "black": "#121314",
                    "blue": "#121314",
                    "brightBlack": "#121314",
                    "brightBlue": "#121314",
                    "brightCyan": "#121314",
                    "brightGreen": "#121314",
                    "brightPurple": "#121314",
                    "brightRed": "#121314",
                    "brightWhite": "#121314",
                    "brightYellow": "#121314",
                    "cursorColor": "#121314",
                    "cyan": "#121314",
                    "foreground": "#121314",
                    "green": "#121314",
                    "name": "Campbell",
                    "purple": "#121314",
                    "red": "#121314",
                    "selectionBackground": "#121314",
                    "white": "#121314",
                    "yellow": "#121314"
                },
                {
                    "background": "#012456",
                    "black": "#0C0C0C",
                    "blue": "#0037DA",
                    "brightBlack": "#767676",
                    "brightBlue": "#3B78FF",
                    "brightCyan": "#61D6D6",
                    "brightGreen": "#16C60C",
                    "brightPurple": "#B4009E",
                    "brightRed": "#E74856",
                    "brightWhite": "#F2F2F2",
                    "brightYellow": "#F9F1A5",
                    "cursorColor": "#FFFFFF",
                    "cyan": "#3A96DD",
                    "foreground": "#CCCCCC",
                    "green": "#13A10E",
                    "name": "Campbell Powershell",
                    "purple": "#881798",
                    "red": "#C50F1F",
                    "selectionBackground": "#FFFFFF",
                    "white": "#CCCCCC",
                    "yellow": "#C19C00"
                }
            ]
        })" };

        const auto settings = winrt::make_self<CascadiaSettings>(userSettings, inboxSettings);

        const auto colorSchemes = settings->GlobalSettings().ColorSchemes();
        VERIFY_ARE_EQUAL(2u, colorSchemes.Size());

        const auto scheme0 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell"));
        VERIFY_ARE_EQUAL(rgb(0x12, 0x13, 0x14), scheme0->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x12, 0x13, 0x14), scheme0->Background());

        const auto scheme1 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell Powershell"));
        VERIFY_ARE_EQUAL(rgb(0xCC, 0xCC, 0xCC), scheme1->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x01, 0x24, 0x56), scheme1->Background());
    }

    void ColorSchemeTests::UpdateSchemeReferences()
    {
        static constexpr std::string_view settingsString{ R"json({
            "defaultProfile": "Inherited reference",
            "profiles": {
                "defaults": {
                    "colorScheme": "Campbell"
                },
                "list": [
                    {
                        "name": "Explicit scheme reference",
                        "colorScheme": "Campbell"
                    },
                    {
                        "name": "Explicit reference; hidden",
                        "colorScheme": "Campbell",
                        "hidden": true
                    },
                    {
                        "name": "Inherited reference"
                    },
                    {
                        "name": "Different reference",
                        "colorScheme": "One Half Dark"
                    },
                    {
                        "name": "rename neither",
                        "colorScheme":
                        {
                            "dark": "One Half Dark",
                            "light": "One Half Light"
                        }
                    },
                    {
                        "name": "rename only light",
                        "colorScheme":
                        {
                            "dark": "One Half Dark",
                            "light": "Campbell"
                        }
                    },
                    {
                        "name": "rename only dark",
                        "colorScheme":
                        {
                            "dark": "Campbell",
                            "light": "One Half Light"
                        }
                    }
                ]
            },
            "schemes": [
                {
                    "background": "#0C0C0C",
                    "black": "#0C0C0C",
                    "blue": "#0037DA",
                    "brightBlack": "#767676",
                    "brightBlue": "#3B78FF",
                    "brightCyan": "#61D6D6",
                    "brightGreen": "#16C60C",
                    "brightPurple": "#B4009E",
                    "brightRed": "#E74856",
                    "brightWhite": "#F2F2F2",
                    "brightYellow": "#F9F1A5",
                    "cursorColor": "#FFFFFF",
                    "cyan": "#3A96DD",
                    "foreground": "#CCCCCC",
                    "green": "#13A10E",
                    "name": "Campbell",
                    "purple": "#881798",
                    "red": "#C50F1F",
                    "selectionBackground": "#FFFFFF",
                    "white": "#CCCCCC",
                    "yellow": "#C19C00"
                },
                {
                    "background": "#0C0C0C",
                    "black": "#0C0C0C",
                    "blue": "#0037DA",
                    "brightBlack": "#767676",
                    "brightBlue": "#3B78FF",
                    "brightCyan": "#61D6D6",
                    "brightGreen": "#16C60C",
                    "brightPurple": "#B4009E",
                    "brightRed": "#E74856",
                    "brightWhite": "#F2F2F2",
                    "brightYellow": "#F9F1A5",
                    "cursorColor": "#FFFFFF",
                    "cyan": "#3A96DD",
                    "foreground": "#CCCCCC",
                    "green": "#13A10E",
                    "name": "Campbell (renamed)",
                    "purple": "#881798",
                    "red": "#C50F1F",
                    "selectionBackground": "#FFFFFF",
                    "white": "#CCCCCC",
                    "yellow": "#C19C00"
                },
                {
                    "background": "#282C34",
                    "black": "#282C34",
                    "blue": "#61AFEF",
                    "brightBlack": "#5A6374",
                    "brightBlue": "#61AFEF",
                    "brightCyan": "#56B6C2",
                    "brightGreen": "#98C379",
                    "brightPurple": "#C678DD",
                    "brightRed": "#E06C75",
                    "brightWhite": "#DCDFE4",
                    "brightYellow": "#E5C07B",
                    "cursorColor": "#FFFFFF",
                    "cyan": "#56B6C2",
                    "foreground": "#DCDFE4",
                    "green": "#98C379",
                    "name": "One Half Dark",
                    "purple": "#C678DD",
                    "red": "#E06C75",
                    "selectionBackground": "#FFFFFF",
                    "white": "#DCDFE4",
                    "yellow": "#E5C07B"
                },
                {
                    "name": "One Half Light",
                    "foreground": "#383A42",
                    "background": "#FAFAFA",
                    "cursorColor": "#4F525D",
                    "black": "#383A42",
                    "red": "#E45649",
                    "green": "#50A14F",
                    "yellow": "#C18301",
                    "blue": "#0184BC",
                    "purple": "#A626A4",
                    "cyan": "#0997B3",
                    "white": "#FAFAFA",
                    "brightBlack": "#4F525D",
                    "brightRed": "#DF6C75",
                    "brightGreen": "#98C379",
                    "brightYellow": "#E4C07A",
                    "brightBlue": "#61AFEF",
                    "brightPurple": "#C577DD",
                    "brightCyan": "#56B5C1",
                    "brightWhite": "#FFFFFF"
                }
            ]
        })json" };

        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString) };

        const auto newName{ L"Campbell (renamed)" };

        settings->UpdateColorSchemeReferences(L"Campbell", newName);

        VERIFY_ARE_EQUAL(newName, settings->ProfileDefaults().DefaultAppearance().DarkColorSchemeName());
        VERIFY_ARE_EQUAL(newName, settings->ProfileDefaults().DefaultAppearance().LightColorSchemeName());
        VERIFY_IS_TRUE(settings->ProfileDefaults().DefaultAppearance().HasDarkColorSchemeName());
        VERIFY_IS_TRUE(settings->ProfileDefaults().DefaultAppearance().HasLightColorSchemeName());

        const auto& profiles{ settings->AllProfiles() };
        {
            const auto& prof{ profiles.GetAt(0) };
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().DarkColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasLightColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(1) };
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().DarkColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasLightColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(2) };
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().DarkColorSchemeName());
            VERIFY_IS_FALSE(prof.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_FALSE(prof.DefaultAppearance().HasLightColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(3) };
            VERIFY_ARE_EQUAL(L"One Half Dark", prof.DefaultAppearance().DarkColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"One Half Dark", prof.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasLightColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(4) };
            VERIFY_ARE_EQUAL(L"One Half Dark", prof.DefaultAppearance().DarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"One Half Light", prof.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasLightColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(5) };
            VERIFY_ARE_EQUAL(L"One Half Dark", prof.DefaultAppearance().DarkColorSchemeName());
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasLightColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(6) };
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().DarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"One Half Light", prof.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasLightColorSchemeName());
        }
    }
}
