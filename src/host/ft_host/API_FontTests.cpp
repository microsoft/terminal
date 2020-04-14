// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::TestExecution;
using namespace WEX::Common;
using WEX::Logging::Log;

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

    const bool bUseValidOutputHandle = (dwConsoleOutput == 1);
    HANDLE hConsoleOutput;
    if (bUseValidOutputHandle)
    {
        hConsoleOutput = GetStdOutputHandle();
    }
    else
    {
        hConsoleOutput = (HANDLE)dwConsoleOutput;
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

    COORD coordFontSize = OneCoreDelay::GetConsoleFontSize((HANDLE)dwConsoleOutput, 0);
    VERIFY_ARE_EQUAL(coordFontSize, c_coordZero, L"Ensure (0,0) coord returned to indicate failure");
    VERIFY_ARE_EQUAL(GetLastError(), (DWORD)ERROR_INVALID_HANDLE, L"Ensure last error was set appropriately");
}

void FontTests::TestGetFontSizeLargeIndexInvalid()
{
    SetLastError(0);
    COORD coordFontSize = OneCoreDelay::GetConsoleFontSize(GetStdOutputHandle(), 0xFFFFFFFF);
    VERIFY_ARE_EQUAL(coordFontSize, c_coordZero, L"Ensure (0,0) coord returned to indicate failure");
    VERIFY_ARE_EQUAL(GetLastError(), (DWORD)ERROR_INVALID_PARAMETER, L"Ensure last error was set appropriately");
}

void FontTests::TestSetConsoleFontNegativeSize()
{
    const HANDLE hConsoleOutput = GetStdOutputHandle();
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
    const HANDLE hConsoleOutput = GetStdOutputHandle();

    Log::Comment(L"1. Ensure that the various GET APIs for font information align with each other.");
    CONSOLE_FONT_INFOEX cfie = { 0 };
    cfie.cbSize = sizeof(cfie);
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cfie));

    CONSOLE_FONT_INFO cfi = { 0 };
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFont(hConsoleOutput, FALSE, &cfi));

    VERIFY_ARE_EQUAL(cfi.nFont, cfie.nFont, L"Ensure regular and Ex APIs return same nFont");
    VERIFY_ARE_NOT_EQUAL(cfi.dwFontSize, c_coordZero, L"Ensure non-zero font size");
    VERIFY_ARE_EQUAL(cfi.dwFontSize, cfie.dwFontSize, L"Ensure regular and Ex APIs return same dwFontSize");

    const COORD coordCurrentFontSize = OneCoreDelay::GetConsoleFontSize(hConsoleOutput, cfi.nFont);
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

    // Ensure that the two values we attempted to set did accurately round-trip through the API.
    // The other unspecified values may have been adjusted/updated by GDI.
    if (0 != NoThrowString(cfieSet.FaceName).CompareNoCase(cfiePost.FaceName))
    {
        Log::Comment(L"We cannot test changing fonts on systems that do not have alternatives available. Skipping test.");
        Log::Result(WEX::Logging::TestResults::Result::Skipped);
        return;
    }
    VERIFY_ARE_EQUAL(cfieSet.dwFontSize.Y, cfiePost.dwFontSize.Y);

    // Ensure that the entire structure we received matches what we expect to usually get for this Lucida Console Size 12 ask.
    CONSOLE_FONT_INFOEX cfieFullExpected = { 0 };
    cfieFullExpected.cbSize = sizeof(cfieFullExpected);
    wcscpy_s(cfieFullExpected.FaceName, L"Lucida Console");

    if (!OneCoreDelay::IsIsWindowPresent())
    {
        // On OneCore Windows without GDI, this is what we expect to get.
        cfieFullExpected.dwFontSize.X = 8;
        cfieFullExpected.dwFontSize.Y = 12;
        cfieFullExpected.FontFamily = 4;
        cfieFullExpected.FontWeight = 0;
    }
    else
    {
        // On client Windows with GDI, this is what we expect to get.
        cfieFullExpected.dwFontSize.X = 7;
        cfieFullExpected.dwFontSize.Y = 12;
        cfieFullExpected.FontFamily = 54;
        cfieFullExpected.FontWeight = 400;
    }

    VERIFY_ARE_EQUAL(cfieFullExpected, cfiePost);
}

void FontTests::TestLongFontNameScenario()
{
    std::wstring expandedLongFontPath = wil::ExpandEnvironmentStringsW<std::wstring>(pwszLongFontPath);

    if (!CheckIfFileExists(expandedLongFontPath.c_str()))
    {
        Log::Comment(L"Lucida Sans Typewriter doesn't exist; skipping long font test.");
        Log::Result(WEX::Logging::TestResults::Result::Skipped);
        return;
    }

    const HANDLE hConsoleOutput = GetStdOutputHandle();

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

    const HANDLE hConsoleOutput = GetStdOutputHandle();
    const HWND hwnd = GetConsoleWindow();
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

    Log::Comment(L"Adjust console window to Lucida Console 12.");
    wcscpy_s(cfiex.FaceName, L"Lucida Console");
    cfiex.dwFontSize.Y = 12;

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
