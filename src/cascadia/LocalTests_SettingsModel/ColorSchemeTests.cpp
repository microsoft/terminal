// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
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

        TEST_METHOD(CanLayerColorScheme);
        TEST_METHOD(LayerColorSchemeProperties);
        TEST_METHOD(LayerColorSchemesOnArray);
        TEST_METHOD(UpdateSchemeReferences);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

    void ColorSchemeTests::CanLayerColorScheme()
    {
        const std::string scheme0String{ R"({
            "name": "scheme0",
            "foreground": "#000000",
            "background": "#010101"
        })" };
        const std::string scheme1String{ R"({
            "name": "scheme1",
            "foreground": "#020202",
            "background": "#030303"
        })" };
        const std::string scheme2String{ R"({
            "name": "scheme0",
            "foreground": "#040404",
            "background": "#050505"
        })" };
        const std::string scheme3String{ R"({
            // "name": "scheme3",
            "foreground": "#060606",
            "background": "#070707"
        })" };

        const auto scheme0Json = VerifyParseSucceeded(scheme0String);
        const auto scheme1Json = VerifyParseSucceeded(scheme1String);
        const auto scheme2Json = VerifyParseSucceeded(scheme2String);
        const auto scheme3Json = VerifyParseSucceeded(scheme3String);

        const auto scheme0 = ColorScheme::FromJson(scheme0Json);

        VERIFY_IS_TRUE(scheme0->ShouldBeLayered(scheme0Json));
        VERIFY_IS_FALSE(scheme0->ShouldBeLayered(scheme1Json));
        VERIFY_IS_TRUE(scheme0->ShouldBeLayered(scheme2Json));
        VERIFY_IS_FALSE(scheme0->ShouldBeLayered(scheme3Json));

        const auto scheme1 = ColorScheme::FromJson(scheme1Json);

        VERIFY_IS_FALSE(scheme1->ShouldBeLayered(scheme0Json));
        VERIFY_IS_TRUE(scheme1->ShouldBeLayered(scheme1Json));
        VERIFY_IS_FALSE(scheme1->ShouldBeLayered(scheme2Json));
        VERIFY_IS_FALSE(scheme1->ShouldBeLayered(scheme3Json));

        const auto scheme3 = ColorScheme::FromJson(scheme3Json);

        VERIFY_IS_FALSE(scheme3->ShouldBeLayered(scheme0Json));
        VERIFY_IS_FALSE(scheme3->ShouldBeLayered(scheme1Json));
        VERIFY_IS_FALSE(scheme3->ShouldBeLayered(scheme2Json));
        VERIFY_IS_FALSE(scheme3->ShouldBeLayered(scheme3Json));
    }

    void ColorSchemeTests::LayerColorSchemeProperties()
    {
        const std::string scheme0String{ R"({
            "name": "scheme0",
            "foreground": "#000000",
            "background": "#010101",
            "selectionBackground": "#010100",
            "cursorColor": "#010001",
            "red": "#010000",
            "green": "#000100",
            "blue": "#000001"
        })" };
        const std::string scheme1String{ R"({
            "name": "scheme1",
            "foreground": "#020202",
            "background": "#030303",
            "selectionBackground": "#020200",
            "cursorColor": "#040004",
            "red": "#020000",

            "blue": "#000002"
        })" };
        const std::string scheme2String{ R"({
            "name": "scheme0",
            "foreground": "#040404",
            "background": "#050505",
            "selectionBackground": "#030300",
            "cursorColor": "#060006",
            "red": "#030000",
            "green": "#000300"
        })" };

        const auto scheme0Json = VerifyParseSucceeded(scheme0String);
        const auto scheme1Json = VerifyParseSucceeded(scheme1String);
        const auto scheme2Json = VerifyParseSucceeded(scheme2String);

        auto scheme0 = ColorScheme::FromJson(scheme0Json);
        VERIFY_ARE_EQUAL(L"scheme0", scheme0->_Name);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 0), scheme0->_Foreground);
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), scheme0->_Background);
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 0), scheme0->_SelectionBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 1, 0, 1), scheme0->_CursorColor);
        VERIFY_ARE_EQUAL(ARGB(0, 1, 0, 0), scheme0->_table[XTERM_RED_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 1, 0), scheme0->_table[XTERM_GREEN_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 1), scheme0->_table[XTERM_BLUE_ATTR]);

        Log::Comment(NoThrowString().Format(
            L"Layering scheme1 on top of scheme0"));
        scheme0->LayerJson(scheme1Json);

        VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), scheme0->_Foreground);
        VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), scheme0->_Background);
        VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 0), scheme0->_SelectionBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 4, 0, 4), scheme0->_CursorColor);
        VERIFY_ARE_EQUAL(ARGB(0, 2, 0, 0), scheme0->_table[XTERM_RED_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 1, 0), scheme0->_table[XTERM_GREEN_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 2), scheme0->_table[XTERM_BLUE_ATTR]);

        Log::Comment(NoThrowString().Format(
            L"Layering scheme2Json on top of (scheme0+scheme1)"));
        scheme0->LayerJson(scheme2Json);

        VERIFY_ARE_EQUAL(ARGB(0, 4, 4, 4), scheme0->_Foreground);
        VERIFY_ARE_EQUAL(ARGB(0, 5, 5, 5), scheme0->_Background);
        VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 0), scheme0->_SelectionBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 6, 0, 6), scheme0->_CursorColor);
        VERIFY_ARE_EQUAL(ARGB(0, 3, 0, 0), scheme0->_table[XTERM_RED_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 3, 0), scheme0->_table[XTERM_GREEN_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 2), scheme0->_table[XTERM_BLUE_ATTR]);
    }

    void ColorSchemeTests::LayerColorSchemesOnArray()
    {
        const std::string scheme0String{ R"({
            "name": "scheme0",
            "foreground": "#000000",
            "background": "#010101"
        })" };
        const std::string scheme1String{ R"({
            "name": "scheme1",
            "foreground": "#020202",
            "background": "#030303"
        })" };
        const std::string scheme2String{ R"({
            "name": "scheme0",
            "foreground": "#040404",
            "background": "#050505"
        })" };
        const std::string scheme3String{ R"({
            // by not providing a name, the scheme will have the name ""
            "foreground": "#060606",
            "background": "#070707"
        })" };

        const auto scheme0Json = VerifyParseSucceeded(scheme0String);
        const auto scheme1Json = VerifyParseSucceeded(scheme1String);
        const auto scheme2Json = VerifyParseSucceeded(scheme2String);
        const auto scheme3Json = VerifyParseSucceeded(scheme3String);

        auto settings = winrt::make_self<CascadiaSettings>();

        VERIFY_ARE_EQUAL(0u, settings->_globals->ColorSchemes().Size());
        VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme0Json));
        VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme1Json));
        VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme2Json));
        VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme3Json));

        settings->_LayerOrCreateColorScheme(scheme0Json);
        {
            for (auto kv : settings->_globals->ColorSchemes())
            {
                Log::Comment(NoThrowString().Format(
                    L"kv:%s->%s", kv.Key().data(), kv.Value().Name().data()));
            }
            VERIFY_ARE_EQUAL(1u, settings->_globals->ColorSchemes().Size());

            VERIFY_IS_TRUE(settings->_globals->ColorSchemes().HasKey(L"scheme0"));
            auto scheme0Proj = settings->_globals->ColorSchemes().Lookup(L"scheme0");
            auto scheme0 = winrt::get_self<ColorScheme>(scheme0Proj);

            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme0Json));
            VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme1Json));
            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme2Json));
            VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme3Json));
            VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 0), scheme0->_Foreground);
            VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), scheme0->_Background);
        }

        settings->_LayerOrCreateColorScheme(scheme1Json);

        {
            VERIFY_ARE_EQUAL(2u, settings->_globals->ColorSchemes().Size());

            VERIFY_IS_TRUE(settings->_globals->ColorSchemes().HasKey(L"scheme0"));
            auto scheme0Proj = settings->_globals->ColorSchemes().Lookup(L"scheme0");
            auto scheme0 = winrt::get_self<ColorScheme>(scheme0Proj);
            VERIFY_IS_TRUE(settings->_globals->ColorSchemes().HasKey(L"scheme1"));
            auto scheme1Proj = settings->_globals->ColorSchemes().Lookup(L"scheme1");
            auto scheme1 = winrt::get_self<ColorScheme>(scheme1Proj);

            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme0Json));
            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme1Json));
            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme2Json));
            VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme3Json));
            VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 0), scheme0->_Foreground);
            VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), scheme0->_Background);
            VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), scheme1->_Foreground);
            VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), scheme1->_Background);
        }
        settings->_LayerOrCreateColorScheme(scheme2Json);

        {
            VERIFY_ARE_EQUAL(2u, settings->_globals->ColorSchemes().Size());

            VERIFY_IS_TRUE(settings->_globals->ColorSchemes().HasKey(L"scheme0"));
            auto scheme0Proj = settings->_globals->ColorSchemes().Lookup(L"scheme0");
            auto scheme0 = winrt::get_self<ColorScheme>(scheme0Proj);
            VERIFY_IS_TRUE(settings->_globals->ColorSchemes().HasKey(L"scheme1"));
            auto scheme1Proj = settings->_globals->ColorSchemes().Lookup(L"scheme1");
            auto scheme1 = winrt::get_self<ColorScheme>(scheme1Proj);

            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme0Json));
            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme1Json));
            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme2Json));
            VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme3Json));
            VERIFY_ARE_EQUAL(ARGB(0, 4, 4, 4), scheme0->_Foreground);
            VERIFY_ARE_EQUAL(ARGB(0, 5, 5, 5), scheme0->_Background);
            VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), scheme1->_Foreground);
            VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), scheme1->_Background);
        }
        settings->_LayerOrCreateColorScheme(scheme3Json);

        {
            VERIFY_ARE_EQUAL(3u, settings->_globals->ColorSchemes().Size());

            VERIFY_IS_TRUE(settings->_globals->ColorSchemes().HasKey(L"scheme0"));
            auto scheme0Proj = settings->_globals->ColorSchemes().Lookup(L"scheme0");
            auto scheme0 = winrt::get_self<ColorScheme>(scheme0Proj);
            VERIFY_IS_TRUE(settings->_globals->ColorSchemes().HasKey(L"scheme1"));
            auto scheme1Proj = settings->_globals->ColorSchemes().Lookup(L"scheme1");
            auto scheme1 = winrt::get_self<ColorScheme>(scheme1Proj);
            VERIFY_IS_TRUE(settings->_globals->ColorSchemes().HasKey(L""));
            auto scheme2Proj = settings->_globals->ColorSchemes().Lookup(L"");
            auto scheme2 = winrt::get_self<ColorScheme>(scheme2Proj);

            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme0Json));
            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme1Json));
            VERIFY_IS_NOT_NULL(settings->_FindMatchingColorScheme(scheme2Json));
            VERIFY_IS_NULL(settings->_FindMatchingColorScheme(scheme3Json));
            VERIFY_ARE_EQUAL(ARGB(0, 4, 4, 4), scheme0->_Foreground);
            VERIFY_ARE_EQUAL(ARGB(0, 5, 5, 5), scheme0->_Background);
            VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), scheme1->_Foreground);
            VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), scheme1->_Background);
            VERIFY_ARE_EQUAL(ARGB(0, 6, 6, 6), scheme2->_Foreground);
            VERIFY_ARE_EQUAL(ARGB(0, 7, 7, 7), scheme2->_Background);
        }
    }

    void ColorSchemeTests::UpdateSchemeReferences()
    {
        const std::string settingsString{ R"json({
                                                "defaultProfile": "Inherited reference",
                                                "profiles": {
                                                    "defaults": {
                                                        "colorScheme": "Scheme 1"
                                                    },
                                                    "list": [
                                                        {
                                                            "name": "Explicit scheme reference",
                                                            "colorScheme": "Scheme 1"
                                                        },
                                                        {
                                                            "name": "Explicit reference; hidden",
                                                            "colorScheme": "Scheme 1",
                                                            "hidden": true
                                                        },
                                                        {
                                                            "name": "Inherited reference"
                                                        },
                                                        {
                                                            "name": "Different reference",
                                                            "colorScheme": "Scheme 2"
                                                        }
                                                    ]
                                                },
                                                "schemes": [
                                                    { "name": "Scheme 1" },
                                                    { "name": "Scheme 2" },
                                                    { "name": "Scheme 1 (renamed)" }
                                                ]
                                            })json" };

        auto settings{ winrt::make_self<CascadiaSettings>(false) };
        settings->_ParseJsonString(settingsString, false);
        settings->_ApplyDefaultsFromUserSettings();
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        // update all references to "Scheme 1"
        const auto newName{ L"Scheme 1 (renamed)" };
        settings->UpdateColorSchemeReferences(L"Scheme 1", newName);

        // verify profile defaults
        Log::Comment(L"Profile Defaults");
        VERIFY_ARE_EQUAL(newName, settings->ProfileDefaults().DefaultAppearance().ColorSchemeName());
        VERIFY_IS_TRUE(settings->ProfileDefaults().DefaultAppearance().HasColorSchemeName());

        // verify all other profiles
        const auto& profiles{ settings->AllProfiles() };
        {
            const auto& prof{ profiles.GetAt(0) };
            Log::Comment(prof.Name().c_str());
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().ColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(1) };
            Log::Comment(prof.Name().c_str());
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().ColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(2) };
            Log::Comment(prof.Name().c_str());
            VERIFY_ARE_EQUAL(newName, prof.DefaultAppearance().ColorSchemeName());
            VERIFY_IS_FALSE(prof.DefaultAppearance().HasColorSchemeName());
        }
        {
            const auto& prof{ profiles.GetAt(3) };
            Log::Comment(prof.Name().c_str());
            VERIFY_ARE_EQUAL(L"Scheme 2", prof.DefaultAppearance().ColorSchemeName());
            VERIFY_IS_TRUE(prof.DefaultAppearance().HasColorSchemeName());
        }
    }
}
