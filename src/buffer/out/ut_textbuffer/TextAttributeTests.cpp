// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../TextAttribute.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

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

    static const int COLOR_TABLE_SIZE = 16;
    COLORREF _colorTable[COLOR_TABLE_SIZE];
    COLORREF _defaultFg = RGB(1, 2, 3);
    COLORREF _defaultBg = RGB(4, 5, 6);
    gsl::span<const COLORREF> _GetTableView();
};

bool TextAttributeTests::ClassSetup()
{
    _colorTable[0] = RGB(12, 12, 12); // Black
    _colorTable[1] = RGB(0, 55, 218); // Dark Blue
    _colorTable[2] = RGB(19, 161, 14); // Dark Green
    _colorTable[3] = RGB(58, 150, 221); // Dark Cyan
    _colorTable[4] = RGB(197, 15, 31); // Dark Red
    _colorTable[5] = RGB(136, 23, 152); // Dark Magenta
    _colorTable[6] = RGB(193, 156, 0); // Dark Yellow
    _colorTable[7] = RGB(204, 204, 204); // Dark White
    _colorTable[8] = RGB(118, 118, 118); // Bright Black
    _colorTable[9] = RGB(59, 120, 255); // Bright Blue
    _colorTable[10] = RGB(22, 198, 12); // Bright Green
    _colorTable[11] = RGB(97, 214, 214); // Bright Cyan
    _colorTable[12] = RGB(231, 72, 86); // Bright Red
    _colorTable[13] = RGB(180, 0, 158); // Bright Magenta
    _colorTable[14] = RGB(249, 241, 165); // Bright Yellow
    _colorTable[15] = RGB(242, 242, 242); // White
    return true;
}

gsl::span<const COLORREF> TextAttributeTests::_GetTableView()
{
    return gsl::span<const COLORREF>(&_colorTable[0], COLOR_TABLE_SIZE);
}

void TextAttributeTests::TestRoundtripLegacy()
{
    WORD expectedLegacy = FOREGROUND_BLUE | BACKGROUND_RED;
    WORD bgOnly = expectedLegacy & BG_ATTRS;
    WORD bgShifted = bgOnly >> 4;
    BYTE bgByte = (BYTE)(bgShifted);

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

    for (int i = 0; i < ARRAYSIZE(metaFlags); ++i)
    {
        WORD flag = metaFlags[i];
        WORD expectedLegacy = FOREGROUND_BLUE | BACKGROUND_RED | flag;
        WORD metaOnly = expectedLegacy & META_ATTRS;
        VERIFY_ARE_EQUAL(flag, metaOnly);

        auto attr = TextAttribute(expectedLegacy);
        VERIFY_IS_TRUE(attr.IsLegacy());
        VERIFY_ARE_EQUAL(expectedLegacy, attr.GetLegacyAttributes());
        VERIFY_ARE_EQUAL(flag, attr._wAttrLegacy);
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
    const COLORREF red = RGB(255, 0, 0);
    const COLORREF faintRed = RGB(127, 0, 0);
    const COLORREF green = RGB(0, 255, 0);
    TextAttribute attr(red, green);
    auto view = _GetTableView();

    // verify that calculated foreground/background are the same as the direct
    //      values when reverse video is not set
    VERIFY_IS_FALSE(attr.IsReverseVideo());

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(red, green), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));

    // with reverse video set, calculated foreground/background values should be
    //      switched while getters stay the same
    attr.SetReverseVideo(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(green, red), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));

    // reset the reverse video
    attr.SetReverseVideo(false);

    // with faint set, the calculated foreground value should be fainter
    //      while the background and getters stay the same
    attr.SetFaint(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(faintRed, green), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));

    // with reverse video set, calculated foreground/background values should be
    //      switched, and the background fainter, while getters stay the same
    attr.SetReverseVideo(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(green, faintRed), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));

    // reset the reverse video and faint attributes
    attr.SetReverseVideo(false);
    attr.SetFaint(false);

    // with invisible set, the calculated foreground value should match the
    //       background, while getters stay the same
    attr.SetInvisible(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(green, green), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));

    // with reverse video set, the calculated background value should match
    //      the foreground, while getters stay the same
    attr.SetReverseVideo(true);

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(red, red), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));
}

void TextAttributeTests::TestReverseDefaultColors()
{
    const COLORREF red = RGB(255, 0, 0);
    const COLORREF green = RGB(0, 255, 0);
    TextAttribute attr{};
    auto view = _GetTableView();

    // verify that calculated foreground/background are the same as the direct
    //      values when reverse video is not set
    VERIFY_IS_FALSE(attr.IsReverseVideo());

    VERIFY_ARE_EQUAL(_defaultFg, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(_defaultBg, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(_defaultFg, _defaultBg), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));

    // with reverse video set, calculated foreground/background values should be
    //      switched while getters stay the same
    attr.SetReverseVideo(true);
    VERIFY_IS_TRUE(attr.IsReverseVideo());

    VERIFY_ARE_EQUAL(_defaultFg, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(_defaultBg, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(_defaultBg, _defaultFg), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));

    attr.SetForeground(red);
    VERIFY_IS_TRUE(attr.IsReverseVideo());

    VERIFY_ARE_EQUAL(red, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(_defaultBg, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(_defaultBg, red), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));

    attr.Invert();
    VERIFY_IS_FALSE(attr.IsReverseVideo());
    attr.SetDefaultForeground();
    attr.SetBackground(green);

    VERIFY_ARE_EQUAL(_defaultFg, attr.GetForeground().GetColor(view, _defaultFg));
    VERIFY_ARE_EQUAL(green, attr.GetBackground().GetColor(view, _defaultBg));
    VERIFY_ARE_EQUAL(std::make_pair(_defaultFg, green), attr.CalculateRgbColors(view, _defaultFg, _defaultBg));
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
    textAttribute.SetIndexedBackground256(BACKGROUND_GREEN >> 4);
    VERIFY_ARE_EQUAL(textAttribute, TextAttribute{ legacyAttribute });

    Log::Comment(L"Default foreground text color should map back to legacy default index.");
    VERIFY_ARE_EQUAL(legacyAttribute, textAttribute.GetLegacyAttributes());

    Log::Comment(L"Background legacy default index should map to default text color.");
    legacyAttribute = FOREGROUND_GREEN | bgLegacyDefault;
    textAttribute.SetIndexedForeground256(FOREGROUND_GREEN);
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
