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
    TEST_METHOD(TestGuidToString);
    TEST_METHOD(TestSplitString);
    TEST_METHOD(TestFilterStringForPaste);
    TEST_METHOD(TestStringToUint);
    TEST_METHOD(TestColorFromXTermColor);

#if !__INSIDE_WINDOWS
    TEST_METHOD(TestMangleWSLPaths);
#endif

    TEST_METHOD(TestTrimTrailingWhitespace);
    TEST_METHOD(TestDontTrimTrailingWhitespace);

    TEST_METHOD(TestEvaluateStartingDirectory);

    void _VerifyXTermColorResult(const std::wstring_view wstr, DWORD colorValue);
    void _VerifyXTermColorInvalid(const std::wstring_view wstr);
};

void UtilsTests::TestClampToShortMax()
{
    const short min = 1;

    // Test outside the lower end of the range
    const auto minExpected = min;
    auto minActual = ClampToShortMax(0, min);
    VERIFY_ARE_EQUAL(minExpected, minActual);

    // Test negative numbers
    const auto negativeExpected = min;
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

void UtilsTests::TestFilterStringForPaste()
{
    // Test carriage return
    const std::wstring noNewLine = L"Hello World";
    VERIFY_ARE_EQUAL(L"Hello World", FilterStringForPaste(noNewLine, FilterOption::CarriageReturnNewline));

    const std::wstring singleCR = L"Hello World\r";
    VERIFY_ARE_EQUAL(L"Hello World\r", FilterStringForPaste(singleCR, FilterOption::CarriageReturnNewline));

    const std::wstring singleLF = L"Hello World\n";
    VERIFY_ARE_EQUAL(L"Hello World\r", FilterStringForPaste(singleLF, FilterOption::CarriageReturnNewline));

    const std::wstring singleCRLF = L"Hello World\r\n";
    VERIFY_ARE_EQUAL(L"Hello World\r", FilterStringForPaste(singleCRLF, FilterOption::CarriageReturnNewline));

    const std::wstring multiCR = L"Hello\rWorld\r";
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r", FilterStringForPaste(multiCR, FilterOption::CarriageReturnNewline));

    const std::wstring multiLF = L"Hello\nWorld\n";
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r", FilterStringForPaste(multiLF, FilterOption::CarriageReturnNewline));

    const std::wstring multiCRLF = L"Hello\r\nWorld\r\n";
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r", FilterStringForPaste(multiCRLF, FilterOption::CarriageReturnNewline));

    const std::wstring multiCR_NoNewLine = L"Hello\rWorld\r123";
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r123", FilterStringForPaste(multiCR_NoNewLine, FilterOption::CarriageReturnNewline));

    const std::wstring multiLF_NoNewLine = L"Hello\nWorld\n123";
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r123", FilterStringForPaste(multiLF_NoNewLine, FilterOption::CarriageReturnNewline));

    const std::wstring multiCRLF_NoNewLine = L"Hello\r\nWorld\r\n123";
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r123", FilterStringForPaste(multiCRLF_NoNewLine, FilterOption::CarriageReturnNewline));

    // Test control code filtering
    const std::wstring noNewLineWithControlCodes = L"Hello\x01\x02\x03 123";
    VERIFY_ARE_EQUAL(L"Hello 123", FilterStringForPaste(noNewLineWithControlCodes, FilterOption::ControlCodes));

    const std::wstring singleCRWithControlCodes = L"Hello World\r\x01\x02\x03 123";
    VERIFY_ARE_EQUAL(L"Hello World\r 123", FilterStringForPaste(singleCRWithControlCodes, FilterOption::ControlCodes));

    const std::wstring singleLFWithControlCodes = L"Hello World\n\x01\x02\x03 123";
    VERIFY_ARE_EQUAL(L"Hello World\n 123", FilterStringForPaste(singleLFWithControlCodes, FilterOption::ControlCodes));

    const std::wstring singleCRLFWithControlCodes = L"Hello World\r\n\x01\x02\x03 123";
    VERIFY_ARE_EQUAL(L"Hello World\r\n 123", FilterStringForPaste(singleCRLFWithControlCodes, FilterOption::ControlCodes));

    VERIFY_ARE_EQUAL(L"Hello World\r 123", FilterStringForPaste(singleCRWithControlCodes, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));
    VERIFY_ARE_EQUAL(L"Hello World\r 123", FilterStringForPaste(singleLFWithControlCodes, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));
    VERIFY_ARE_EQUAL(L"Hello World\r 123", FilterStringForPaste(singleCRLFWithControlCodes, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));

    const std::wstring multiCRWithControlCodes = L"Hello\r\x01\x02\x03World\r\x01\x02\x03 123";
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r 123", FilterStringForPaste(multiCRWithControlCodes, FilterOption::ControlCodes));

    const std::wstring multiLFWithControlCodes = L"Hello\n\x01\x02\x03World\n\x01\x02\x03 123";
    VERIFY_ARE_EQUAL(L"Hello\nWorld\n 123", FilterStringForPaste(multiLFWithControlCodes, FilterOption::ControlCodes));

    const std::wstring multiCRLFWithControlCodes = L"Hello\r\nWorld\r\n\x01\x02\x03 123";
    VERIFY_ARE_EQUAL(L"Hello\r\nWorld\r\n 123", FilterStringForPaste(multiCRLFWithControlCodes, FilterOption::ControlCodes));

    VERIFY_ARE_EQUAL(L"Hello\rWorld\r 123", FilterStringForPaste(multiCRWithControlCodes, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r 123", FilterStringForPaste(multiLFWithControlCodes, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));
    VERIFY_ARE_EQUAL(L"Hello\rWorld\r 123", FilterStringForPaste(multiCRLFWithControlCodes, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));

    const std::wstring multiLineWithLotsOfControlCodes = L"e\bc\bh\bo\b \b'.\b!\b:\b\b \bke\bS\b \bi3\bl \bld\bK\bo\b -1\b+\b9 +\b2\b-1'\b >\b \b/\bt\bm\bp\b/\bl\bo\bl\b\r\nsleep 1\r\nmd5sum /tmp/lol";

    VERIFY_ARE_EQUAL(L"echo '.!: keS i3l ldKo -1+9 +2-1' > /tmp/lol\rsleep 1\rmd5sum /tmp/lol",
                     FilterStringForPaste(multiLineWithLotsOfControlCodes, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));

    // Malicious string that tries to prematurely terminate bracketed
    const std::wstring malicious = L"echo\x1b[201~";
    VERIFY_ARE_EQUAL(L"echo[201~", FilterStringForPaste(malicious, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));

    // C1 control codes
    const std::wstring c1ControlCodes = L"echo\x9c";
    VERIFY_ARE_EQUAL(L"echo", FilterStringForPaste(c1ControlCodes, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));

    // Test Unicode content
    const std::wstring unicodeString = L"你好\r\n\x01世界\x02\r\n123";
    VERIFY_ARE_EQUAL(L"你好\r世界\r123",
                     FilterStringForPaste(unicodeString, FilterOption::CarriageReturnNewline | FilterOption::ControlCodes));
}

void UtilsTests::TestStringToUint()
{
    auto success = false;
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
    const auto color = ColorFromXTermColor(wstr);
    VERIFY_IS_TRUE(color.has_value());
    VERIFY_ARE_EQUAL(colorValue, static_cast<COLORREF>(color.value()));
}

void UtilsTests::_VerifyXTermColorInvalid(const std::wstring_view wstr)
{
    auto color = ColorFromXTermColor(wstr);
    VERIFY_IS_FALSE(color.has_value());
}

#if !__INSIDE_WINDOWS
// Windows' compiler dislikes these raw strings...
void UtilsTests::TestMangleWSLPaths()
{
    // Continue on failures
    const WEX::TestExecution::DisableVerifyExceptions disableExceptionsScope;

    const auto startingDirectory{ L"SENTINEL" };
    // MUST MANGLE
    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "SENTINEL" )", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl -d X)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "SENTINEL" -d X)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl -d X ~/bin/sh)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "SENTINEL" -d X ~/bin/sh)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl.exe)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl.exe" --cd "SENTINEL" )", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl.exe -d X)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl.exe" --cd "SENTINEL" -d X)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl.exe -d X ~/bin/sh)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl.exe" --cd "SENTINEL" -d X ~/bin/sh)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"("wsl")", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "SENTINEL" )", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"("wsl.exe")", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl.exe" --cd "SENTINEL" )", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"("wsl" -d X)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "SENTINEL"  -d X)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"("wsl.exe" -d X)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl.exe" --cd "SENTINEL"  -d X)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"("C:\Windows\system32\wsl.exe" -d X)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("C:\Windows\system32\wsl.exe" --cd "SENTINEL"  -d X)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"("C:\windows\system32\wsl" -d X)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("C:\windows\system32\wsl" --cd "SENTINEL"  -d X)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl ~/bin)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "SENTINEL" ~/bin)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    // MUST NOT MANGLE
    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"("C:\wsl.exe" -d X)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"("C:\wsl.exe" -d X)", commandline);
        VERIFY_ARE_EQUAL(startingDirectory, path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(C:\wsl.exe)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"(C:\wsl.exe)", commandline);
        VERIFY_ARE_EQUAL(startingDirectory, path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl --cd C:\)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"(wsl --cd C:\)", commandline);
        VERIFY_ARE_EQUAL(startingDirectory, path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl ~)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"(wsl ~)", commandline);
        VERIFY_ARE_EQUAL(startingDirectory, path);
    }

    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl ~ -d Ubuntu)", startingDirectory);
        VERIFY_ARE_EQUAL(LR"(wsl ~ -d Ubuntu)", commandline);
        VERIFY_ARE_EQUAL(startingDirectory, path);
    }

    {
        // Test for GH#11994 - make sure `//wsl$/` paths get mangled back to
        // `\\wsl$\`, to workaround a potential bug in `wsl --cd`
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl -d Ubuntu)", LR"(//wsl$/Ubuntu/home/user)");
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "\\wsl$\Ubuntu\home\user" -d Ubuntu)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }
    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl -d Ubuntu)", LR"(\\wsl$\Ubuntu\home\user)");
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "\\wsl$\Ubuntu\home\user" -d Ubuntu)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    {
        // Same, but with `wsl.localhost`
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl -d Ubuntu)", LR"(//wsl.localhost/Ubuntu/home/user)");
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "\\wsl.localhost\Ubuntu\home\user" -d Ubuntu)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }
    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl -d Ubuntu)", LR"(\\wsl.localhost\Ubuntu\home\user)");
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "\\wsl.localhost\Ubuntu\home\user" -d Ubuntu)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }

    /// Tests for GH #12353

    const auto expectedUserProfilePath = wil::ExpandEnvironmentStringsW<std::wstring>(L"%USERPROFILE%");
    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl -d Ubuntu)", L"~");
        VERIFY_ARE_EQUAL(LR"("wsl" --cd "~" -d Ubuntu)", commandline);
        VERIFY_ARE_EQUAL(L"", path);
    }
    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(wsl ~ -d Ubuntu)", L"~");
        VERIFY_ARE_EQUAL(LR"(wsl ~ -d Ubuntu)", commandline);
        VERIFY_ARE_EQUAL(expectedUserProfilePath, path);
    }
    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(ubuntu ~ -d Ubuntu)", L"~");
        VERIFY_ARE_EQUAL(LR"(ubuntu ~ -d Ubuntu)", commandline);
        VERIFY_ARE_EQUAL(expectedUserProfilePath, path);
    }
    {
        auto [commandline, path] = MangleStartingDirectoryForWSL(LR"(powershell.exe)", L"~");
        VERIFY_ARE_EQUAL(LR"(powershell.exe)", commandline);
        VERIFY_ARE_EQUAL(expectedUserProfilePath, path);
    }
}
#endif

void UtilsTests::TestTrimTrailingWhitespace()
{
    // Continue on failures
    const WEX::TestExecution::DisableVerifyExceptions disableExceptionsScope;

    // Tests for GH #11473
    VERIFY_ARE_EQUAL(L"Foo", TrimPaste(L"Foo   "));
    VERIFY_ARE_EQUAL(L"Foo", TrimPaste(L"Foo\n"));
    VERIFY_ARE_EQUAL(L"Foo", TrimPaste(L"Foo\n\n"));
    VERIFY_ARE_EQUAL(L"Foo", TrimPaste(L"Foo\r\n"));
    VERIFY_ARE_EQUAL(L"Foo Bar", TrimPaste(L"Foo Bar\n"));
    VERIFY_ARE_EQUAL(L"Foo\tBar", TrimPaste(L"Foo\tBar\n"));

    VERIFY_ARE_EQUAL(L"Foo Bar", TrimPaste(L"Foo Bar\t"), L"Trim when there is a tab at the end.");
    VERIFY_ARE_EQUAL(L"Foo Bar", TrimPaste(L"Foo Bar\t\t"), L"Trim when there are tabs at the end.");
    VERIFY_ARE_EQUAL(L"Foo Bar", TrimPaste(L"Foo Bar\t\n"), L"Trim when there are tabs at the start of the whitespace at the end.");
    VERIFY_ARE_EQUAL(L"Foo\tBar", TrimPaste(L"Foo\tBar\t\n"), L"Trim when there are tabs in the middle of the string, and in the whitespace at the end.");
    VERIFY_ARE_EQUAL(L"Foo\tBar", TrimPaste(L"Foo\tBar\n\t"), L"Trim when there are tabs in the middle of the string, and in the whitespace at the end.");
    VERIFY_ARE_EQUAL(L"Foo\tBar", TrimPaste(L"Foo\tBar\t\n\t"), L"Trim when there are tabs in the middle of the string, and in the whitespace at the end.");
}
void UtilsTests::TestDontTrimTrailingWhitespace()
{
    // Continue on failures
    const WEX::TestExecution::DisableVerifyExceptions disableExceptionsScope;

    VERIFY_ARE_EQUAL(L"Foo\tBar", TrimPaste(L"Foo\tBar"));

    // Tests for GH #12387
    VERIFY_ARE_EQUAL(L"Foo\nBar\n", TrimPaste(L"Foo\nBar\n"));
    VERIFY_ARE_EQUAL(L"Foo  Baz\nBar\n", TrimPaste(L"Foo  Baz\nBar\n"));
    VERIFY_ARE_EQUAL(L"Foo\tBaz\nBar\n", TrimPaste(L"Foo\tBaz\nBar\n"), L"Don't trim when there's a trailing newline, and tabs in the middle");
    VERIFY_ARE_EQUAL(L"Foo\tBaz\nBar\t\n", TrimPaste(L"Foo\tBaz\nBar\t\n"), L"Don't trim when there's a trailing newline, and tabs in the middle");

    // We need to both
    // * trim when there's a tab followed by only whitespace
    // * not trim then there's a tab in the middle, and the string ends in whitespace
}

void UtilsTests::TestEvaluateStartingDirectory()
{
    // Continue on failures
    const WEX::TestExecution::DisableVerifyExceptions disableExceptionsScope;

    auto test = [](auto& expected, auto& cwd, auto& startingDir) {
        VERIFY_ARE_EQUAL(expected, EvaluateStartingDirectory(cwd, startingDir));
    };

    // A NOTE: EvaluateStartingDirectory makes no attempt to cannonicalize the
    // path. So if you do any sort of relative paths, it'll literally just
    // append.

    {
        std::wstring cwd = L"C:\\Windows\\System32";

        // Literally blank
        test(L"C:\\Windows\\System32\\", cwd, L"");

        // Absolute Windows path
        test(L"C:\\Windows", cwd, L"C:\\Windows");
        test(L"C:/Users/migrie", cwd, L"C:/Users/migrie");

        // Relative Windows path
        test(L"C:\\Windows\\System32\\.", cwd, L"."); // ?
        test(L"C:\\Windows\\System32\\.\\System32", cwd, L".\\System32"); // ?
        test(L"C:\\Windows\\System32\\./dev", cwd, L"./dev");

        // WSL '~' path
        test(L"~", cwd, L"~");
        test(L"~/dev", cwd, L"~/dev");

        // WSL or Windows / path - this will ultimately be evaluated by the connection
        test(L"/", cwd, L"/");
        test(L"/dev", cwd, L"/dev");
    }

    {
        std::wstring cwd = L"C:/Users/migrie";

        // Literally blank
        test(L"C:/Users/migrie\\", cwd, L"");

        // Absolute Windows path
        test(L"C:\\Windows", cwd, L"C:\\Windows");
        test(L"C:/Users/migrie", cwd, L"C:/Users/migrie");

        // Relative Windows path
        test(L"C:/Users/migrie\\.", cwd, L"."); // ?
        test(L"C:/Users/migrie\\.\\System32", cwd, L".\\System32"); // ?
        test(L"C:/Users/migrie\\./dev", cwd, L"./dev");

        // WSL '~' path
        test(L"~", cwd, L"~");
        test(L"~/dev", cwd, L"~/dev");

        // WSL or Windows / path - this will ultimately be evaluated by the connection
        test(L"/", cwd, L"/");
        test(L"/dev", cwd, L"/dev");
    }
}
