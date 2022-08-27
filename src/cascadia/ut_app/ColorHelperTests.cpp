// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalSettingsModel/Profile.h"
#include "../TerminalApp/ColorHelper.h"

// Import some templates to compare floats using approximate matching.
#include <consoletaeftemplates.hpp>

using namespace Microsoft::Console;
using namespace winrt::TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppUnitTests
{
    class ColorHelperTests
    {
        BEGIN_TEST_CLASS(ColorHelperTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(ConvertRgbToHsl);
        TEST_METHOD(ConvertHslToRgb);
        TEST_METHOD(LuminanceTests);
    };

    void ColorHelperTests::ConvertHslToRgb()
    {
        auto red = winrt::Windows::UI::Colors::Red();
        auto redHsl = ColorHelper::RgbToHsl(red);
        VERIFY_ARE_EQUAL(0.f, redHsl.H);
        VERIFY_ARE_EQUAL(1.f, redHsl.S);
        VERIFY_ARE_EQUAL(0.5f, redHsl.L);

        auto green = winrt::Windows::UI::Colors::Lime();
        auto greenHsl = ColorHelper::RgbToHsl(green);
        VERIFY_ARE_EQUAL(120.f, greenHsl.H);
        VERIFY_ARE_EQUAL(1.f, greenHsl.S);
        VERIFY_ARE_EQUAL(0.5f, greenHsl.L);

        auto blue = winrt::Windows::UI::Colors::Blue();
        auto blueHsl = ColorHelper::RgbToHsl(blue);
        VERIFY_ARE_EQUAL(240.f, blueHsl.H);
        VERIFY_ARE_EQUAL(1.f, blueHsl.S);
        VERIFY_ARE_EQUAL(0.5f, blueHsl.L);

        auto darkTurquoise = winrt::Windows::UI::Colors::DarkTurquoise();
        auto darkTurquoiseHsl = ColorHelper::RgbToHsl(darkTurquoise);
        VERIFY_ARE_EQUAL(181.f, darkTurquoiseHsl.H);
        VERIFY_ARE_EQUAL(1.f, darkTurquoiseHsl.S);
        VERIFY_ARE_EQUAL(0.41f, darkTurquoiseHsl.L);

        auto darkViolet = winrt::Windows::UI::Colors::DarkViolet();
        auto darkVioletHsl = ColorHelper::RgbToHsl(darkViolet);
        VERIFY_ARE_EQUAL(282.f, darkVioletHsl.H);
        VERIFY_ARE_EQUAL(1.f, darkVioletHsl.S);
        VERIFY_ARE_EQUAL(0.414f, darkVioletHsl.L);

        auto white = winrt::Windows::UI::Colors::White();
        auto whiteHsl = ColorHelper::RgbToHsl(white);
        VERIFY_ARE_EQUAL(0.f, whiteHsl.H);
        VERIFY_ARE_EQUAL(0.f, whiteHsl.S);
        VERIFY_ARE_EQUAL(1.f, whiteHsl.L);

        auto black = winrt::Windows::UI::Colors::Black();
        auto blackHsl = ColorHelper::RgbToHsl(black);
        VERIFY_ARE_EQUAL(0.f, blackHsl.H);
        VERIFY_ARE_EQUAL(0.f, blackHsl.S);
        VERIFY_ARE_EQUAL(0.f, blackHsl.L);
    }

    void ColorHelperTests::ConvertRgbToHsl()
    {
        auto redHsl = HSL{ 0.f, 100.f, 50.f };
        auto red = ColorHelper::HslToRgb(redHsl);
        VERIFY_ARE_EQUAL(255, red.R);
        VERIFY_ARE_EQUAL(0, red.G);
        VERIFY_ARE_EQUAL(0, red.B);

        auto greenHsl = HSL{ 120.f, 100.f, 50.f };
        auto green = ColorHelper::HslToRgb(greenHsl);
        VERIFY_ARE_EQUAL(0, green.R);
        VERIFY_ARE_EQUAL(255, green.G);
        VERIFY_ARE_EQUAL(0, green.B);

        auto blueHsl = HSL{ 240.f, 100.f, 50.f };
        auto blue = ColorHelper::HslToRgb(blueHsl);
        VERIFY_ARE_EQUAL(0, blue.R);
        VERIFY_ARE_EQUAL(0, blue.G);
        VERIFY_ARE_EQUAL(255, blue.B);

        auto darkTurquoiseHsl = HSL{ 181.f, 100.f, 41.f };
        auto darkTurquoise = ColorHelper::HslToRgb(darkTurquoiseHsl);
        VERIFY_ARE_EQUAL(0, darkTurquoise.R);
        VERIFY_ARE_EQUAL(206, darkTurquoise.G);
        VERIFY_ARE_EQUAL(209, darkTurquoise.B);

        auto darkVioletHsl = HSL{ 282.f, 100.f, 41.4f };
        auto darkViolet = ColorHelper::HslToRgb(darkVioletHsl);
        VERIFY_ARE_EQUAL(148, darkViolet.R);
        VERIFY_ARE_EQUAL(0, darkViolet.G);
        VERIFY_ARE_EQUAL(211, darkViolet.B);

        auto whiteHsl = HSL{ 360.f, 100.f, 100.f };
        auto white = ColorHelper::HslToRgb(whiteHsl);
        VERIFY_ARE_EQUAL(255, white.R);
        VERIFY_ARE_EQUAL(255, white.G);
        VERIFY_ARE_EQUAL(255, white.B);

        auto blackHsl = HSL{ 0.f, 0.f, 0.f };
        auto black = ColorHelper::HslToRgb(blackHsl);
        VERIFY_ARE_EQUAL(0, black.R);
        VERIFY_ARE_EQUAL(0, black.G);
        VERIFY_ARE_EQUAL(0, black.B);
    }

    void ColorHelperTests::LuminanceTests()
    {
        auto darkTurquoiseLuminance = ColorHelper::GetLuminance(winrt::Windows::UI::Colors::DarkTurquoise());
        VERIFY_ARE_EQUAL(48.75f, darkTurquoiseLuminance * 100);
        auto darkVioletLuminance = ColorHelper::GetLuminance(winrt::Windows::UI::Colors::DarkViolet());
        VERIFY_ARE_EQUAL(11.f, darkVioletLuminance * 100);
        auto magentaLuminance = ColorHelper::GetLuminance(winrt::Windows::UI::Colors::Magenta());
        VERIFY_ARE_EQUAL(28.48f, magentaLuminance * 100);
    }
}
