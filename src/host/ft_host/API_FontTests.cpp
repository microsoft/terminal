// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace WEX::Logging;

static const COORD c_coordZero = { 0, 0 };

static const PCWSTR pwszLongFontPath = L"%WINDIR%\\Fonts\\ltype.ttf";

class FontTests
{
    BEGIN_TEST_CLASS(FontTests)
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    BEGIN_TEST_METHOD(TestCurrentFontAPIsInvalid)
        TEST_METHOD_PROPERTY(L"Data:dwConsoleOutput", L"{0, 1, 0xFFFFFFFF}")
        TEST_METHOD_PROPERTY(L"Data:bMaximumWindow", L"{TRUE, FALSE}")
        TEST_METHOD_PROPERTY(L"Data:strOperation", L"{Get, GetEx, SetEx}")
    END_TEST_METHOD();

    BEGIN_TEST_METHOD(TestGetFontSizeInvalid)
        TEST_METHOD_PROPERTY(L"Data:dwConsoleOutput", L"{0, 0xFFFFFFFF}")
    END_TEST_METHOD();

    TEST_METHOD(TestGetFontSizeLargeIndexInvalid);
    TEST_METHOD(TestSetConsoleFontNegativeSize);

    TEST_METHOD(TestFontScenario);
    TEST_METHOD(TestLongFontNameScenario);

    TEST_METHOD(TestSetFontAdjustsWindow);
};

bool FontTests::TestSetup()
{
    SetVerifyOutput verifySettings(VerifyOutputSettings::LogOnlyFailures);
    return true;
}

bool FontTests::TestCleanup()
{
    SetVerifyOutput verifySettings(VerifyOutputSettings::LogOnlyFailures);
    return true;
}

void FontTests::TestCurrentFontAPIsInvalid()
{
    DWORD dwConsoleOutput;
    bool bMaximumWindow;
    String strOperation;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dwConsoleOutput", dwConsoleOutput), L"Get output handle value");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"bMaximumWindow", bMaximumWindow), L"Get maximized window value");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"strOperation", strOperation), L"Get operation value");

    const auto bUseValidOutputHandle = (dwConsoleOutput == 1);
    HANDLE hConsoleOutput;
    if (bUseValidOutputHandle)
    {
        hConsoleOutput = GetStdOutputHandle();
    }
    else
    {
        hConsoleOutput = ULongToHandle(dwConsoleOutput);
    }

    if (strOperation == L"Get")
    {
        CONSOLE_FONT_INFO cfi = { 0 };

        if (bUseValidOutputHandle)
        {
            VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFont(hConsoleOutput, (BOOL)bMaximumWindow, &cfi));
        }
        else
        {
            VERIFY_WIN32_BOOL_FAILED(OneCoreDelay::GetCurrentConsoleFont(hConsoleOutput, (BOOL)bMaximumWindow, &cfi));
        }
    }
    else if (strOperation == L"GetEx")
    {
        CONSOLE_FONT_INFOEX cfie = { 0 };
        VERIFY_WIN32_BOOL_FAILED(OneCoreDelay::GetCurrentConsoleFontEx(hConsoleOutput, (BOOL)bMaximumWindow, &cfie));
    }
    else if (strOperation == L"SetEx")
    {
        CONSOLE_FONT_INFOEX cfie = { 0 };
        VERIFY_WIN32_BOOL_FAILED(OneCoreDelay::SetCurrentConsoleFontEx(hConsoleOutput, (BOOL)bMaximumWindow, &cfie));
    }
    else
    {
        VERIFY_FAIL(L"Unrecognized operation");
    }
}

void FontTests::TestGetFontSizeInvalid()
{
    DWORD dwConsoleOutput;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dwConsoleOutput", dwConsoleOutput), L"Get input handle value");

    // Need to make sure that last error is cleared so that we can verify that lasterror was set by GetConsoleFontSize
    SetLastError(0);

    auto coordFontSize = OneCoreDelay::GetConsoleFontSize(ULongToHandle(dwConsoleOutput), 0);
    VERIFY_ARE_EQUAL(coordFontSize, c_coordZero, L"Ensure (0,0) coord returned to indicate failure");
    VERIFY_ARE_EQUAL(GetLastError(), (DWORD)ERROR_INVALID_HANDLE, L"Ensure last error was set appropriately");
}

void FontTests::TestGetFontSizeLargeIndexInvalid()
{
    SetLastError(0);
    auto coordFontSize = OneCoreDelay::GetConsoleFontSize(GetStdOutputHandle(), 0xFFFFFFFF);
    VERIFY_ARE_EQUAL(coordFontSize, c_coordZero, L"Ensure (0,0) coord returned to indicate failure");
    VERIFY_ARE_EQUAL(GetLastError(), (DWORD)ERROR_INVALID_PARAMETER, L"Ensure last error was set appropriately");
}

void FontTests::TestSetConsoleFontNegativeSize()
{
    const auto hConsoleOutput = GetStdOutputHandle();
    CONSOLE_FONT_INFOEX cfie = { 0 };
    cfie.cbSize = sizeof(cfie);
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfie));
    cfie.dwFontSize.X = -4;
    cfie.dwFontSize.Y = -12;

    // as strange as it sounds, we don't filter out negative font sizes. under the hood, this call ends up in
    // FindCreateFont, which runs through our list of loaded fonts, fails to find, takes the absolute value of Y, and
    // then performs a GDI font enumeration for fonts that match. we should hold on to this behavior until we can
    // establish that it's no longer necessary.
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfie));
}

void FontTests::TestFontScenario()
{
    const auto hConsoleOutput = GetStdOutputHandle();

    Log::Comment(L"1. Ensure that the various GET APIs for font information align with each other.");
    CONSOLE_FONT_INFOEX cfie = { 0 };
    cfie.cbSize = sizeof(cfie);
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfie));

    CONSOLE_FONT_INFO cfi = { 0 };
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFont(hConsoleOutput, FALSE, &cfi));

    VERIFY_ARE_EQUAL(cfi.nFont, cfie.nFont, L"Ensure regular and Ex APIs return same nFont");
    VERIFY_ARE_NOT_EQUAL(cfi.dwFontSize, c_coordZero, L"Ensure non-zero font size");
    VERIFY_ARE_EQUAL(cfi.dwFontSize, cfie.dwFontSize, L"Ensure regular and Ex APIs return same dwFontSize");

    const auto coordCurrentFontSize = OneCoreDelay::GetConsoleFontSize(hConsoleOutput, cfi.nFont);
    VERIFY_ARE_EQUAL(coordCurrentFontSize, cfi.dwFontSize, L"Ensure GetConsoleFontSize output matches GetCurrentConsoleFont");

    // ---------------------

    Log::Comment(L"2. Ensure that our font settings round-trip appropriately through the Ex APIs");
    CONSOLE_FONT_INFOEX cfieSet = { 0 };
    cfieSet.cbSize = sizeof(cfieSet);
    cfieSet.dwFontSize.Y = 12;
    VERIFY_SUCCEEDED(StringCchCopy(cfieSet.FaceName, ARRAYSIZE(cfieSet.FaceName), L"Lucida Console"));

    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfieSet));

    CONSOLE_FONT_INFOEX cfiePost = { 0 };
    cfiePost.cbSize = sizeof(cfiePost);
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfiePost));

    VERIFY_IS_GREATER_THAN(cfiePost.dwFontSize.X, 0);
    VERIFY_IS_GREATER_THAN(cfiePost.dwFontSize.Y, 0);
    auto cfieFullExpected = cfieSet;
    cfieFullExpected.dwFontSize = cfiePost.dwFontSize;
    VERIFY_ARE_EQUAL(cfieFullExpected, cfiePost);
    VERIFY_ARE_EQUAL(cfiePost.dwFontSize, OneCoreDelay::GetConsoleFontSize(hConsoleOutput, cfiePost.nFont));

    Log::Comment(L"3. Ensure that a fully specified cell size round-trips unchanged.");
    cfieSet.dwFontSize = { 8, 12 };
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfieSet));
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfiePost));
    VERIFY_ARE_EQUAL(cfieSet, cfiePost);
    VERIFY_ARE_EQUAL(cfieSet.dwFontSize, OneCoreDelay::GetConsoleFontSize(hConsoleOutput, cfiePost.nFont));
}

void FontTests::TestLongFontNameScenario()
{
    auto expandedLongFontPath = wil::ExpandEnvironmentStringsW<std::wstring>(pwszLongFontPath);

    if (!CheckIfFileExists(expandedLongFontPath.c_str()))
    {
        Log::Comment(L"Lucida Sans Typewriter doesn't exist; skipping long font test.");
        Log::Result(WEX::Logging::TestResults::Result::Skipped);
        return;
    }

    const auto hConsoleOutput = GetStdOutputHandle();

    CONSOLE_FONT_INFOEX cfieSetLong = { 0 };
    cfieSetLong.cbSize = sizeof(cfieSetLong);
    cfieSetLong.FontFamily = 54;
    cfieSetLong.dwFontSize.Y = 12;
    VERIFY_SUCCEEDED(StringCchCopy(cfieSetLong.FaceName, ARRAYSIZE(cfieSetLong.FaceName), L"Lucida Sans Typewriter"));

    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfieSetLong));

    CONSOLE_FONT_INFOEX cfiePostLong = { 0 };
    cfiePostLong.cbSize = sizeof(cfiePostLong);
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfiePostLong));

    Log::Comment(NoThrowString().Format(L"%ls %ls", cfieSetLong.FaceName, cfiePostLong.FaceName));

    VERIFY_ARE_EQUAL(0, NoThrowString(cfieSetLong.FaceName).CompareNoCase(cfiePostLong.FaceName));
}

void FontTests::TestSetFontAdjustsWindow()
{
    if (!OneCoreDelay::IsIsWindowPresent())
    {
        Log::Comment(L"Adjusting window size by changing font scenario can't be checked on platform without classic window operations.");
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    const auto hConsoleOutput = GetStdOutputHandle();
    const auto hwnd = GetConsoleWindow();
    VERIFY_IS_TRUE(!!IsWindow(hwnd));
    RECT rc = { 0 };

    CONSOLE_FONT_INFOEX cfiex = { 0 };
    cfiex.cbSize = sizeof(cfiex);

    Log::Comment(L"First set the console window to Consolas 16.");
    wcscpy_s(cfiex.FaceName, L"Consolas");
    cfiex.dwFontSize.Y = 16;

    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfiex));
    Sleep(250);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetClientRect(hwnd, &rc), L"Retrieve client rectangle size for Consolas 16.");
    SIZE szConsolas;
    szConsolas.cx = rc.right - rc.left;
    szConsolas.cy = rc.bottom - rc.top;
    Log::Comment(NoThrowString().Format(L"Client rect size is (X: %d, Y: %d)", szConsolas.cx, szConsolas.cy));

    Log::Comment(L"Adjust console window to Lucida Console 8.");
    wcscpy_s(cfiex.FaceName, L"Lucida Console");
    cfiex.dwFontSize.Y = 8;

    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfiex));
    Sleep(250);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetClientRect(hwnd, &rc), L"Retrieve client rectangle size for Lucida Console 12.");
    SIZE szLucida;
    szLucida.cx = rc.right - rc.left;
    szLucida.cy = rc.bottom - rc.top;

    Log::Comment(NoThrowString().Format(L"Client rect size is (X: %d, Y: %d)", szLucida.cx, szLucida.cy));
    Log::Comment(L"Window should shrink in size when going to Lucida 12 from Consolas 16.");
    VERIFY_IS_LESS_THAN(szLucida.cx, szConsolas.cx);
    VERIFY_IS_LESS_THAN(szLucida.cy, szConsolas.cy);

    Log::Comment(L"Adjust console window back to Consolas 16.");
    wcscpy_s(cfiex.FaceName, L"Consolas");
    cfiex.dwFontSize.Y = 16;

    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfiex));
    Sleep(250);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetClientRect(hwnd, &rc), L"Retrieve client rectangle size for Consolas 16.");
    szConsolas.cx = rc.right - rc.left;
    szConsolas.cy = rc.bottom - rc.top;

    Log::Comment(NoThrowString().Format(L"Client rect size is (X: %d, Y: %d)", szConsolas.cx, szConsolas.cy));
    Log::Comment(L"Window should grow in size when going from Lucida 12 to Consolas 16.");
    VERIFY_IS_LESS_THAN(szLucida.cx, szConsolas.cx);
    VERIFY_IS_LESS_THAN(szLucida.cy, szConsolas.cy);
}
