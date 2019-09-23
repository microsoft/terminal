// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::Common;

HANDLE g_hOut = INVALID_HANDLE_VALUE;
CONSOLE_SCREEN_BUFFER_INFOEX g_sbiex_backup = { 0 };
COORD g_cWriteSize = { 16, 16 };

const int LEGACY_MODE = 0;
const int VT_SIMPLE_MODE = 1;
const int VT_256_MODE = 2;
const int VT_RGB_MODE = 3;
const int VT_256_GRID_MODE = 4;

// This class is intended to test boundary conditions for:
// SetConsoleActiveScreenBuffer
class RgbColorTests
{
    BEGIN_TEST_CLASS(RgbColorTests)
    END_TEST_CLASS()

    TEST_METHOD_SETUP(MethodSetup)
    {
        g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD outMode;
        VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(g_hOut, &outMode));
        outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(g_hOut, outMode));
        CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
        sbiex.cbSize = sizeof(sbiex);

        BOOL fSuccess = GetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
        if (fSuccess)
        {
            sbiex.srWindow.Bottom++; // hack because the API is not good at roundtrip

            g_sbiex_backup = sbiex;

            // Set the Color table to a known color table
            sbiex.ColorTable[0] = RGB(0x0000, 0x0000, 0x0000);
            sbiex.ColorTable[1] = RGB(0x0000, 0x0000, 0x0080);
            sbiex.ColorTable[2] = RGB(0x0000, 0x0080, 0x0000);
            sbiex.ColorTable[3] = RGB(0x0000, 0x0080, 0x0080);
            sbiex.ColorTable[4] = RGB(0x0080, 0x0000, 0x0000);
            sbiex.ColorTable[5] = RGB(0x0080, 0x0000, 0x0080);
            sbiex.ColorTable[6] = RGB(0x0080, 0x0080, 0x0000);
            sbiex.ColorTable[7] = RGB(0x00C0, 0x00C0, 0x00C0);
            sbiex.ColorTable[8] = RGB(0x0080, 0x0080, 0x0080);
            sbiex.ColorTable[9] = RGB(0x0000, 0x0000, 0x00FF);
            sbiex.ColorTable[10] = RGB(0x0000, 0x00FF, 0x0000);
            sbiex.ColorTable[11] = RGB(0x0000, 0x00FF, 0x00FF);
            sbiex.ColorTable[12] = RGB(0x00FF, 0x0000, 0x0000);
            sbiex.ColorTable[13] = RGB(0x00FF, 0x0000, 0x00FF);
            sbiex.ColorTable[14] = RGB(0x00FF, 0x00FF, 0x0000);
            sbiex.ColorTable[15] = RGB(0x00FF, 0x00FF, 0x00FF);

            sbiex.dwCursorPosition.X = 0;
            sbiex.dwCursorPosition.Y = 0;

            SetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
        }
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        SetConsoleScreenBufferInfoEx(g_hOut, &g_sbiex_backup);

        return true;
    }
    TEST_METHOD(TestPureLegacy);
    TEST_METHOD(TestVTSimpleToLegacy);
    TEST_METHOD(TestVT256ToLegacy);
    TEST_METHOD(TestVTRGBToLegacy);
    TEST_METHOD(TestVT256Grid);
};

// Takes windows 16 color table index, and returns a int for the equivalent SGR sequence
WORD WinToVTColor(int winColor, bool isForeground)
{
    bool red = (winColor & FOREGROUND_RED) > 0;
    bool green = (winColor & FOREGROUND_GREEN) > 0;
    bool blue = (winColor & FOREGROUND_BLUE) > 0;
    bool bright = (winColor & FOREGROUND_INTENSITY) > 0;

    WORD result = isForeground ? 30 : 40;
    result += bright ? 60 : 0;
    result += red ? 1 : 0;
    result += green ? 2 : 0;
    result += blue ? 4 : 0;

    return result;
}

WORD MakeAttribute(int fg, int bg)
{
    return (WORD)((bg << 4) | (fg));
}

// Takes a windows 16 color table index, and returns the equivalent xterm table index
//   (also in [0,15])
int WinToXtermIndex(int iWinColor)
{
    bool fRed = (iWinColor & 0x04) > 0;
    bool fGreen = (iWinColor & 0x02) > 0;
    bool fBlue = (iWinColor & 0x01) > 0;
    bool fBright = (iWinColor & 0x08) > 0;
    int iXtermTableEntry = (fRed ? 0x1 : 0x0) | (fGreen ? 0x2 : 0x0) | (fBlue ? 0x4 : 0x0) | (fBright ? 0x8 : 0x0);
    return iXtermTableEntry;
}

int WriteLegacyColorTestChars(int fg, int bg)
{
    DWORD numWritten = 0;
    SetConsoleTextAttribute(g_hOut, MakeAttribute(fg, bg));
    WriteConsole(g_hOut, L"*", 1, &numWritten, nullptr);
    return numWritten;
}

int WriteVTSimpleTestChars(int fg, int bg)
{
    DWORD numWritten = 0;
    wprintf(L"\x1b[%d;%dm", WinToVTColor(fg, true), WinToVTColor(bg, false));
    WriteConsole(g_hOut, L"*", 1, &numWritten, nullptr);
    return numWritten;
}

int WriteVT256TestChars(int fg, int bg)
{
    DWORD numWritten = 0;
    wprintf(L"\x1b[38;5;%d;48;5;%dm", WinToXtermIndex(fg), WinToXtermIndex(bg));
    WriteConsole(g_hOut, L"*", 1, &numWritten, nullptr);
    return numWritten;
}

int WriteVT256GridTestChars(int fg, int bg)
{
    DWORD numWritten = 0;
    WORD index = MakeAttribute(fg, bg);
    wprintf(L"\x1b[38;5;%d;48;5;%dm", index, index);
    WriteConsole(g_hOut, L"*", 1, &numWritten, nullptr);
    return numWritten;
}

int WriteVTRGBTestChars(int fg, int bg)
{
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(sbiex);
    GetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
    COLORREF fgColor = sbiex.ColorTable[fg];
    COLORREF bgColor = sbiex.ColorTable[bg];

    int fgRed = GetRValue(fgColor);
    int fgBlue = GetBValue(fgColor);
    int fgGreen = GetGValue(fgColor);

    int bgRed = GetRValue(bgColor);
    int bgBlue = GetBValue(bgColor);
    int bgGreen = GetGValue(bgColor);

    DWORD numWritten = 0;
    wprintf(L"\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm", fgRed, fgGreen, fgBlue, bgRed, bgGreen, bgBlue);
    WriteConsole(g_hOut, L"*", 1, &numWritten, nullptr);
    return numWritten;
}

BOOL CreateColorGrid(int iColorMode)
{
    COORD coordCursor = { 0 };
    BOOL fSuccess = SetConsoleCursorPosition(g_hOut, coordCursor);

    if (fSuccess)
    {
        CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
        sbiex.cbSize = sizeof(sbiex);
        fSuccess = GetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
        if (fSuccess)
        {
            DWORD totalWritten = 0;
            COORD writeSize = g_cWriteSize;
            COORD cursorPosInitial = sbiex.dwCursorPosition;
            for (int fg = 0; fg < writeSize.Y; fg++)
            {
                DWORD numWritten = 0;
                for (int bg = 0; bg < writeSize.X; bg++)
                {
                    switch (iColorMode)
                    {
                    case LEGACY_MODE:
                        numWritten = WriteLegacyColorTestChars(fg, bg);
                        totalWritten += numWritten;
                        break;
                    case VT_SIMPLE_MODE:
                        numWritten = WriteVTSimpleTestChars(fg, bg);
                        totalWritten += numWritten;
                        break;
                    case VT_256_MODE:
                        numWritten = WriteVT256TestChars(fg, bg);
                        totalWritten += numWritten;
                        break;
                    case VT_RGB_MODE:
                        numWritten = WriteVTRGBTestChars(fg, bg);
                        totalWritten += numWritten;
                        break;
                    case VT_256_GRID_MODE:
                        numWritten = WriteVT256GridTestChars(fg, bg);
                        totalWritten += numWritten;
                        break;
                    default:
                        VERIFY_ARE_EQUAL(true, false, L"Did not provide a valid color mode");
                    }
                }
                switch (iColorMode)
                {
                case LEGACY_MODE:
                    SetConsoleTextAttribute(g_hOut, sbiex.wAttributes);
                    WriteConsole(g_hOut, L"\n", 1, &numWritten, nullptr);
                    break;
                case VT_SIMPLE_MODE:
                case VT_256_MODE:
                case VT_RGB_MODE:
                case VT_256_GRID_MODE:
                    wprintf(L"\x1b[0m\n");
                    break;
                default:
                    VERIFY_ARE_EQUAL(true, false, L"Did not provide a valid color mode");
                }
            }
            fSuccess = totalWritten == (1 * 16 * 16);
        }
    }

    return fSuccess;
}

BOOL CreateLegacyColorGrid()
{
    return CreateColorGrid(LEGACY_MODE);
}

WORD GetGridAttrs(int x, int y, CHAR_INFO* pBuffer, COORD cGridSize)
{
    return (pBuffer[(cGridSize.X * y) + x]).Attributes;
}

BOOL ValidateLegacyColorGrid(COORD cursorPosInitial)
{
    COORD actualWriteSize;
    actualWriteSize.X = 16;
    actualWriteSize.Y = 16;

    CHAR_INFO* rOutputBuffer = new CHAR_INFO[actualWriteSize.X * actualWriteSize.Y];

    SMALL_RECT srReadRegion = { 0 };
    srReadRegion.Top = cursorPosInitial.Y;
    srReadRegion.Left = cursorPosInitial.X;
    srReadRegion.Right = srReadRegion.Left + actualWriteSize.X;
    srReadRegion.Bottom = srReadRegion.Top + actualWriteSize.Y;

    BOOL fSuccess = ReadConsoleOutput(g_hOut, rOutputBuffer, actualWriteSize, { 0 }, &srReadRegion);
    VERIFY_WIN32_BOOL_SUCCEEDED(fSuccess, L"Read the output back");
    if (fSuccess)
    {
        CHAR_INFO* pInfo = rOutputBuffer;
        for (int fg = 0; fg < g_cWriteSize.Y; fg++)
        {
            for (int bg = 0; bg < g_cWriteSize.X; bg++)
            {
                WORD wExpected = MakeAttribute(fg, bg);
                VERIFY_ARE_EQUAL(pInfo->Attributes, wExpected, NoThrowString().Format(L"fg, bg = (%d,%d)", fg, bg));
                fSuccess &= pInfo->Attributes == wExpected;
                pInfo += 1; // We wrote one character each time
            }
        }
    }
    delete[] rOutputBuffer;

    return fSuccess;
}

BOOL Validate256GridToLegacy(COORD cursorPosInitial)
{
    COORD actualWriteSize;
    actualWriteSize.X = 16;
    actualWriteSize.Y = 16;

    CHAR_INFO* rOutputBuffer = new CHAR_INFO[actualWriteSize.X * actualWriteSize.Y];

    SMALL_RECT srReadRegion = { 0 };
    srReadRegion.Top = cursorPosInitial.Y;
    srReadRegion.Left = cursorPosInitial.X;
    srReadRegion.Right = srReadRegion.Left + actualWriteSize.X;
    srReadRegion.Bottom = srReadRegion.Top + actualWriteSize.Y;

    BOOL fSuccess = ReadConsoleOutput(g_hOut, rOutputBuffer, actualWriteSize, { 0 }, &srReadRegion);
    VERIFY_WIN32_BOOL_SUCCEEDED(fSuccess, L"Read the output back");
    if (fSuccess)
    {
        // Validate some locations on the grid with what we know they'll be translated to.
        // First column has the 16 colors from the table, in VT order
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 0, rOutputBuffer, actualWriteSize), MakeAttribute(0x0, 0x0));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 1, rOutputBuffer, actualWriteSize), MakeAttribute(0x4, 0x4));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 2, rOutputBuffer, actualWriteSize), MakeAttribute(0x2, 0x2));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 3, rOutputBuffer, actualWriteSize), MakeAttribute(0x6, 0x6));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 4, rOutputBuffer, actualWriteSize), MakeAttribute(0x1, 0x1));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 5, rOutputBuffer, actualWriteSize), MakeAttribute(0x5, 0x5));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 6, rOutputBuffer, actualWriteSize), MakeAttribute(0x3, 0x3));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 7, rOutputBuffer, actualWriteSize), MakeAttribute(0x7, 0x7));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 8, rOutputBuffer, actualWriteSize), MakeAttribute(0x8, 0x8));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 9, rOutputBuffer, actualWriteSize), MakeAttribute(0xC, 0xC));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 10, rOutputBuffer, actualWriteSize), MakeAttribute(0xA, 0xA));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 11, rOutputBuffer, actualWriteSize), MakeAttribute(0xE, 0xE));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 12, rOutputBuffer, actualWriteSize), MakeAttribute(0x9, 0x9));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 13, rOutputBuffer, actualWriteSize), MakeAttribute(0xD, 0xD));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 14, rOutputBuffer, actualWriteSize), MakeAttribute(0xB, 0xB));
        VERIFY_ARE_EQUAL(GetGridAttrs(0, 15, rOutputBuffer, actualWriteSize), MakeAttribute(0xF, 0xF));

        // Verify some other locations in the table, that will be RGB->Legacy conversions.
        VERIFY_ARE_EQUAL(GetGridAttrs(1, 1, rOutputBuffer, actualWriteSize), MakeAttribute(0x1, 0x1));
        VERIFY_ARE_EQUAL(GetGridAttrs(2, 1, rOutputBuffer, actualWriteSize), MakeAttribute(0xB, 0xB));
        VERIFY_ARE_EQUAL(GetGridAttrs(2, 2, rOutputBuffer, actualWriteSize), MakeAttribute(0x2, 0x2));
        VERIFY_ARE_EQUAL(GetGridAttrs(2, 3, rOutputBuffer, actualWriteSize), MakeAttribute(0x3, 0x3));
        VERIFY_ARE_EQUAL(GetGridAttrs(3, 4, rOutputBuffer, actualWriteSize), MakeAttribute(0x4, 0x4));
        VERIFY_ARE_EQUAL(GetGridAttrs(3, 5, rOutputBuffer, actualWriteSize), MakeAttribute(0x5, 0x5));
        VERIFY_ARE_EQUAL(GetGridAttrs(4, 5, rOutputBuffer, actualWriteSize), MakeAttribute(0x9, 0x9));
        VERIFY_ARE_EQUAL(GetGridAttrs(4, 6, rOutputBuffer, actualWriteSize), MakeAttribute(0x6, 0x6));
        VERIFY_ARE_EQUAL(GetGridAttrs(4, 7, rOutputBuffer, actualWriteSize), MakeAttribute(0x7, 0x7));
        VERIFY_ARE_EQUAL(GetGridAttrs(3, 11, rOutputBuffer, actualWriteSize), MakeAttribute(0x8, 0x8));
        VERIFY_ARE_EQUAL(GetGridAttrs(3, 12, rOutputBuffer, actualWriteSize), MakeAttribute(0x1, 0x1));
        VERIFY_ARE_EQUAL(GetGridAttrs(4, 12, rOutputBuffer, actualWriteSize), MakeAttribute(0xA, 0xA));
        VERIFY_ARE_EQUAL(GetGridAttrs(5, 12, rOutputBuffer, actualWriteSize), MakeAttribute(0xD, 0xD));
        VERIFY_ARE_EQUAL(GetGridAttrs(10, 12, rOutputBuffer, actualWriteSize), MakeAttribute(0xE, 0xE));
        VERIFY_ARE_EQUAL(GetGridAttrs(10, 13, rOutputBuffer, actualWriteSize), MakeAttribute(0xC, 0xC));
        VERIFY_ARE_EQUAL(GetGridAttrs(11, 13, rOutputBuffer, actualWriteSize), MakeAttribute(0xF, 0xF));

        // Greyscale ramp
        VERIFY_ARE_EQUAL(GetGridAttrs(14, 8, rOutputBuffer, actualWriteSize), MakeAttribute(0x0, 0x0));
        VERIFY_ARE_EQUAL(GetGridAttrs(14, 9, rOutputBuffer, actualWriteSize), MakeAttribute(0x0, 0x0));

        VERIFY_ARE_EQUAL(GetGridAttrs(14, 14, rOutputBuffer, actualWriteSize), MakeAttribute(0x8, 0x8));
        VERIFY_ARE_EQUAL(GetGridAttrs(14, 15, rOutputBuffer, actualWriteSize), MakeAttribute(0x8, 0x8));
        VERIFY_ARE_EQUAL(GetGridAttrs(15, 0, rOutputBuffer, actualWriteSize), MakeAttribute(0x8, 0x8));

        VERIFY_ARE_EQUAL(GetGridAttrs(15, 8, rOutputBuffer, actualWriteSize), MakeAttribute(0x7, 0x7));
        VERIFY_ARE_EQUAL(GetGridAttrs(15, 9, rOutputBuffer, actualWriteSize), MakeAttribute(0x7, 0x7));

        VERIFY_ARE_EQUAL(GetGridAttrs(15, 14, rOutputBuffer, actualWriteSize), MakeAttribute(0xF, 0xF));
        VERIFY_ARE_EQUAL(GetGridAttrs(15, 15, rOutputBuffer, actualWriteSize), MakeAttribute(0xF, 0xF));
    }
    delete[] rOutputBuffer;
    return fSuccess;
}

void RgbColorTests::TestPureLegacy()
{
    BOOL fSuccess;
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(sbiex);
    fSuccess = CreateLegacyColorGrid();
    if (fSuccess)
    {
        GetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
        COORD actualPos = sbiex.dwCursorPosition;
        // Subtract the size of the grid to get back to the top of it.
        actualPos.Y -= g_cWriteSize.Y;
        fSuccess = ValidateLegacyColorGrid(actualPos);
        VERIFY_WIN32_BOOL_SUCCEEDED(fSuccess, L"Validated Legacy Color Grid");
    }
}

void RgbColorTests::TestVTSimpleToLegacy()
{
    BOOL fSuccess;
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(sbiex);
    fSuccess = CreateColorGrid(VT_SIMPLE_MODE);
    if (fSuccess)
    {
        GetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
        COORD actualPos = sbiex.dwCursorPosition;
        // Subtract the size of the grid to get back to the top of it.
        actualPos.Y -= g_cWriteSize.Y;
        fSuccess = ValidateLegacyColorGrid(actualPos);
        VERIFY_WIN32_BOOL_SUCCEEDED(fSuccess, L"Validated Simple VT Color Grid");
    }
}

void RgbColorTests::TestVT256ToLegacy()
{
    BOOL fSuccess;
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(sbiex);
    fSuccess = CreateColorGrid(VT_256_MODE);
    if (fSuccess)
    {
        GetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
        COORD actualPos = sbiex.dwCursorPosition;
        // Subtract the size of the grid to get back to the top of it.
        actualPos.Y -= g_cWriteSize.Y;
        fSuccess = ValidateLegacyColorGrid(actualPos);
        VERIFY_WIN32_BOOL_SUCCEEDED(fSuccess, L"Validated 256 Table VT Color Grid");
    }
}

void RgbColorTests::TestVTRGBToLegacy()
{
    BOOL fSuccess;
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(sbiex);
    fSuccess = CreateColorGrid(VT_RGB_MODE);
    if (fSuccess)
    {
        GetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
        COORD actualPos = sbiex.dwCursorPosition;
        // Subtract the size of the grid to get back to the top of it.
        actualPos.Y -= g_cWriteSize.Y;
        fSuccess = ValidateLegacyColorGrid(actualPos);
        VERIFY_WIN32_BOOL_SUCCEEDED(fSuccess, L"Validated RGB VT Color Grid");
    }
}

void RgbColorTests::TestVT256Grid()
{
    BOOL fSuccess;
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(sbiex);
    fSuccess = CreateColorGrid(VT_256_GRID_MODE);
    if (fSuccess)
    {
        GetConsoleScreenBufferInfoEx(g_hOut, &sbiex);
        COORD actualPos = sbiex.dwCursorPosition;
        // Subtract the size of the grid to get back to the top of it.
        actualPos.Y -= g_cWriteSize.Y;
        fSuccess = Validate256GridToLegacy(actualPos);
        VERIFY_WIN32_BOOL_SUCCEEDED(fSuccess, L"Validated VT 256 Color Grid to Legacy Attributes");
    }
}
