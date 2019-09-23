// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::TestExecution;
using WEX::Logging::Log;
using namespace WEX::Common;

// This class is intended to test:
// GetConsoleScreenBufferInfo
// GetConsoleScreenBufferInfoEx
// GetLargestConsoleWindowSize
// SetConsoleScreenBufferInfoEx --> SetScreenBufferInfo internally
// SetConsoleScreenBufferSize
// SetConsoleWindowInfo
class DimensionsTests
{
    BEGIN_TEST_CLASS(DimensionsTests)
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestGetLargestConsoleWindowSize);
    TEST_METHOD(TestGetConsoleScreenBufferInfoAndEx);

    BEGIN_TEST_METHOD(TestSetConsoleWindowInfo)
        // This needs to run in both absolute and relative modes.
        TEST_METHOD_PROPERTY(L"Data:bAbsolute", L"{TRUE, FALSE}")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestSetConsoleScreenBufferSize)
        // 0x1 = X, 0x2 = Y, 0x3 = Both
        TEST_METHOD_PROPERTY(L"Data:scaleChoices", L"{1, 2, 3}")
    END_TEST_METHOD()

    TEST_METHOD(TestZeroSizedConsoleScreenBuffers);
    TEST_METHOD(TestSetConsoleScreenBufferInfoEx);
};

bool DimensionsTests::TestSetup()
{
    return Common::TestBufferSetup();
}

bool DimensionsTests::TestCleanup()
{
    return Common::TestBufferCleanup();
}

void DimensionsTests::TestGetLargestConsoleWindowSize()
{
    if (!OneCoreDelay::IsIsWindowPresent())
    {
        Log::Comment(L"Largest window size scenario can't be checked on platform without classic window operations.");
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    // Note that this API is named "window size" but actually refers to the maximum viewport.
    // Viewport is defined as the character count that can fit within one client area of the window.
    // It has nothing to do with the outer pixel dimensions of the window.
    // To know the largest window size, we need:
    // - The size of the monitor that the console window is on
    // - The style of the window
    // - The current size of the font used within that window

    // The "largest window size" is the maximum number of rows and columns worth of characters
    // that can be displayed if the current console window was stretched as large as it is currently
    // allowed to be on the given monitor.

    // NOTE: The legacy behavior of this function (in v1) was to give the "full screen window" size as the largest
    // even if it was in windowed mode and wouldn't fit on the monitor.

    // Get the window handle
    HWND const hWindow = GetConsoleWindow();
    VerifySucceededGLE(VERIFY_IS_TRUE(!!IsWindow(hWindow), L"Get the window handle for the window."));

    // Get the dimensions of the monitor that the window is on.
    HMONITOR const hMonitor = MonitorFromWindow(hWindow, MONITOR_DEFAULTTONULL);
    VerifySucceededGLE(VERIFY_IS_NOT_NULL(hMonitor, L"Get the monitor handle corresponding to the console window."));

    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(MONITORINFO);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetMonitorInfoW(hMonitor, &mi), L"Get monitor information for the handle.");

    // Get the styles for the window from the handle
    DWORD const dwStyle = GetWindowStyle(hWindow);
    DWORD const dwStyleEx = GetWindowExStyle(hWindow);
    BOOL const bHasMenu = OneCoreDelay::GetMenu(hWindow) != nullptr;

    // Get the current font size
    CONSOLE_FONT_INFO cfi;
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFont(Common::_hConsole, FALSE, &cfi), L"Get the current console font structure.");

    // Now use what we've learned to attempt to calculate the expected size
    COORD coordExpected = { 0 };

    RECT rcPixels = mi.rcWork; // start from the monitor work area as the maximum pixel size

    // we have to adjust the work area by the size of the window borders to compensate for a maximized window
    // where the window manager will render the borders off the edges of the screen.
    WINDOWINFO wi = { 0 };
    wi.cbSize = sizeof(WINDOWINFO);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetWindowInfo(hWindow, &wi), L"Get window information to obtain window border sizes.");
    rcPixels.top -= wi.cyWindowBorders;
    rcPixels.bottom += wi.cyWindowBorders;
    rcPixels.left -= wi.cxWindowBorders;
    rcPixels.right += wi.cxWindowBorders;

    UnadjustWindowRectEx(&rcPixels, dwStyle, bHasMenu, dwStyleEx); // convert outer window dimensions into client area size

    // Do not reserve space for scroll bars.

    // Now take width and height and divide them by the size of a character to get the max character count.
    coordExpected.X = (SHORT)((rcPixels.right - rcPixels.left) / cfi.dwFontSize.X);
    coordExpected.Y = (SHORT)((rcPixels.bottom - rcPixels.top) / cfi.dwFontSize.Y);

    // Now finally ask the console what it thinks its largest size should be and compare.
    COORD const coordLargest = GetLargestConsoleWindowSize(Common::_hConsole);
    VerifySucceededGLE(VERIFY_IS_NOT_NULL(coordLargest, L"Now ask what the console thinks the largest size should be."));

    VERIFY_ARE_EQUAL(coordExpected, coordLargest, L"Compare what we calculated to what the console says the largest size should be.");
}

void DimensionsTests::TestGetConsoleScreenBufferInfoAndEx()
{
    // Get both structures
    CONSOLE_SCREEN_BUFFER_INFO sbi = { 0 };
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfo(Common::_hConsole, &sbi), L"Retrieve old-style buffer info.");
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiex), L"Retrieve extended buffer info.");

    Log::Comment(NoThrowString().Format(L"Verify overlapping values are the same between both call types."));

    VERIFY_ARE_EQUAL(sbi.dwCursorPosition, sbiex.dwCursorPosition);
    VERIFY_ARE_EQUAL(sbi.dwMaximumWindowSize, sbi.dwMaximumWindowSize);
    VERIFY_ARE_EQUAL(sbi.dwSize, sbiex.dwSize);
    VERIFY_ARE_EQUAL(sbi.srWindow, sbiex.srWindow);
    VERIFY_ARE_EQUAL(sbi.wAttributes, sbiex.wAttributes);
}

void ConvertAbsoluteToRelative(bool const bAbsolute, SMALL_RECT* const srViewport, const SMALL_RECT* const srOriginalWindow)
{
    if (!bAbsolute)
    {
        srViewport->Left -= srOriginalWindow->Left;
        srViewport->Right -= srOriginalWindow->Right;
        srViewport->Top -= srOriginalWindow->Top;
        srViewport->Bottom -= srOriginalWindow->Bottom;
    }
}

void TestSetConsoleWindowInfoHelper(bool const bAbsolute,
                                    const SMALL_RECT* const srViewport,
                                    const SMALL_RECT* const srOriginalViewport,
                                    bool const bExpectedResult,
                                    PCWSTR pwszDescription)
{
    SMALL_RECT srTest = *srViewport;

    ConvertAbsoluteToRelative(bAbsolute, &srTest, srOriginalViewport);

    Log::Comment(NoThrowString().Format(L"Abs:%s Original:%s Viewport:%s",
                                        bAbsolute ? L"True" : L"False",
                                        VerifyOutputTraits<SMALL_RECT>::ToString(*srOriginalViewport).ToCStrWithFallbackTo(L"Fail To Display SMALL_RECT"),
                                        VerifyOutputTraits<SMALL_RECT>::ToString(srTest).ToCStrWithFallbackTo(L"Fail To Display SMALL_RECT")));

    if (bExpectedResult)
    {
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleWindowInfo(Common::_hConsole, bAbsolute, &srTest), pwszDescription);
    }
    else
    {
        VERIFY_WIN32_BOOL_FAILED(SetConsoleWindowInfo(Common::_hConsole, bAbsolute, &srTest), pwszDescription);
    }
}

void DimensionsTests::TestSetConsoleWindowInfo()
{
    bool bAbsolute;
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"bAbsolute", bAbsolute), L"Get absolute vs. relative parameter");

    // Get window and buffer information
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiex), L"Get initial buffer and window information.");

    SMALL_RECT srViewport = { 0 };

    // Test with and without absolute
    // Left > Right, Top > Bottom (INVALID)
    srViewport.Right = sbiex.srWindow.Left;
    srViewport.Left = sbiex.srWindow.Right;
    srViewport.Bottom = sbiex.srWindow.Top;
    srViewport.Top = sbiex.srWindow.Bottom;

    TestSetConsoleWindowInfoHelper(bAbsolute, &srViewport, &sbiex.srWindow, false, L"Ensure Left > Right, Top > Bottom is marked invalid.");

    // Window greater than, equal to and less than the max client window
    // Window > Max ( INVALID )
    srViewport.Left = 0;
    srViewport.Top = 0;
    srViewport.Right = sbiex.dwMaximumWindowSize.X; // this is 1 larger than the valid right bound since it's 0-based array indexes
    srViewport.Bottom = sbiex.dwMaximumWindowSize.Y;

    TestSetConsoleWindowInfoHelper(bAbsolute, &srViewport, &sbiex.srWindow, false, L"Ensure window larger than max is marked invalid.");

    // Set to same position we were just at (full screen or not)
    // VALID, SUCCESS

    srViewport = sbiex.srWindow;

    TestSetConsoleWindowInfoHelper(bAbsolute, &srViewport, &sbiex.srWindow, true, L"Set to the original window size");
    TestSetConsoleWindowInfoHelper(bAbsolute, &srViewport, &sbiex.srWindow, true, L"Confirm that setting it again to the same position works.");

    // Will fail while in full screen, but no current way to set that mode externally. :(

    // Finally, check roundtrip by changing window.
    srViewport = sbiex.srWindow;
    srViewport.Left += 1;
    srViewport.Right -= 1;
    srViewport.Top += 1;
    srViewport.Bottom -= 1;

    // Verify the assumption that the viewport was sufficiently large to shrink it in the above manner.
    if (srViewport.Left > srViewport.Right ||
        srViewport.Top > srViewport.Bottom ||
        (srViewport.Right - srViewport.Left) < 1 ||
        (srViewport.Bottom - srViewport.Top) < 1)
    {
        VERIFY_FAIL(NoThrowString().Format(L"Adjusted viewport is invalid. %s", VerifyOutputTraits<SMALL_RECT>::ToString(srViewport).GetBuffer()));
    }

    // Store a copy of the original (for comparison in case the relative translation is applied).
    SMALL_RECT const srViewportBefore = srViewport;

    TestSetConsoleWindowInfoHelper(bAbsolute, &srViewport, &sbiex.srWindow, true, L"Attempt shrinking the window in a valid manner.");

    // Get it back and ensure it's the same dimensions
    CONSOLE_SCREEN_BUFFER_INFO sbi = { 0 };
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfo(Common::_hConsole, &sbi), L"Confirm the size we specified round-trips through to the Get API.");

    VERIFY_ARE_EQUAL(srViewportBefore, sbi.srWindow, L"Match before and after viewport sizes.");
}

void RestrictDimensionsHelper(COORD* const coordTest, SHORT const x, SHORT const y, bool const fUseX, bool const fUseY)
{
    if (fUseX)
    {
        coordTest->X = x;
    }

    if (fUseY)
    {
        coordTest->Y = y;
    }
}

void DimensionsTests::TestSetConsoleScreenBufferSize()
{
    DWORD dwMode = { 0 };
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"scaleChoices", dwMode), L"Get active mode");

    bool fAdjustX = false;
    bool fAdjustY = false;

    if ((dwMode & 0x1) != 0)
    {
        fAdjustX = true;
        Log::Comment(L"Adjusting X dimension");
    }
    if ((dwMode & 0x2) != 0)
    {
        fAdjustY = true;
        Log::Comment(L"Adjusting Y dimension");
    }

    CONSOLE_SCREEN_BUFFER_INFO sbi = { 0 };
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfo(Common::_hConsole, &sbi), L"Get initial buffer/window information.");

    COORD coordSize = { 0 };

    // Ensure buffer size cannot be smaller than minimum
    RestrictDimensionsHelper(&coordSize, 0, 0, fAdjustX, fAdjustY);
    VERIFY_WIN32_BOOL_FAILED(SetConsoleScreenBufferSize(Common::_hConsole, coordSize), L"Set buffer size to smaller than minimum possible.");

    // Ensure buffer size cannot be excessively large.
    RestrictDimensionsHelper(&coordSize, SHRT_MAX, SHRT_MAX, fAdjustX, fAdjustY);
    VERIFY_WIN32_BOOL_FAILED(SetConsoleScreenBufferSize(Common::_hConsole, coordSize), L"Set buffer size to very, very large.");

    // Ensure buffer size cannot be excessively small (negative).
    RestrictDimensionsHelper(&coordSize, SHRT_MIN, SHRT_MIN, fAdjustX, fAdjustY);
    VERIFY_WIN32_BOOL_FAILED(SetConsoleScreenBufferSize(Common::_hConsole, coordSize), L"Set buffer size to negative values.");

    // Ensure success on giving the same size back that we started with
    coordSize = sbi.dwSize;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(Common::_hConsole, coordSize), L"Set it to the same size as initial.");

    // save the dimensions of the window for use in tests relative to window size
    COORD coordWindowDim;
    coordWindowDim.X = sbi.srWindow.Right - sbi.srWindow.Left;
    coordWindowDim.Y = sbi.srWindow.Bottom - sbi.srWindow.Top;

    // Ensure buffer size cannot be smaller than the window
    coordSize = coordWindowDim;
    RestrictDimensionsHelper(&coordSize, coordSize.X - 1, coordSize.Y - 1, fAdjustX, fAdjustY);
    VERIFY_WIN32_BOOL_FAILED(SetConsoleScreenBufferSize(Common::_hConsole, coordSize), L"Try to make buffer smaller than the window size.");

    // Success on setting a buffer larger than the window
    coordSize = coordWindowDim;
    coordSize.X++;
    coordSize.Y++;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(Common::_hConsole, coordSize), L"Try to make buffer larger than the window size.");
}

void DimensionsTests::TestZeroSizedConsoleScreenBuffers()
{
    // Make sure we never accept zero-sized console buffers through the public API
    const COORD rgTestCoords[] = {
        { 0, 0 },
        { 0, 1 },
        { 1, 0 }
    };

    for (size_t i = 0; i < ARRAYSIZE(rgTestCoords); i++)
    {
        const BOOL fSucceeded = SetConsoleScreenBufferSize(Common::_hConsole, rgTestCoords[i]);
        VERIFY_IS_FALSE(!!fSucceeded,
                        NoThrowString().Format(L"Setting zero console size should always fail (x: %d y:%d)",
                                               rgTestCoords[i].X,
                                               rgTestCoords[i].Y));
        VERIFY_ARE_EQUAL((DWORD)ERROR_INVALID_PARAMETER, GetLastError());
    }
}

template<typename T>
void TestSetConsoleScreenBufferInfoExHelper(bool const fShouldHaveChanged,
                                            T const pOriginal,
                                            T const pTest,
                                            T const pReturned,
                                            PCWSTR pwszDescriptor)
{
    if (fShouldHaveChanged)
    {
        VERIFY_ARE_EQUAL(pTest, pReturned, NoThrowString().Format(L"Verify %s has changed to match the test value.", pwszDescriptor));
        VERIFY_ARE_NOT_EQUAL(pOriginal, pReturned, NoThrowString().Format(L"Verify %s does not match original value.", pwszDescriptor));
    }
    else
    {
        VERIFY_ARE_NOT_EQUAL(pTest, pReturned, NoThrowString().Format(L"Verify %s has NOT changed to match the test value.", pwszDescriptor));
        VERIFY_ARE_EQUAL(pOriginal, pReturned, NoThrowString().Format(L"Verify %s DOES match original value.", pwszDescriptor));
    }
}

void DimensionsTests::TestSetConsoleScreenBufferInfoEx()
{
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);

    // <n/a> = cbSize
    // Attributes = wAttributes
    // ColorTable = ColorTable
    // CursorPosition = dwCursorPosition
    // FullscreenSupported = bFullscreenSupported
    // MaximumWindowSize = dwMaximumWindowSize
    // PopupAttributes = wPopupAttributes
    // Size = dwSize

    // combine to make srWindow. Translated inside the driver \minkernel\console\client\getset.c
    // CurrentWindowSize
    // ScrollPosition

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiex), L"Get original buffer state.");

    // save a copy for the final comparison.
    CONSOLE_SCREEN_BUFFER_INFOEX const sbiexOriginal = sbiex;

    // check invalid values of viewport size
    sbiex = sbiexOriginal;
    sbiex.dwSize.X = 0;
    sbiex.dwSize.Y = 0;
    VERIFY_WIN32_BOOL_FAILED(SetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiex), L"Try 0x0 viewport size.");

    sbiex = sbiexOriginal;
    sbiex.dwSize.X = MAXSHORT;
    sbiex.dwSize.Y = MAXSHORT;
    VERIFY_WIN32_BOOL_FAILED(SetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiex), L"Try MAX by MAX viewport size.");

    // Fill the entire structure with new data and set
    sbiex.dwSize.X = 200;
    sbiex.dwSize.Y = 5555;

    sbiex.srWindow.Left = 0;
    sbiex.srWindow.Right = 79;
    sbiex.srWindow.Top = 0;
    sbiex.srWindow.Bottom = 49;

    sbiex.wAttributes = BACKGROUND_BLUE | BACKGROUND_INTENSITY | FOREGROUND_RED;
    sbiex.wPopupAttributes = BACKGROUND_GREEN | FOREGROUND_RED;

    sbiex.ColorTable[0] = 0x0000000F;
    sbiex.ColorTable[1] = 0x000000F0;
    sbiex.ColorTable[2] = 0x00000F00;
    sbiex.ColorTable[3] = 0x0000F000;
    sbiex.ColorTable[4] = 0x000F0000;
    sbiex.ColorTable[5] = 0x00F00000;
    sbiex.ColorTable[6] = 0x000000FF;
    sbiex.ColorTable[7] = 0x00000FF0;
    sbiex.ColorTable[8] = 0x0000FF00;
    sbiex.ColorTable[9] = 0x000FF000;
    sbiex.ColorTable[10] = 0x00FF0000;
    sbiex.ColorTable[11] = 0x00000FFF;
    sbiex.ColorTable[12] = 0x0000FFF0;
    sbiex.ColorTable[13] = 0x000FFF00;
    sbiex.ColorTable[14] = 0x00FFF000;
    sbiex.ColorTable[15] = 0x0000FFFF;

    sbiex.dwMaximumWindowSize.X = 100;
    sbiex.dwMaximumWindowSize.Y = 80;

    sbiex.bFullscreenSupported = !sbiex.bFullscreenSupported; // set to opposite

    // DO NOT TRY TO SET THE CURSOR. It may or may not be in the same place. The Set API actually never obeyed the request
    // to set the position and we can't fix it now.

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiex), L"Attempt to set structure with all new data.");

    // Confirm that the prompt stored settings as appropriate
    CONSOLE_SCREEN_BUFFER_INFOEX sbiexAfter = { 0 };
    sbiexAfter.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiexAfter), L"Retrieve set data with get.");

    // Verify that relevant properties were stored into the console.

    // The buffer size is weird because there are currently two valid answers.
    // This is due to the word wrap status of the console which is currently not visible through the API.
    // We must accept either answer as valid.
    bool fBufferSizePassed = false;

    // 1. The buffer size we set matches exactly with what we retrieved after it was done. (classic behavior, no word wrap)
    if (VerifyCompareTraits<COORD, COORD>().AreEqual(sbiex.dwSize, sbiexAfter.dwSize))
    {
        fBufferSizePassed = true;
    }

    // 2. The buffer size is restricted/pegged to the width (X dimension) of the window. (new behavior, word wrap)
    short sWidthLimit = (sbiex.srWindow.Right - sbiex.srWindow.Left) + 1; // the right index counts as valid, so right - left + 1 for total width.

    // 2a. Width expected might be reduced if the buffer is taller than the window. If so, reduce by a scroll bar in width.
    if (sbiex.dwSize.Y > ((sbiex.srWindow.Bottom - sbiex.srWindow.Top) + 1)) // the bottom index counts as valid, so bottom - top + 1 for total height.
    {
        // Get pixel size of a vertical scroll bar.
        short const sVerticalScrollWidthPx = (SHORT)GetSystemMetrics(SM_CXVSCROLL);

        // Get the current font size
        CONSOLE_FONT_INFO cfi;
        VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetCurrentConsoleFont(Common::_hConsole, FALSE, &cfi), L"Get the current console font structure.");

        if (VERIFY_ARE_NOT_EQUAL(0, cfi.dwFontSize.X, L"Verify that the font width is not zero or we'll have a division error."))
        {
            // Figure out how many character widths to reduce by.
            short sReduceBy = 0;

            // Divide the size of a scroll bar by the font widths.
            sReduceBy = sVerticalScrollWidthPx / cfi.dwFontSize.X;

            // If there is a remainder, add one more. We can't render partial characters.
            sReduceBy += sVerticalScrollWidthPx % cfi.dwFontSize.X ? 1 : 0;

            // Subtract the number of characters being reserved for the scroll bar.
            sWidthLimit -= sReduceBy;
        }
    }

    // 2b. Do the comparison. Y should be correct, but X will be the lesser of the size we asked for or the window limit for word wrap.
    if (sbiex.dwSize.Y == sbiexAfter.dwSize.Y && min(sbiex.dwSize.X, sWidthLimit) == sbiexAfter.dwSize.X)
    {
        fBufferSizePassed = true;
    }
    VERIFY_IS_TRUE(fBufferSizePassed, L"Verify Buffer Size has changed as expected.");

    // Test remaining parameters are the same
    TestSetConsoleScreenBufferInfoExHelper(true, sbiexOriginal.wAttributes, sbiex.wAttributes, sbiexAfter.wAttributes, L"Attributes (Fg/Bg Colors)");
    TestSetConsoleScreenBufferInfoExHelper(true, sbiexOriginal.wPopupAttributes, sbiex.wPopupAttributes, sbiexAfter.wPopupAttributes, L"Popup Attributes (Fg/Bg Colors)");

    // verify colors match
    for (UINT i = 0; i < 16; i++)
    {
        TestSetConsoleScreenBufferInfoExHelper(true, sbiexOriginal.ColorTable[i], sbiex.ColorTable[i], sbiexAfter.ColorTable[i], NoThrowString().Format(L"Color %x", i));
    }

    // NOTE: Max window size and the positioning of the window are adjusted at the discretion of the console.
    // They will not necessarily match, so we're not testing them.

    // NOTE: Full screen will NOT be changed by this API and should match the originals.
    TestSetConsoleScreenBufferInfoExHelper(false, sbiexOriginal.bFullscreenSupported, sbiex.bFullscreenSupported, sbiexAfter.bFullscreenSupported, L"Fullscreen");

    // NOTE: Ignore cursor position. It can change or not depending on the word wrap mode and the API set doesn't do anything.

    // BUG: This is a long standing bug in the console which some of our customers have documented on the MSDN page.
    //      The console driver (\minkernel\console\client\getset.c) is treating the viewport as an "exclusive" rectangle where it is actually "inclusive"
    //      of its edges. This means when it does a width calculation, it has an off-by-one error and will shrink the window in height and width by 1 each
    //      trip around. For example, normally we do viewport width as Right-Left+1, and the driver does it as Right-Left.
    //      As this has lasted so long, it's likely a compat issue to fix now. So we'll leave it in and compensate for it in the test here.
    // See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms686039(v=vs.85).aspx
    CONSOLE_SCREEN_BUFFER_INFOEX sbiexBug = sbiexOriginal;
    sbiexBug.srWindow.Bottom++;
    sbiexBug.srWindow.Right++;

    // Restore original settings
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiexBug), L"Restore original settings.");

    // Ensure originals are restored.
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiexAfter), L"Retrieve what we just set.");

    // NOTE: Set the two cursor positions to the same thing because we don't care to compare them. They can
    // be different or they may not be different. The SET API doesn't actually work so it depends on the other state,
    // which we're not measuring now.
    sbiexAfter.dwCursorPosition = sbiexOriginal.dwCursorPosition;

    VERIFY_ARE_EQUAL(sbiexAfter, sbiexOriginal, L"Ensure settings are back to original values.");
}
