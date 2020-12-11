// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../inc/utils.hpp"
#include "../inc/colorTable.hpp"
#include <conattrs.hpp>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Console::Utils;

class UtilsTests
{
    TEST_CLASS(UtilsTests);

    TEST_METHOD(TestClampToShortMax);
    TEST_METHOD(TestSwapColorPalette);
    TEST_METHOD(TestGuidToString);
    TEST_METHOD(TestSplitString);
    TEST_METHOD(TestStringToUint);
    TEST_METHOD(TestColorFromXTermColor);

    void _VerifyXTermColorResult(const std::wstring_view wstr, DWORD colorValue);
    void _VerifyXTermColorInvalid(const std::wstring_view wstr);
};

void UtilsTests::TestClampToShortMax()
{
    const short min = 1;

    // Test outside the lower end of the range
    const short minExpected = min;
    auto minActual = ClampToShortMax(0, min);
    VERIFY_ARE_EQUAL(minExpected, minActual);

    // Test negative numbers
    const short negativeExpected = min;
    auto negativeActual = ClampToShortMax(-1, min);
    VERIFY_ARE_EQUAL(negativeExpected, negativeActual);

    // Test outside the upper end of the range
    const short maxExpected = SHRT_MAX;
    auto maxActual = ClampToShortMax(50000, min);
    VERIFY_ARE_EQUAL(maxExpected, maxActual);

    // Test within the range
    const short withinRangeExpected = 100;
    auto withinRangeActual = ClampToShortMax(withinRangeExpected, min);
    VERIFY_ARE_EQUAL(withinRangeExpected, withinRangeActual);
}
void UtilsTests::TestSwapColorPalette()
{
    std::array<COLORREF, COLOR_TABLE_SIZE> terminalTable;
    std::array<COLORREF, COLOR_TABLE_SIZE> consoleTable;

    gsl::span<COLORREF> terminalTableView = { &terminalTable[0], terminalTable.size() };
    gsl::span<COLORREF> consoleTableView = { &consoleTable[0], consoleTable.size() };

    // First set up the colors
    InitializeCampbellColorTable(terminalTableView);
    InitializeCampbellColorTableForConhost(consoleTableView);

    VERIFY_ARE_EQUAL(terminalTable[0], consoleTable[0]);
    VERIFY_ARE_EQUAL(terminalTable[1], consoleTable[4]);
    VERIFY_ARE_EQUAL(terminalTable[2], consoleTable[2]);
    VERIFY_ARE_EQUAL(terminalTable[3], consoleTable[6]);
    VERIFY_ARE_EQUAL(terminalTable[4], consoleTable[1]);
    VERIFY_ARE_EQUAL(terminalTable[5], consoleTable[5]);
    VERIFY_ARE_EQUAL(terminalTable[6], consoleTable[3]);
    VERIFY_ARE_EQUAL(terminalTable[7], consoleTable[7]);
    VERIFY_ARE_EQUAL(terminalTable[8], consoleTable[8]);
    VERIFY_ARE_EQUAL(terminalTable[9], consoleTable[12]);
    VERIFY_ARE_EQUAL(terminalTable[10], consoleTable[10]);
    VERIFY_ARE_EQUAL(terminalTable[11], consoleTable[14]);
    VERIFY_ARE_EQUAL(terminalTable[12], consoleTable[9]);
    VERIFY_ARE_EQUAL(terminalTable[13], consoleTable[13]);
    VERIFY_ARE_EQUAL(terminalTable[14], consoleTable[11]);
    VERIFY_ARE_EQUAL(terminalTable[15], consoleTable[15]);
}

void UtilsTests::TestGuidToString()
{
    constexpr GUID constantGuid{
        0x01020304, 0x0506, 0x0708, { 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 }
    };
    constexpr std::wstring_view constantGuidString{ L"{01020304-0506-0708-090a-0b0c0d0e0f10}" };

    auto generatedGuid{ GuidToString(constantGuid) };

    VERIFY_ARE_EQUAL(constantGuidString.size(), generatedGuid.size());
    VERIFY_ARE_EQUAL(constantGuidString, generatedGuid);
}

void UtilsTests::TestSplitString()
{
    std::vector<std::wstring_view> result;
    result = SplitString(L"", L';');
    VERIFY_ARE_EQUAL(0u, result.size());
    result = SplitString(L"1", L';');
    VERIFY_ARE_EQUAL(1u, result.size());
    result = SplitString(L";", L';');
    VERIFY_ARE_EQUAL(2u, result.size());
    result = SplitString(L"123", L';');
    VERIFY_ARE_EQUAL(1u, result.size());

    result = SplitString(L";123", L';');
    VERIFY_ARE_EQUAL(2u, result.size());
    VERIFY_ARE_EQUAL(L"", result.at(0));
    VERIFY_ARE_EQUAL(L"123", result.at(1));

    result = SplitString(L"123;", L';');
    VERIFY_ARE_EQUAL(2u, result.size());
    VERIFY_ARE_EQUAL(L"123", result.at(0));
    VERIFY_ARE_EQUAL(L"", result.at(1));

    result = SplitString(L"123;456", L';');
    VERIFY_ARE_EQUAL(2u, result.size());
    VERIFY_ARE_EQUAL(L"123", result.at(0));
    VERIFY_ARE_EQUAL(L"456", result.at(1));

    result = SplitString(L"123;456;789", L';');
    VERIFY_ARE_EQUAL(3u, result.size());
    VERIFY_ARE_EQUAL(L"123", result.at(0));
    VERIFY_ARE_EQUAL(L"456", result.at(1));
    VERIFY_ARE_EQUAL(L"789", result.at(2));
}

void UtilsTests::TestStringToUint()
{
    bool success = false;
    unsigned int value = 0;
    success = StringToUint(L"", value);
    VERIFY_IS_FALSE(success);
    success = StringToUint(L"xyz", value);
    VERIFY_IS_FALSE(success);
    success = StringToUint(L";", value);
    VERIFY_IS_FALSE(success);

    success = StringToUint(L"1", value);
    VERIFY_IS_TRUE(success);
    VERIFY_ARE_EQUAL(1u, value);

    success = StringToUint(L"123", value);
    VERIFY_IS_TRUE(success);
    VERIFY_ARE_EQUAL(123u, value);

    success = StringToUint(L"123456789", value);
    VERIFY_IS_TRUE(success);
    VERIFY_ARE_EQUAL(123456789u, value);
}

void UtilsTests::TestColorFromXTermColor()
{
    _VerifyXTermColorResult(L"rgb:1/1/1", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rGb:1/1/1", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"RGB:1/1/1", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:111/1/1", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:1111/1/1", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:1/11/1", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:1/111/1", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:1/1111/1", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:1/1/11", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:1/1/111", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:1/1/1111", RGB(0x11, 0x11, 0x11));
    _VerifyXTermColorResult(L"rgb:1/23/4", RGB(0x11, 0x23, 0x44));
    _VerifyXTermColorResult(L"rgb:1/23/45", RGB(0x11, 0x23, 0x45));
    _VerifyXTermColorResult(L"rgb:1/23/456", RGB(0x11, 0x23, 0x45));
    _VerifyXTermColorResult(L"rgb:12/34/5", RGB(0x12, 0x34, 0x55));
    _VerifyXTermColorResult(L"rgb:12/34/56", RGB(0x12, 0x34, 0x56));
    _VerifyXTermColorResult(L"rgb:12/345/67", RGB(0x12, 0x34, 0x67));
    _VerifyXTermColorResult(L"rgb:12/345/678", RGB(0x12, 0x34, 0x67));
    _VerifyXTermColorResult(L"rgb:123/456/789", RGB(0x12, 0x45, 0x78));
    _VerifyXTermColorResult(L"rgb:123/4564/789", RGB(0x12, 0x45, 0x78));
    _VerifyXTermColorResult(L"rgb:123/4564/7897", RGB(0x12, 0x45, 0x78));
    _VerifyXTermColorResult(L"rgb:1231/4564/7897", RGB(0x12, 0x45, 0x78));

    _VerifyXTermColorResult(L"#111", RGB(0x10, 0x10, 0x10));
    _VerifyXTermColorResult(L"#123456", RGB(0x12, 0x34, 0x56));
    _VerifyXTermColorResult(L"#123456789", RGB(0x12, 0x45, 0x78));
    _VerifyXTermColorResult(L"#123145647897", RGB(0x12, 0x45, 0x78));

    _VerifyXTermColorResult(L"orange", RGB(255, 165, 0));
    _VerifyXTermColorResult(L"dark green", RGB(0, 100, 0));
    _VerifyXTermColorResult(L"medium sea green", RGB(60, 179, 113));
    _VerifyXTermColorResult(L"LightYellow", RGB(255, 255, 224));
    _VerifyXTermColorResult(L"yellow", RGB(255, 255, 0));
    _VerifyXTermColorResult(L"yellow3", RGB(205, 205, 0));
    _VerifyXTermColorResult(L"wheat", RGB(245, 222, 179));
    _VerifyXTermColorResult(L"wheat4", RGB(139, 126, 102));
    _VerifyXTermColorResult(L"royalblue", RGB(65, 105, 225));
    _VerifyXTermColorResult(L"royalblue3", RGB(58, 95, 205));
    _VerifyXTermColorResult(L"gray", RGB(190, 190, 190));
    _VerifyXTermColorResult(L"grey", RGB(190, 190, 190));
    _VerifyXTermColorResult(L"gray0", RGB(0, 0, 0));
    _VerifyXTermColorResult(L"grey0", RGB(0, 0, 0));
    _VerifyXTermColorResult(L"gray58", RGB(148, 148, 148));
    _VerifyXTermColorResult(L"grey58", RGB(148, 148, 148));
    _VerifyXTermColorResult(L"gray99", RGB(252, 252, 252));
    _VerifyXTermColorResult(L"grey99", RGB(252, 252, 252));

    // Invalid sequences.
    _VerifyXTermColorInvalid(L"");
    _VerifyXTermColorInvalid(L"r:");
    _VerifyXTermColorInvalid(L"rg:");
    _VerifyXTermColorInvalid(L"rgb:");
    _VerifyXTermColorInvalid(L"rgb:/");
    _VerifyXTermColorInvalid(L"rgb://");
    _VerifyXTermColorInvalid(L"rgb:///");
    _VerifyXTermColorInvalid(L"rgb:1");
    _VerifyXTermColorInvalid(L"rgb:1/");
    _VerifyXTermColorInvalid(L"rgb:/1");
    _VerifyXTermColorInvalid(L"rgb:1/1");
    _VerifyXTermColorInvalid(L"rgb:1/1/");
    _VerifyXTermColorInvalid(L"rgb:1/11/");
    _VerifyXTermColorInvalid(L"rgb:/1/1");
    _VerifyXTermColorInvalid(L"rgb:1/1/1/");
    _VerifyXTermColorInvalid(L"rgb:1/1/1/1");
    _VerifyXTermColorInvalid(L"rgb:111111111");
    _VerifyXTermColorInvalid(L"rgb:this/is/invalid");
    _VerifyXTermColorInvalid(L"rgba:1/1/1");
    _VerifyXTermColorInvalid(L"rgbi:1/1/1");
    _VerifyXTermColorInvalid(L"cmyk:1/1/1/1");
    _VerifyXTermColorInvalid(L"rgb#111");
    _VerifyXTermColorInvalid(L"rgb:#111");
    _VerifyXTermColorInvalid(L"rgb:rgb:1/1/1");
    _VerifyXTermColorInvalid(L"rgb:rgb:#111");
    _VerifyXTermColorInvalid(L"#");
    _VerifyXTermColorInvalid(L"#1");
    _VerifyXTermColorInvalid(L"#1111");
    _VerifyXTermColorInvalid(L"#11111");
    _VerifyXTermColorInvalid(L"#1/1/1");
    _VerifyXTermColorInvalid(L"#11/1/");
    _VerifyXTermColorInvalid(L"#1111111");
    _VerifyXTermColorInvalid(L"#/1/1/1");
    _VerifyXTermColorInvalid(L"#rgb:1/1/1");
    _VerifyXTermColorInvalid(L"#111invalid");
    _VerifyXTermColorInvalid(L"#invalid111");
    _VerifyXTermColorInvalid(L"#1111111111111111");
    _VerifyXTermColorInvalid(L"12/34/56");
    _VerifyXTermColorInvalid(L"123456");
    _VerifyXTermColorInvalid(L"rgb：1/1/1");
    _VerifyXTermColorInvalid(L"中文rgb:1/1/1");
    _VerifyXTermColorInvalid(L"rgb中文:1/1/1");
    _VerifyXTermColorInvalid(L"这是一句中文");
    _VerifyXTermColorInvalid(L"RGBİ1/1/1");
    _VerifyXTermColorInvalid(L"rgbİ1/1/1");
    _VerifyXTermColorInvalid(L"rgbİ:1/1/1");
    _VerifyXTermColorInvalid(L"rgß:1/1/1");
    _VerifyXTermColorInvalid(L"rgẞ:1/1/1");
    _VerifyXTermColorInvalid(L"yellow8");
    _VerifyXTermColorInvalid(L"yellow10");
    _VerifyXTermColorInvalid(L"yellow3a");
    _VerifyXTermColorInvalid(L"3yellow");
    _VerifyXTermColorInvalid(L"royal3blue");
    _VerifyXTermColorInvalid(L"5gray");
    _VerifyXTermColorInvalid(L"5gray8");
    _VerifyXTermColorInvalid(L"58grey");
    _VerifyXTermColorInvalid(L"gray-1");
    _VerifyXTermColorInvalid(L"gray101");
    _VerifyXTermColorInvalid(L"gray-");
    _VerifyXTermColorInvalid(L"gray;");
}

void UtilsTests::_VerifyXTermColorResult(const std::wstring_view wstr, DWORD colorValue)
{
    std::optional<til::color> color = ColorFromXTermColor(wstr);
    VERIFY_IS_TRUE(color.has_value());
    VERIFY_ARE_EQUAL(colorValue, (COLORREF)color.value());
}

void UtilsTests::_VerifyXTermColorInvalid(const std::wstring_view wstr)
{
    std::optional<til::color> color = ColorFromXTermColor(wstr);
    VERIFY_IS_FALSE(color.has_value());
}
