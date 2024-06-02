// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../types/inc/colorTable.hpp"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class ColorSchemeTests : public JsonTestClass
    {
        TEST_CLASS(ColorSchemeTests);

        TEST_METHOD(ParseSimpleColorScheme);
        TEST_METHOD(LayerColorSchemesOnArray);
        TEST_METHOD(UpdateSchemeReferences);

        TEST_METHOD(LayerColorSchemesWithUserOwnedCollision);
        TEST_METHOD(LayerColorSchemesWithUserOwnedCollisionRetargetsAllProfiles);
        TEST_METHOD(LayerColorSchemesWithUserOwnedCollisionWithFragments);
        TEST_METHOD(LayerColorSchemesWithUserOwnedMultipleCollisions);

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
        VERIFY_ARE_EQUAL(rgb(0xCC, 0xCC, 0xCC), scheme0->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x0C, 0x0C, 0x0C), scheme0->Background());

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

    void ColorSchemeTests::LayerColorSchemesWithUserOwnedCollision()
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
                },
                {
                    "name": "Antique",
                    "foreground": "#C0C0C0",
                    "background": "#000000",
                    "cursorColor": "#FFFFFF",
                    "black": "#000000",
                    "red": "#800000",
                    "green": "#008000",
                    "yellow": "#808000",
                    "blue": "#000080",
                    "purple": "#800080",
                    "cyan": "#008080",
                    "white": "#C0C0C0",
                    "brightBlack": "#808080",
                    "brightRed": "#FF0000",
                    "brightGreen": "#00FF00",
                    "brightYellow": "#FFFF00",
                    "brightBlue": "#0000FF",
                    "brightPurple": "#FF00FF",
                    "brightCyan": "#00FFFF",
                    "brightWhite": "#FFFFFF"
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
                    "name": "Antique",
                    "foreground": "#C0C0C0",
                    "background": "#000000",
                    "cursorColor": "#FFFFFF",
                    "black": "#000000",
                    "red": "#800000",
                    "green": "#008000",
                    "yellow": "#808000",
                    "blue": "#000080",
                    "purple": "#800080",
                    "cyan": "#008080",
                    "white": "#C0C0C0",
                    "brightBlack": "#808080",
                    "brightRed": "#FF0000",
                    "brightGreen": "#00FF00",
                    "brightYellow": "#FFFF00",
                    "brightBlue": "#0000FF",
                    "brightPurple": "#FF00FF",
                    "brightCyan": "#00FFFF",
                    "brightWhite": "#FFFFFF"
                }
            ]
        })" };

        // In this test, The user has a copy of Campbell which they have modified and a copy of Antique which they
        // have not. Campbell should be renamed to Campbell (modified) and copied while Antique should simply
        // be demoted to "Inbox" status.

        const auto settings = winrt::make_self<CascadiaSettings>(userSettings, inboxSettings);

        const auto colorSchemes = settings->GlobalSettings().ColorSchemes();
        VERIFY_ARE_EQUAL(3u, colorSchemes.Size()); // There should be three: Campbell, Campbell Edited, Antique

        const auto scheme0 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell (modified)"));
        VERIFY_ARE_EQUAL(rgb(0x12, 0x13, 0x14), scheme0->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x12, 0x13, 0x14), scheme0->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::User, scheme0->Origin());

        // Stock Campbell is now untouched
        const auto scheme1 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell"));
        VERIFY_ARE_EQUAL(rgb(0xcc, 0xcc, 0xcc), scheme1->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x0c, 0x0c, 0x0c), scheme1->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::InBox, scheme1->Origin());

        const auto scheme2 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Antique"));
        VERIFY_ARE_EQUAL(rgb(0xc0, 0xc0, 0xc0), scheme2->Foreground());
        VERIFY_ARE_EQUAL(Model::OriginTag::InBox, scheme2->Origin());
    }

    void ColorSchemeTests::LayerColorSchemesWithUserOwnedCollisionRetargetsAllProfiles()
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
                },
                {
                    "name": "Antique",
                    "foreground": "#C0C0C0",
                    "background": "#000000",
                    "cursorColor": "#FFFFFF",
                    "black": "#000000",
                    "red": "#800000",
                    "green": "#008000",
                    "yellow": "#808000",
                    "blue": "#000080",
                    "purple": "#800080",
                    "cyan": "#008080",
                    "white": "#C0C0C0",
                    "brightBlack": "#808080",
                    "brightRed": "#FF0000",
                    "brightGreen": "#00FF00",
                    "brightYellow": "#FFFF00",
                    "brightBlue": "#0000FF",
                    "brightPurple": "#FF00FF",
                    "brightCyan": "#00FFFF",
                    "brightWhite": "#FFFFFF"
                }
            ]
        })" };
        static constexpr std::string_view userSettings{ R"({
            "profiles": {
                "defaults": { }, // We should insert Campbell here
                "list": [
                    {
                        "name" : "profile0" // Does not specify Campbell, should not be edited!
                    },
                    {
                        "name" : "profile1",
                        "colorScheme": "Antique" // This should not be changed
                    },
                    {
                        "name" : "profile2",
                        "colorScheme": "Campbell" // Direct specification should be replaced
                    },
                    {
                        "name" : "profile3",
                        "unfocusedAppearance": {
                            "colorScheme": "Campbell" // Direct specification should be replaced
                        }
                    },
                    {
                        "name" : "profile4",
                        "unfocusedAppearance": {
                            "colorScheme": {
                                "dark": "Campbell" // Direct specification should be replaced
                            }
                        }
                    }
                ],
            },
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
                }
            ]
        })" };

        // The user has a copy of Campbell that they have modified.
        // Profile 0 inherited its default value from the compiled-in settings ("Campbell"),
        // but through the user's perspective _they changed the values in the default scheme._
        // Therefore, we need to retarget any profile with the compiled-in defaults to the
        // new copy of Campbell.
        //
        // Critically, we need to make sure that we do this at the lowest layer that will apply
        // to the most profiles... otherwise we'll make the settings really annoying by putting
        // in so many references to Campbell (modified)

        const auto settings = winrt::make_self<CascadiaSettings>(userSettings, inboxSettings);

        const auto defaults{ settings->ProfileDefaults() };
        VERIFY_IS_TRUE(defaults.DefaultAppearance().HasLightColorSchemeName());
        VERIFY_ARE_EQUAL(L"Campbell (modified)", defaults.DefaultAppearance().LightColorSchemeName());
        VERIFY_IS_TRUE(defaults.DefaultAppearance().HasDarkColorSchemeName());
        VERIFY_ARE_EQUAL(L"Campbell (modified)", defaults.DefaultAppearance().DarkColorSchemeName());

        const auto& profiles{ settings->AllProfiles() };
        {
            const auto& prof0{ profiles.GetAt(0) };
            VERIFY_IS_FALSE(prof0.DefaultAppearance().HasLightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof0.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_FALSE(prof0.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof0.DefaultAppearance().DarkColorSchemeName());
        }
        {
            const auto& prof1{ profiles.GetAt(1) };
            VERIFY_IS_TRUE(prof1.DefaultAppearance().HasLightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Antique", prof1.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof1.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Antique", prof1.DefaultAppearance().DarkColorSchemeName());
        }
        {
            const auto& prof2{ profiles.GetAt(2) };
            VERIFY_IS_TRUE(prof2.DefaultAppearance().HasLightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof2.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof2.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof2.DefaultAppearance().DarkColorSchemeName());
        }
        {
            const auto& prof3{ profiles.GetAt(3) };
            VERIFY_IS_FALSE(prof3.DefaultAppearance().HasLightColorSchemeName());
            VERIFY_IS_TRUE(prof3.UnfocusedAppearance().HasLightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof3.DefaultAppearance().LightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof3.UnfocusedAppearance().LightColorSchemeName());

            VERIFY_IS_FALSE(prof3.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_IS_TRUE(prof3.UnfocusedAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof3.DefaultAppearance().DarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof3.UnfocusedAppearance().DarkColorSchemeName());
        }
        {
            const auto& prof4{ profiles.GetAt(4) };

            VERIFY_IS_FALSE(prof4.DefaultAppearance().HasLightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof4.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_FALSE(prof4.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof4.DefaultAppearance().DarkColorSchemeName());

            VERIFY_IS_FALSE(prof4.UnfocusedAppearance().HasLightColorSchemeName()); // Inherited, did not specify
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof4.UnfocusedAppearance().LightColorSchemeName());

            VERIFY_IS_TRUE(prof4.UnfocusedAppearance().HasDarkColorSchemeName()); // Locally overridden, locally overwritten
            VERIFY_ARE_EQUAL(L"Campbell (modified)", prof4.UnfocusedAppearance().DarkColorSchemeName());
        }
    }

    void ColorSchemeTests::LayerColorSchemesWithUserOwnedCollisionWithFragments()
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
                },
                {
                    "name": "Antique",
                    "foreground": "#C0C0C0",
                    "background": "#000000",
                    "cursorColor": "#FFFFFF",
                    "black": "#000000",
                    "red": "#800000",
                    "green": "#008000",
                    "yellow": "#808000",
                    "blue": "#000080",
                    "purple": "#800080",
                    "cyan": "#008080",
                    "white": "#C0C0C0",
                    "brightBlack": "#808080",
                    "brightRed": "#FF0000",
                    "brightGreen": "#00FF00",
                    "brightYellow": "#FFFF00",
                    "brightBlue": "#0000FF",
                    "brightPurple": "#FF00FF",
                    "brightCyan": "#00FFFF",
                    "brightWhite": "#FFFFFF"
                }
            ]
        })" };

        static constexpr std::string_view fragment{ R"({
            "profiles": [
                {
                    "guid": "{347a67b5-b3a3-4484-9f96-a92d68f6e787}",
                    "name": "fragment profile 0",
                    "colorScheme": {
                        "light": "Mango Light",
                        "dark": "Mango Dark"
                    }
                }
            ],
            "schemes": [
                {
                    "name": "Campbell",
                    "foreground": "#444444",
                    "background": "#444444",
                    "cursorColor": "#999999",
                    "black": "#444444",
                    "red": "#994444",
                    "green": "#494944",
                    "yellow": "#949444",
                    "blue": "#444494",
                    "purple": "#444449",
                    "cyan": "#444449",
                    "white": "#949499",
                    "brightBlack": "#444444",
                    "brightRed": "#994444",
                    "brightGreen": "#499444",
                    "brightYellow": "#999449",
                    "brightBlue": "#444999",
                    "brightPurple": "#994994",
                    "brightCyan": "#449494",
                    "brightWhite": "#999999"
                },
                {
                    "name": "Mango Dark",
                    "foreground": "#D3D7CF",
                    "background": "#000000",
                    "cursorColor": "#FFFFFF",
                    "black": "#000000",
                    "red": "#CC0000",
                    "green": "#4E9A06",
                    "yellow": "#C4A000",
                    "blue": "#3465A4",
                    "purple": "#75507B",
                    "cyan": "#06989A",
                    "white": "#D3D7CF",
                    "brightBlack": "#555753",
                    "brightRed": "#EF2929",
                    "brightGreen": "#8AE234",
                    "brightYellow": "#FCE94F",
                    "brightBlue": "#729FCF",
                    "brightPurple": "#AD7FA8",
                    "brightCyan": "#34E2E2",
                    "brightWhite": "#EEEEEC"
                },
                {
                    "name": "Mango Light",
                    "foreground": "#555753",
                    "background": "#FFFFFF",
                    "cursorColor": "#000000",
                    "black": "#000000",
                    "red": "#CC0000",
                    "green": "#4E9A06",
                    "yellow": "#C4A000",
                    "blue": "#3465A4",
                    "purple": "#75507B",
                    "cyan": "#06989A",
                    "white": "#D3D7CF",
                    "brightBlack": "#555753",
                    "brightRed": "#EF2929",
                    "brightGreen": "#8AE234",
                    "brightYellow": "#FCE94F",
                    "brightBlue": "#729FCF",
                    "brightPurple": "#AD7FA8",
                    "brightCyan": "#34E2E2",
                    "brightWhite": "#EEEEEC"
                }
            ]
        })" };

        static constexpr std::string_view userSettings{ R"({
            "profiles": {
                "defaults": { },
                "list": [
                    {
                        "name" : "profile0"
                    },
                    {
                        "name" : "profile1",
                        "colorScheme": "Antique"
                    },
                    {
                        "name" : "profile2",
                        "colorScheme": "Mango Light"
                    }
                ],
            },
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
                    "name": "Mango Light",
                    "purple": "#121314",
                    "red": "#121314",
                    "selectionBackground": "#121314",
                    "white": "#121314",
                    "yellow": "#121314"
                }
            ]
        })" };

        // In this case, we have a fragment that overrides Campbell and adds Mango Light and Dark.
        // The user is overriding Mango Light.
        // We'll want to make sure that:
        // 1. Campbell has the final modified settings, but does not have a user-owned modified fork.
        // 2. Antique is unmodified.
        // 3. Mango Light needs a modified fork, which contains the user's modified copy
        // 4. Mango Dark does not need a modified fork.
        // The fragment also comes with a profile that uses Mango Light; its light theme should be redirected to Mango Light (modified),
        // but its dark theme should remain the same.

        SettingsLoader loader{ userSettings, inboxSettings };
        loader.MergeInboxIntoUserSettings();
        loader.MergeFragmentIntoUserSettings(L"TestFragment", fragment);
        loader.FinalizeLayering();
        loader.FixupUserSettings();
        const auto settings = winrt::make_self<CascadiaSettings>(std::move(loader));

        // VERIFY SCHEMES
        const auto colorSchemes = settings->GlobalSettings().ColorSchemes();
        const auto scheme0 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell"));
        VERIFY_ARE_EQUAL(rgb(0x44, 0x44, 0x44), scheme0->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x44, 0x44, 0x44), scheme0->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::Fragment, scheme0->Origin());

        // Stock Antique is untouched
        const auto scheme1 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Antique"));
        VERIFY_ARE_EQUAL(rgb(0xc0, 0xc0, 0xc0), scheme1->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x00, 0x00, 0x00), scheme1->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::InBox, scheme1->Origin());

        // Stock Mango Light is untouched as well
        const auto scheme2 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Mango Light"));
        VERIFY_ARE_EQUAL(rgb(0x55, 0x57, 0x53), scheme2->Foreground());
        VERIFY_ARE_EQUAL(rgb(0xff, 0xff, 0xff), scheme2->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::Fragment, scheme2->Origin());

        const auto scheme3 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Mango Light (modified)"));
        VERIFY_ARE_EQUAL(rgb(0x12, 0x13, 0x14), scheme3->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x12, 0x13, 0x14), scheme3->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::User, scheme3->Origin());

        // VERIFY PROFILES
        const auto defaults{ settings->ProfileDefaults() };
        VERIFY_IS_FALSE(defaults.DefaultAppearance().HasLightColorSchemeName()); // User did not specify Campbell, Fragment edited it
        VERIFY_ARE_EQUAL(L"Campbell", defaults.DefaultAppearance().LightColorSchemeName());
        VERIFY_IS_FALSE(defaults.DefaultAppearance().HasDarkColorSchemeName()); // User did not specify Campbell, Fragment edited it
        VERIFY_ARE_EQUAL(L"Campbell", defaults.DefaultAppearance().DarkColorSchemeName());

        const auto& profiles{ settings->AllProfiles() };
        {
            const auto& prof0{ profiles.GetAt(0) };
            VERIFY_ARE_EQUAL(L"Campbell", prof0.DefaultAppearance().LightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell", prof0.DefaultAppearance().DarkColorSchemeName());
        }
        {
            const auto& prof1{ profiles.GetAt(1) };
            VERIFY_IS_TRUE(prof1.DefaultAppearance().HasLightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Antique", prof1.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof1.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Antique", prof1.DefaultAppearance().DarkColorSchemeName());
        }
        {
            const auto& prof2{ profiles.GetAt(2) };
            VERIFY_IS_TRUE(prof2.DefaultAppearance().HasLightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Mango Light (modified)", prof2.DefaultAppearance().LightColorSchemeName());
            VERIFY_IS_TRUE(prof2.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Mango Light (modified)", prof2.DefaultAppearance().DarkColorSchemeName());
        }
        {
            const auto& prof3{ profiles.GetAt(3) };
            VERIFY_IS_TRUE(prof3.DefaultAppearance().HasLightColorSchemeName());
            VERIFY_ARE_EQUAL(L"Mango Light (modified)", prof3.DefaultAppearance().LightColorSchemeName());

            // The leaf profile should *not* specify a dark scheme itself, but it should inherit one.
            VERIFY_IS_FALSE(prof3.DefaultAppearance().HasDarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Mango Dark", prof3.DefaultAppearance().DarkColorSchemeName());
        }
    }

    void ColorSchemeTests::LayerColorSchemesWithUserOwnedMultipleCollisions()
    {
        static constexpr std::string_view inboxSettings{ R"({
            "schemes": [
                {
                    "background": "#111111",
                    "black": "#111111",
                    "blue": "#111111",
                    "brightBlack": "#111111",
                    "brightBlue": "#111111",
                    "brightCyan": "#111111",
                    "brightGreen": "#111111",
                    "brightPurple": "#111111",
                    "brightRed": "#111111",
                    "brightWhite": "#111111",
                    "brightYellow": "#111111",
                    "cursorColor": "#111111",
                    "cyan": "#111111",
                    "foreground": "#111111",
                    "green": "#111111",
                    "name": "Campbell",
                    "purple": "#111111",
                    "red": "#111111",
                    "selectionBackground": "#111111",
                    "white": "#111111",
                    "yellow": "#111111"
                }
            ]
        })" };
        static constexpr std::string_view userSettings{ R"-({
            "profiles": [
                {
                    "name" : "profile0"
                }
            ],
            "schemes": [
                {
                    "background": "#222222",
                    "black": "#222222",
                    "blue": "#222222",
                    "brightBlack": "#222222",
                    "brightBlue": "#222222",
                    "brightCyan": "#222222",
                    "brightGreen": "#222222",
                    "brightPurple": "#222222",
                    "brightRed": "#222222",
                    "brightWhite": "#222222",
                    "brightYellow": "#222222",
                    "cursorColor": "#222222",
                    "cyan": "#222222",
                    "foreground": "#222222",
                    "green": "#222222",
                    "name": "Campbell",
                    "purple": "#222222",
                    "red": "#222222",
                    "selectionBackground": "#222222",
                    "white": "#222222",
                    "yellow": "#222222"
                },
                {
                    "background": "#333333",
                    "black": "#333333",
                    "blue": "#333333",
                    "brightBlack": "#333333",
                    "brightBlue": "#333333",
                    "brightCyan": "#333333",
                    "brightGreen": "#333333",
                    "brightPurple": "#333333",
                    "brightRed": "#333333",
                    "brightWhite": "#333333",
                    "brightYellow": "#333333",
                    "cursorColor": "#333333",
                    "cyan": "#333333",
                    "foreground": "#333333",
                    "green": "#333333",
                    "name": "Campbell (modified)",
                    "purple": "#333333",
                    "red": "#333333",
                    "selectionBackground": "#333333",
                    "white": "#333333",
                    "yellow": "#333333"
                }
            ]
        })-" };

        // In this test, The user has a copy of Campbell which they have modified, and another scheme annoyingly named
        // Campbell (modified). Ha. Let's make sure we don't stomp their (modified) with ours.

        const auto settings = winrt::make_self<CascadiaSettings>(userSettings, inboxSettings);

        const auto colorSchemes = settings->GlobalSettings().ColorSchemes();
        VERIFY_ARE_EQUAL(3u, colorSchemes.Size()); // There should be three: Campbell, Campbell (modified), Campbell (modified 2)

        const auto scheme0 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell (modified 2)"));
        VERIFY_ARE_EQUAL(rgb(0x22, 0x22, 0x22), scheme0->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x22, 0x22, 0x22), scheme0->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::User, scheme0->Origin());

        const auto scheme1 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell (modified)"));
        VERIFY_ARE_EQUAL(rgb(0x33, 0x33, 0x33), scheme1->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x33, 0x33, 0x33), scheme1->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::User, scheme1->Origin());

        // Stock Campbell is now untouched
        const auto scheme2 = winrt::get_self<ColorScheme>(colorSchemes.Lookup(L"Campbell"));
        VERIFY_ARE_EQUAL(rgb(0x11, 0x11, 0x11), scheme2->Foreground());
        VERIFY_ARE_EQUAL(rgb(0x11, 0x11, 0x11), scheme2->Background());
        VERIFY_ARE_EQUAL(Model::OriginTag::InBox, scheme2->Origin());
    }

}
