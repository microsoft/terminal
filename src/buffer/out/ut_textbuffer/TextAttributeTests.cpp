// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"
#include "../../../renderer/inc/RenderSettings.hpp"

#include "../TextAttribute.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace Microsoft::Console::Render;

class TextAttributeTests
{
    TEST_CLASS(TextAttributeTests);
    TEST_CLASS_SETUP(ClassSetup);

    TEST_METHOD(TestRoundtripLegacy);
    TEST_METHOD(TestRoundtripMetaBits);
    TEST_METHOD(TestRoundtripExhaustive);
    TEST_METHOD(TestTextAttributeColorGetters);
    TEST_METHOD(TestReverseDefaultColors);
    TEST_METHOD(TestRoundtripDefaultColors);
    TEST_METHOD(TestIntenseAsBright);

    RenderSettings _renderSettings;
    const COLORREF _defaultFg = RGB(1, 2, 3);
    const COLORREF _defaultBg = RGB(4, 5, 6);
    const size_t _defaultFgIndex = TextColor::DEFAULT_FOREGROUND;
    const size_t _defaultBgIndex = TextColor::DEFAULT_BACKGROUND;
};

bool TextAttributeTests::ClassSetup()
{
    _renderSettings.SetColorAlias(ColorAlias::DefaultForeground, _defaultFgIndex, _defaultFg);
    _renderSettings.SetColorAlias(ColorAlias::DefaultBackground, _defaultBgIndex, _defaultBg);
    return true;
}

void TextAttributeTests::TestRoundtripLegacy()
{
    WORD expectedLegacy = FOREGROUND_BLUE | BACKGROUND_RED;
    WORD bgOnly = expectedLegacy & BG_ATTRS;
    WORD bgShifted = bgOnly >> 4;
    auto bgByte = (BYTE)(bgShifted);

    VERIFY_ARE_EQUAL(FOREGROUND_RED, bgByte);

    auto attr = TextAttribute(expectedLegacy);

    VERIFY_IS_TRUE(attr.IsLegacy());
    VERIFY_ARE_EQUAL(expectedLegacy, attr.GetLegacyAttributes());
}

void TextAttributeTests::TestRoundtripMetaBits()
{
    WORD metaFlags[] = {
        COMMON_LVB_GRID_HORIZONTAL,
        COMMON_LVB_GRID_LVERTICAL,
        COMMON_LVB_GRID_RVERTICAL,
        COMMON_LVB_REVERSE_VIDEO,
        COMMON_LVB_UNDERSCORE
    };

    for (auto i = 0; i < ARRAYSIZE(metaFlags); ++i)
    {
        auto flag = metaFlags[i];
        WORD expectedLegacy = FOREGROUND_BLUE | BACKGROUND_RED | flag;
        WORD metaOnly = expectedLegacy & META_ATTRS;
        VERIFY_ARE_EQUAL(flag, metaOnly);

        auto attr = TextAttribute(expectedLegacy);
        VERIFY_IS_TRUE(attr.IsLegacy());
        VERIFY_ARE_EQUAL(expectedLegacy, attr.GetLegacyAttributes());
        VERIFY_ARE_EQUAL(flag, static_cast<WORD>(attr._attrs));
    }
}

void TextAttributeTests::TestRoundtripExhaustive()
{
    WORD allAttrs = (META_ATTRS | FG_ATTRS | BG_ATTRS);
    // This test covers some 0xdfff test cases, printing out Verify: IsTrue for
    //      each takes a lot longer than checking.
    // Only VERIFY if the comparison actually fails to speed up the test.
    Log::Comment(L"This test will check each possible legacy attribute to make "
                 "sure it roundtrips through the creation of a text attribute.");
    Log::Comment(L"It will only log if it fails.");
    for (WORD wLegacy = 0; wLegacy < allAttrs; wLegacy++)
    {
        // 0x2000 is not an actual meta attribute
        // COMMON_LVB_TRAILING_BYTE and COMMON_LVB_TRAILING_BYTE are no longer
        //      stored in the TextAttributes, they're stored in the CharRow
        if (WI_IsFlagSet(wLegacy, 0x2000) ||
            WI_IsFlagSet(wLegacy, COMMON_LVB_LEADING_BYTE) ||
            WI_IsFlagSet(wLegacy, COMMON_LVB_TRAILING_BYTE))
        {
            continue;
        }

        auto attr = TextAttribute(wLegacy);

        if (wLegacy != attr.GetLegacyAttributes())
        {
            Log::Comment(NoThrowString().Format(
                L"Failed on wLegacy=0x%x", wLegacy));
            VERIFY_ARE_EQUAL(wLegacy, attr.GetLegacyAttributes());
        }
    }
}

void TextAttributeTests::TestTextAttributeColorGetters()
{
    const auto& colorTable = _renderSettings.GetColorTable();
    const auto red = RGB(255, 0, 0);
    const auto faintRed = RGB(127, 0, 0);
    const auto green = RGB(0, 255, 0);
    TextAttribute attr(red, green);

    // verify that calculated foreground/background are the same as the direct
    //      values when reverse video is not set
    VERIFY_IS_FALSE(attr.IsReverseVideo());

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(red, green), _renderSettings.GetAttributeColors(attr));

    // with reverse video set, calculated foreground/background values should be
    //      switched while getters stay the same
    attr.SetReverseVideo(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(green, red), _renderSettings.GetAttributeColors(attr));

    // reset the reverse video
    attr.SetReverseVideo(false);

    // with faint set, the calculated foreground value should be fainter
    //      while the background and getters stay the same
    attr.SetFaint(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(faintRed, green), _renderSettings.GetAttributeColors(attr));

    // with reverse video set, calculated foreground/background values should be
    //      switched, and the background fainter, while getters stay the same
    attr.SetReverseVideo(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(green, faintRed), _renderSettings.GetAttributeColors(attr));

    // reset the reverse video and faint attributes
    attr.SetReverseVideo(false);
    attr.SetFaint(false);

    // with invisible set, the calculated foreground value should match the
    //       background, while getters stay the same
    attr.SetInvisible(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(green, green), _renderSettings.GetAttributeColors(attr));

    // with reverse video set, the calculated background value should match
    //      the foreground, while getters stay the same
    attr.SetReverseVideo(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(red, red), _renderSettings.GetAttributeColors(attr));
}

void TextAttributeTests::TestReverseDefaultColors()
{
    const auto& colorTable = _renderSettings.GetColorTable();
    const auto red = RGB(255, 0, 0);
    const auto green = RGB(0, 255, 0);
    TextAttribute attr{};

    // verify that calculated foreground/background are the same as the direct
    //      values when reverse video is not set
    VERIFY_IS_FALSE(attr.IsReverseVideo());

    VERIFY_ARE_EQUAL(_defaultFg, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(_defaultBg, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(_defaultFg, _defaultBg), _renderSettings.GetAttributeColors(attr));

    // with reverse video set, calculated foreground/background values should be
    //      switched while getters stay the same
    attr.SetReverseVideo(true);
    VERIFY_IS_TRUE(attr.IsReverseVideo());

    VERIFY_ARE_EQUAL(_defaultFg, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(_defaultBg, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(_defaultBg, _defaultFg), _renderSettings.GetAttributeColors(attr));

    attr.SetForeground(red);
    VERIFY_IS_TRUE(attr.IsReverseVideo());

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(_defaultBg, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(_defaultBg, red), _renderSettings.GetAttributeColors(attr));

    attr.Invert();
    VERIFY_IS_FALSE(attr.IsReverseVideo());
    attr.SetDefaultForeground();
    attr.SetBackground(green);

    VERIFY_ARE_EQUAL(_defaultFg, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    VERIFY_ARE_EQUAL(std::make_pair(_defaultFg, green), _renderSettings.GetAttributeColors(attr));
}

void TextAttributeTests::TestRoundtripDefaultColors()
{
    // Set the legacy default colors to yellow on blue.
    const BYTE fgLegacyDefault = FOREGROUND_RED;
    const BYTE bgLegacyDefault = BACKGROUND_BLUE;
    TextAttribute::SetLegacyDefaultAttributes(fgLegacyDefault | bgLegacyDefault);

    WORD legacyAttribute;
    TextAttribute textAttribute;

    Log::Comment(L"Foreground legacy default index should map to default text color.");
    legacyAttribute = fgLegacyDefault | BACKGROUND_GREEN;
    textAttribute.SetDefaultForeground();
    textAttribute.SetIndexedBackground256(TextColor::DARK_GREEN);
    VERIFY_ARE_EQUAL(textAttribute, TextAttribute{ legacyAttribute });

    Log::Comment(L"Default foreground text color should map back to legacy default index.");
    VERIFY_ARE_EQUAL(legacyAttribute, textAttribute.GetLegacyAttributes());

    Log::Comment(L"Background legacy default index should map to default text color.");
    legacyAttribute = FOREGROUND_GREEN | bgLegacyDefault;
    textAttribute.SetIndexedForeground256(TextColor::DARK_GREEN);
    textAttribute.SetDefaultBackground();
    VERIFY_ARE_EQUAL(textAttribute, TextAttribute{ legacyAttribute });

    Log::Comment(L"Default background text color should map back to legacy default index.");
    VERIFY_ARE_EQUAL(legacyAttribute, textAttribute.GetLegacyAttributes());

    Log::Comment(L"Foreground and background legacy defaults should map to default text colors.");
    legacyAttribute = fgLegacyDefault | bgLegacyDefault;
    textAttribute.SetDefaultForeground();
    textAttribute.SetDefaultBackground();
    VERIFY_ARE_EQUAL(textAttribute, TextAttribute{ legacyAttribute });

    Log::Comment(L"Default foreground and background text colors should map back to legacy defaults.");
    VERIFY_ARE_EQUAL(legacyAttribute, textAttribute.GetLegacyAttributes());

    // Reset the legacy default colors to white on black.
    TextAttribute::SetLegacyDefaultAttributes(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void TextAttributeTests::TestIntenseAsBright()
{
    const auto& colorTable = _renderSettings.GetColorTable();
    const auto darkBlack = til::at(colorTable, 0);
    const auto brightBlack = til::at(colorTable, 8);
    const auto darkGreen = til::at(colorTable, 2);

    TextAttribute attr{};

    // verify that calculated foreground/background are the same as the direct
    //      values when not intense
    VERIFY_IS_FALSE(attr.IsIntense());

    VERIFY_ARE_EQUAL(_defaultFg, attr.GetForeground().GetColor(colorTable, _defaultFgIndex));
    VERIFY_ARE_EQUAL(_defaultBg, attr.GetBackground().GetColor(colorTable, _defaultBgIndex));
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, true);
    VERIFY_ARE_EQUAL(std::make_pair(_defaultFg, _defaultBg), _renderSettings.GetAttributeColors(attr));
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, false);
    VERIFY_ARE_EQUAL(std::make_pair(_defaultFg, _defaultBg), _renderSettings.GetAttributeColors(attr));

    // with intense set, calculated foreground/background values shouldn't change for the default colors.
    attr.SetIntense(true);
    VERIFY_IS_TRUE(attr.IsIntense());
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, true);
    VERIFY_ARE_EQUAL(std::make_pair(_defaultFg, _defaultBg), _renderSettings.GetAttributeColors(attr));
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, false);
    VERIFY_ARE_EQUAL(std::make_pair(_defaultFg, _defaultBg), _renderSettings.GetAttributeColors(attr));

    attr.SetIndexedForeground(TextColor::DARK_BLACK);
    VERIFY_IS_TRUE(attr.IsIntense());

    Log::Comment(L"Foreground should be bright black when intense is bright is enabled");
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, true);
    VERIFY_ARE_EQUAL(std::make_pair(brightBlack, _defaultBg), _renderSettings.GetAttributeColors(attr));

    Log::Comment(L"Foreground should be dark black when intense is bright is disabled");
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, false);
    VERIFY_ARE_EQUAL(std::make_pair(darkBlack, _defaultBg), _renderSettings.GetAttributeColors(attr));

    attr.SetIndexedBackground(TextColor::DARK_GREEN);
    VERIFY_IS_TRUE(attr.IsIntense());

    Log::Comment(L"background should be unaffected by 'intense is bright'");
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, true);
    VERIFY_ARE_EQUAL(std::make_pair(brightBlack, darkGreen), _renderSettings.GetAttributeColors(attr));
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, false);
    VERIFY_ARE_EQUAL(std::make_pair(darkBlack, darkGreen), _renderSettings.GetAttributeColors(attr));

    attr.SetIntense(false);
    VERIFY_IS_FALSE(attr.IsIntense());
    Log::Comment(L"when not intense, 'intense is bright' changes nothing");
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, true);
    VERIFY_ARE_EQUAL(std::make_pair(darkBlack, darkGreen), _renderSettings.GetAttributeColors(attr));
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, false);
    VERIFY_ARE_EQUAL(std::make_pair(darkBlack, darkGreen), _renderSettings.GetAttributeColors(attr));

    Log::Comment(L"When set to a bright color, and intense, 'intense is bright' changes nothing");
    attr.SetIntense(true);
    attr.SetIndexedForeground(TextColor::BRIGHT_BLACK);
    VERIFY_IS_TRUE(attr.IsIntense());
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, true);
    VERIFY_ARE_EQUAL(std::make_pair(brightBlack, darkGreen), _renderSettings.GetAttributeColors(attr));
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, false);
    VERIFY_ARE_EQUAL(std::make_pair(brightBlack, darkGreen), _renderSettings.GetAttributeColors(attr));

    // Restore the default IntenseIsBright mode.
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, true);
}
