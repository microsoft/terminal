// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "..\inc\utils.hpp"
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

    gsl::span<COLORREF> terminalTableView = { &terminalTable[0], gsl::narrow<ptrdiff_t>(terminalTable.size()) };
    gsl::span<COLORREF> consoleTableView = { &consoleTable[0], gsl::narrow<ptrdiff_t>(consoleTable.size()) };

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
