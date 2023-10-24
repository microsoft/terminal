// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../TextColor.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class TextColorTests
{
    TEST_CLASS(TextColorTests);

    TEST_CLASS_SETUP(ClassSetup);

    TEST_METHOD(TestDefaultColor);
    TEST_METHOD(TestDarkIndexColor);
    TEST_METHOD(TestBrightIndexColor);
    TEST_METHOD(TestRgbColor);
    TEST_METHOD(TestChangeColor);

    std::array<COLORREF, TextColor::TABLE_SIZE> _colorTable;
    const COLORREF _defaultFg = RGB(1, 2, 3);
    const COLORREF _defaultBg = RGB(4, 5, 6);
    const size_t _defaultFgIndex = TextColor::DEFAULT_FOREGROUND;
    const size_t _defaultBgIndex = TextColor::DEFAULT_BACKGROUND;
};

bool TextColorTests::ClassSetup()
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
    _colorTable[_defaultFgIndex] = _defaultFg;
    _colorTable[_defaultBgIndex] = _defaultBg;
    return true;
}

void TextColorTests::TestDefaultColor()
{
    TextColor defaultColor;

    VERIFY_IS_TRUE(defaultColor.IsDefault());
    VERIFY_IS_FALSE(defaultColor.IsLegacy());
    VERIFY_IS_FALSE(defaultColor.IsRgb());

    auto color = defaultColor.GetColor(_colorTable, _defaultFgIndex, false);
    VERIFY_ARE_EQUAL(_defaultFg, color);

    color = defaultColor.GetColor(_colorTable, _defaultFgIndex, true);
    VERIFY_ARE_EQUAL(_defaultFg, color);

    color = defaultColor.GetColor(_colorTable, _defaultBgIndex, false);
    VERIFY_ARE_EQUAL(_defaultBg, color);

    color = defaultColor.GetColor(_colorTable, _defaultBgIndex, true);
    VERIFY_ARE_EQUAL(_defaultBg, color);
}

void TextColorTests::TestDarkIndexColor()
{
    TextColor indexColor((BYTE)(7), false);

    VERIFY_IS_FALSE(indexColor.IsDefault());
    VERIFY_IS_TRUE(indexColor.IsLegacy());
    VERIFY_IS_FALSE(indexColor.IsRgb());

    auto color = indexColor.GetColor(_colorTable, _defaultFgIndex, false);
    VERIFY_ARE_EQUAL(_colorTable[7], color);

    color = indexColor.GetColor(_colorTable, _defaultFgIndex, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = indexColor.GetColor(_colorTable, _defaultBgIndex, false);
    VERIFY_ARE_EQUAL(_colorTable[7], color);

    color = indexColor.GetColor(_colorTable, _defaultBgIndex, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);
}

void TextColorTests::TestBrightIndexColor()
{
    TextColor indexColor((BYTE)(15), false);

    VERIFY_IS_FALSE(indexColor.IsDefault());
    VERIFY_IS_TRUE(indexColor.IsLegacy());
    VERIFY_IS_FALSE(indexColor.IsRgb());

    auto color = indexColor.GetColor(_colorTable, _defaultFgIndex, false);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = indexColor.GetColor(_colorTable, _defaultFgIndex, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = indexColor.GetColor(_colorTable, _defaultBgIndex, false);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = indexColor.GetColor(_colorTable, _defaultBgIndex, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);
}

void TextColorTests::TestRgbColor()
{
    auto myColor = RGB(7, 8, 9);
    TextColor rgbColor(myColor);

    VERIFY_IS_FALSE(rgbColor.IsDefault());
    VERIFY_IS_FALSE(rgbColor.IsLegacy());
    VERIFY_IS_TRUE(rgbColor.IsRgb());

    auto color = rgbColor.GetColor(_colorTable, _defaultFgIndex, false);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(_colorTable, _defaultFgIndex, true);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, false);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, true);
    VERIFY_ARE_EQUAL(myColor, color);
}

void TextColorTests::TestChangeColor()
{
    auto myColor = RGB(7, 8, 9);
    TextColor rgbColor(myColor);

    VERIFY_IS_FALSE(rgbColor.IsDefault());
    VERIFY_IS_FALSE(rgbColor.IsLegacy());
    VERIFY_IS_TRUE(rgbColor.IsRgb());

    auto color = rgbColor.GetColor(_colorTable, _defaultFgIndex, false);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(_colorTable, _defaultFgIndex, true);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, false);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, true);
    VERIFY_ARE_EQUAL(myColor, color);

    rgbColor.SetDefault();

    color = rgbColor.GetColor(_colorTable, _defaultFgIndex, false);
    VERIFY_ARE_EQUAL(_defaultFg, color);

    color = rgbColor.GetColor(_colorTable, _defaultFgIndex, true);
    VERIFY_ARE_EQUAL(_defaultFg, color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, false);
    VERIFY_ARE_EQUAL(_defaultBg, color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, true);
    VERIFY_ARE_EQUAL(_defaultBg, color);

    rgbColor.SetIndex(7, false);
    color = rgbColor.GetColor(_colorTable, _defaultFgIndex, false);
    VERIFY_ARE_EQUAL(_colorTable[7], color);

    color = rgbColor.GetColor(_colorTable, _defaultFgIndex, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, false);
    VERIFY_ARE_EQUAL(_colorTable[7], color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    rgbColor.SetIndex(15, false);
    color = rgbColor.GetColor(_colorTable, _defaultFgIndex, false);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = rgbColor.GetColor(_colorTable, _defaultFgIndex, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, false);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = rgbColor.GetColor(_colorTable, _defaultBgIndex, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);
}
