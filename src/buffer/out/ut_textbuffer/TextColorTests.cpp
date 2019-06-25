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

    static const int COLOR_TABLE_SIZE = 16;
    COLORREF _colorTable[COLOR_TABLE_SIZE];
    COLORREF _defaultFg = RGB(1, 2, 3);
    COLORREF _defaultBg = RGB(4, 5, 6);
    std::basic_string_view<COLORREF> _GetTableView();
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
    return true;
}

std::basic_string_view<COLORREF> TextColorTests::_GetTableView()
{
    return std::basic_string_view<COLORREF>(&_colorTable[0], COLOR_TABLE_SIZE);
}

void TextColorTests::TestDefaultColor()
{
    TextColor defaultColor;

    VERIFY_IS_TRUE(defaultColor.IsDefault());
    VERIFY_IS_FALSE(defaultColor.IsLegacy());
    VERIFY_IS_FALSE(defaultColor.IsRgb());

    auto view = _GetTableView();

    auto color = defaultColor.GetColor(view, _defaultFg, false);
    VERIFY_ARE_EQUAL(_defaultFg, color);

    color = defaultColor.GetColor(view, _defaultFg, true);
    VERIFY_ARE_EQUAL(_defaultFg, color);

    color = defaultColor.GetColor(view, _defaultBg, false);
    VERIFY_ARE_EQUAL(_defaultBg, color);

    color = defaultColor.GetColor(view, _defaultBg, true);
    VERIFY_ARE_EQUAL(_defaultBg, color);
}

void TextColorTests::TestDarkIndexColor()
{
    TextColor indexColor((BYTE)(7));

    VERIFY_IS_FALSE(indexColor.IsDefault());
    VERIFY_IS_TRUE(indexColor.IsLegacy());
    VERIFY_IS_FALSE(indexColor.IsRgb());

    auto view = _GetTableView();

    auto color = indexColor.GetColor(view, _defaultFg, false);
    VERIFY_ARE_EQUAL(_colorTable[7], color);

    color = indexColor.GetColor(view, _defaultFg, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = indexColor.GetColor(view, _defaultBg, false);
    VERIFY_ARE_EQUAL(_colorTable[7], color);

    color = indexColor.GetColor(view, _defaultBg, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);
}

void TextColorTests::TestBrightIndexColor()
{
    TextColor indexColor((BYTE)(15));

    VERIFY_IS_FALSE(indexColor.IsDefault());
    VERIFY_IS_TRUE(indexColor.IsLegacy());
    VERIFY_IS_FALSE(indexColor.IsRgb());

    auto view = _GetTableView();

    auto color = indexColor.GetColor(view, _defaultFg, false);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = indexColor.GetColor(view, _defaultFg, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = indexColor.GetColor(view, _defaultBg, false);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = indexColor.GetColor(view, _defaultBg, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);
}

void TextColorTests::TestRgbColor()
{
    COLORREF myColor = RGB(7, 8, 9);
    TextColor rgbColor(myColor);

    VERIFY_IS_FALSE(rgbColor.IsDefault());
    VERIFY_IS_FALSE(rgbColor.IsLegacy());
    VERIFY_IS_TRUE(rgbColor.IsRgb());

    auto view = _GetTableView();

    auto color = rgbColor.GetColor(view, _defaultFg, false);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(view, _defaultFg, true);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(view, _defaultBg, false);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(view, _defaultBg, true);
    VERIFY_ARE_EQUAL(myColor, color);
}

void TextColorTests::TestChangeColor()
{
    COLORREF myColor = RGB(7, 8, 9);
    TextColor rgbColor(myColor);

    VERIFY_IS_FALSE(rgbColor.IsDefault());
    VERIFY_IS_FALSE(rgbColor.IsLegacy());
    VERIFY_IS_TRUE(rgbColor.IsRgb());

    auto view = _GetTableView();

    auto color = rgbColor.GetColor(view, _defaultFg, false);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(view, _defaultFg, true);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(view, _defaultBg, false);
    VERIFY_ARE_EQUAL(myColor, color);

    color = rgbColor.GetColor(view, _defaultBg, true);
    VERIFY_ARE_EQUAL(myColor, color);

    rgbColor.SetDefault();

    color = rgbColor.GetColor(view, _defaultFg, false);
    VERIFY_ARE_EQUAL(_defaultFg, color);

    color = rgbColor.GetColor(view, _defaultFg, true);
    VERIFY_ARE_EQUAL(_defaultFg, color);

    color = rgbColor.GetColor(view, _defaultBg, false);
    VERIFY_ARE_EQUAL(_defaultBg, color);

    color = rgbColor.GetColor(view, _defaultBg, true);
    VERIFY_ARE_EQUAL(_defaultBg, color);

    rgbColor.SetIndex(7);
    color = rgbColor.GetColor(view, _defaultFg, false);
    VERIFY_ARE_EQUAL(_colorTable[7], color);

    color = rgbColor.GetColor(view, _defaultFg, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = rgbColor.GetColor(view, _defaultBg, false);
    VERIFY_ARE_EQUAL(_colorTable[7], color);

    color = rgbColor.GetColor(view, _defaultBg, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    rgbColor.SetIndex(15);
    color = rgbColor.GetColor(view, _defaultFg, false);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = rgbColor.GetColor(view, _defaultFg, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = rgbColor.GetColor(view, _defaultBg, false);
    VERIFY_ARE_EQUAL(_colorTable[15], color);

    color = rgbColor.GetColor(view, _defaultBg, true);
    VERIFY_ARE_EQUAL(_colorTable[15], color);
}
