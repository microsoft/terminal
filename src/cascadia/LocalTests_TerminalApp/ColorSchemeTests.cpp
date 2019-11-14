// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppLocalTests
{
    // Unfortunately, these tests _WILL NOT_ work in our CI, until we have a lab
    // machine available that can run Windows version 18362.

    class ColorSchemeTests : public JsonTestClass
    {
        // Use a custom manifest to ensure that we can activate winrt types from
        // our test. This property will tell taef to manually use this as the
        // sxs manifest during this test class. It includes all the cppwinrt
        // types we've defined, so if your test is crashing for an unknown
        // reason, make sure it's included in that file.
        // If you want to do anything XAML-y, you'll need to run your test in a
        // packaged context. See TabTests.cpp for more details on that.
        BEGIN_TEST_CLASS(ColorSchemeTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.LocalTests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(CanLayerColorScheme);
        TEST_METHOD(LayerColorSchemeProperties);
        TEST_METHOD(LayerColorSchemesOnArray);

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

        VERIFY_IS_TRUE(scheme0.ShouldBeLayered(scheme0Json));
        VERIFY_IS_FALSE(scheme0.ShouldBeLayered(scheme1Json));
        VERIFY_IS_TRUE(scheme0.ShouldBeLayered(scheme2Json));
        VERIFY_IS_FALSE(scheme0.ShouldBeLayered(scheme3Json));

        const auto scheme1 = ColorScheme::FromJson(scheme1Json);

        VERIFY_IS_FALSE(scheme1.ShouldBeLayered(scheme0Json));
        VERIFY_IS_TRUE(scheme1.ShouldBeLayered(scheme1Json));
        VERIFY_IS_FALSE(scheme1.ShouldBeLayered(scheme2Json));
        VERIFY_IS_FALSE(scheme1.ShouldBeLayered(scheme3Json));

        const auto scheme3 = ColorScheme::FromJson(scheme3Json);

        VERIFY_IS_FALSE(scheme3.ShouldBeLayered(scheme0Json));
        VERIFY_IS_FALSE(scheme3.ShouldBeLayered(scheme1Json));
        VERIFY_IS_FALSE(scheme3.ShouldBeLayered(scheme2Json));
        VERIFY_IS_FALSE(scheme3.ShouldBeLayered(scheme3Json));
    }

    void ColorSchemeTests::LayerColorSchemeProperties()
    {
        const std::string scheme0String{ R"({
            "name": "scheme0",
            "foreground": "#000000",
            "background": "#010101",
            "selectionBackground": "#010100",
            "red": "#010000",
            "green": "#000100",
            "blue": "#000001"
        })" };
        const std::string scheme1String{ R"({
            "name": "scheme1",
            "foreground": "#020202",
            "background": "#030303",
            "selectionBackground": "#020200",
            "red": "#020000",

            "blue": "#000002"
        })" };
        const std::string scheme2String{ R"({
            "name": "scheme0",
            "foreground": "#040404",
            "background": "#050505",
            "selectionBackground": "#030300",
            "red": "#030000",
            "green": "#000300"
        })" };

        const auto scheme0Json = VerifyParseSucceeded(scheme0String);
        const auto scheme1Json = VerifyParseSucceeded(scheme1String);
        const auto scheme2Json = VerifyParseSucceeded(scheme2String);

        auto scheme0 = ColorScheme::FromJson(scheme0Json);
        VERIFY_ARE_EQUAL(L"scheme0", scheme0._schemeName);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 0), scheme0._defaultForeground);
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), scheme0._defaultBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), scheme0._defaultBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 0), scheme0._selectionBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 1, 0, 0), scheme0._table[XTERM_RED_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 1, 0), scheme0._table[XTERM_GREEN_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 1), scheme0._table[XTERM_BLUE_ATTR]);

        Log::Comment(NoThrowString().Format(
            L"Layering scheme1 on top of scheme0"));
        scheme0.LayerJson(scheme1Json);

        VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), scheme0._defaultForeground);
        VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), scheme0._defaultBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 0), scheme0._selectionBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 2, 0, 0), scheme0._table[XTERM_RED_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 1, 0), scheme0._table[XTERM_GREEN_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 2), scheme0._table[XTERM_BLUE_ATTR]);

        Log::Comment(NoThrowString().Format(
            L"Layering scheme2Json on top of (scheme0+scheme1)"));
        scheme0.LayerJson(scheme2Json);

        VERIFY_ARE_EQUAL(ARGB(0, 4, 4, 4), scheme0._defaultForeground);
        VERIFY_ARE_EQUAL(ARGB(0, 5, 5, 5), scheme0._defaultBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 0), scheme0._selectionBackground);
        VERIFY_ARE_EQUAL(ARGB(0, 3, 0, 0), scheme0._table[XTERM_RED_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 3, 0), scheme0._table[XTERM_GREEN_ATTR]);
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 2), scheme0._table[XTERM_BLUE_ATTR]);
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

        CascadiaSettings settings;

        VERIFY_ARE_EQUAL(0u, settings._globals.GetColorSchemes().size());
        VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme0Json));
        VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme1Json));
        VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme2Json));
        VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme3Json));

        settings._LayerOrCreateColorScheme(scheme0Json);
        {
            for (auto& kv : settings._globals._colorSchemes)
            {
                Log::Comment(NoThrowString().Format(
                    L"kv:%s->%s", kv.first.data(), kv.second.GetName().data()));
            }
            VERIFY_ARE_EQUAL(1u, settings._globals.GetColorSchemes().size());

            VERIFY_IS_TRUE(settings._globals._colorSchemes.find(L"scheme0") != settings._globals._colorSchemes.end());
            auto scheme0 = settings._globals._colorSchemes.find(L"scheme0")->second;

            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme0Json));
            VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme1Json));
            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme2Json));
            VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme3Json));
            VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 0), scheme0._defaultForeground);
            VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), scheme0._defaultBackground);
        }

        settings._LayerOrCreateColorScheme(scheme1Json);

        {
            VERIFY_ARE_EQUAL(2u, settings._globals.GetColorSchemes().size());

            VERIFY_IS_TRUE(settings._globals._colorSchemes.find(L"scheme0") != settings._globals._colorSchemes.end());
            auto scheme0 = settings._globals._colorSchemes.find(L"scheme0")->second;
            VERIFY_IS_TRUE(settings._globals._colorSchemes.find(L"scheme1") != settings._globals._colorSchemes.end());
            auto scheme1 = settings._globals._colorSchemes.find(L"scheme1")->second;

            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme0Json));
            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme1Json));
            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme2Json));
            VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme3Json));
            VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 0), scheme0._defaultForeground);
            VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), scheme0._defaultBackground);
            VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), scheme1._defaultForeground);
            VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), scheme1._defaultBackground);
        }
        settings._LayerOrCreateColorScheme(scheme2Json);

        {
            VERIFY_ARE_EQUAL(2u, settings._globals.GetColorSchemes().size());

            VERIFY_IS_TRUE(settings._globals._colorSchemes.find(L"scheme0") != settings._globals._colorSchemes.end());
            auto scheme0 = settings._globals._colorSchemes.find(L"scheme0")->second;
            VERIFY_IS_TRUE(settings._globals._colorSchemes.find(L"scheme1") != settings._globals._colorSchemes.end());
            auto scheme1 = settings._globals._colorSchemes.find(L"scheme1")->second;

            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme0Json));
            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme1Json));
            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme2Json));
            VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme3Json));
            VERIFY_ARE_EQUAL(ARGB(0, 4, 4, 4), scheme0._defaultForeground);
            VERIFY_ARE_EQUAL(ARGB(0, 5, 5, 5), scheme0._defaultBackground);
            VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), scheme1._defaultForeground);
            VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), scheme1._defaultBackground);
        }
        settings._LayerOrCreateColorScheme(scheme3Json);

        {
            VERIFY_ARE_EQUAL(3u, settings._globals.GetColorSchemes().size());

            VERIFY_IS_TRUE(settings._globals._colorSchemes.find(L"scheme0") != settings._globals._colorSchemes.end());
            auto scheme0 = settings._globals._colorSchemes.find(L"scheme0")->second;
            VERIFY_IS_TRUE(settings._globals._colorSchemes.find(L"scheme1") != settings._globals._colorSchemes.end());
            auto scheme1 = settings._globals._colorSchemes.find(L"scheme1")->second;
            VERIFY_IS_TRUE(settings._globals._colorSchemes.find(L"") != settings._globals._colorSchemes.end());
            auto scheme2 = settings._globals._colorSchemes.find(L"")->second;

            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme0Json));
            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme1Json));
            VERIFY_IS_NOT_NULL(settings._FindMatchingColorScheme(scheme2Json));
            VERIFY_IS_NULL(settings._FindMatchingColorScheme(scheme3Json));
            VERIFY_ARE_EQUAL(ARGB(0, 4, 4, 4), scheme0._defaultForeground);
            VERIFY_ARE_EQUAL(ARGB(0, 5, 5, 5), scheme0._defaultBackground);
            VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), scheme1._defaultForeground);
            VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), scheme1._defaultBackground);
            VERIFY_ARE_EQUAL(ARGB(0, 6, 6, 6), scheme2._defaultForeground);
            VERIFY_ARE_EQUAL(ARGB(0, 7, 7, 7), scheme2._defaultBackground);
        }
    }
}
