// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#define CP_USA 437

class FillOutputTests
{
    BEGIN_TEST_CLASS(FillOutputTests)
    END_TEST_CLASS()

    TEST_METHOD(WriteNarrowGlyphAscii)
    {
        HANDLE hConsole = GetStdOutputHandle();
        DWORD charsWritten = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterA(hConsole,
                                                                'a',
                                                                1,
                                                                { 0, 0 },
                                                                &charsWritten));
        VERIFY_ARE_EQUAL(1u, charsWritten);

        // test a box drawing character
        const UINT previousCodepage = GetConsoleOutputCP();
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(CP_USA));

        charsWritten = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterA(hConsole,
                                                                '\xCE', // U+256C box drawing double vertical and horizontal
                                                                1,
                                                                { 0, 0 },
                                                                &charsWritten));
        VERIFY_ARE_EQUAL(1u, charsWritten);
        VERIFY_SUCCEEDED(SetConsoleOutputCP(previousCodepage));
    }

    TEST_METHOD(WriteNarrowGlyphUnicode)
    {
        HANDLE hConsole = GetStdOutputHandle();
        DWORD charsWritten = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(hConsole,
                                                                L'a',
                                                                1,
                                                                { 0, 0 },
                                                                &charsWritten));
        VERIFY_ARE_EQUAL(1u, charsWritten);
    }

    TEST_METHOD(WriteWideGlyphUnicode)
    {
        HANDLE hConsole = GetStdOutputHandle();
        DWORD charsWritten = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(hConsole,
                                                                L'\x304F',
                                                                1,
                                                                { 0, 0 },
                                                                &charsWritten));
        VERIFY_ARE_EQUAL(1u, charsWritten);
    }

    TEST_METHOD(UnsetWrap)
    {
        // WARNING: If this test suddenly decides to start failing,
        // this is because the wrap registry key is not set.
        // TODO GH #2859: Get/Set Registry Key for Wrap

        HANDLE hConsole = GetStdOutputHandle();
        DWORD charsWritten = 0;

        CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
        sbiex.cbSize = sizeof(sbiex);
        VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hConsole, &sbiex));

        const auto consoleWidth = sbiex.dwSize.X;

        std::wstring input(consoleWidth + 2, L'a');
        std::wstring filled(consoleWidth, L'b');

        // Write until a wrap occurs
        VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleW(hConsole,
                                                  input.data(),
                                                  gsl::narrow_cast<DWORD>(input.size()),
                                                  &charsWritten,
                                                  nullptr));

        // Verify wrap occurred
        std::unique_ptr<wchar_t[]> bufferText = std::make_unique<wchar_t[]>(consoleWidth);
        DWORD readSize = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(hConsole,
                                                                bufferText.get(),
                                                                consoleWidth,
                                                                { 0, 0 },
                                                                &readSize));

        WEX::Common::String expected(input.c_str(), readSize);
        WEX::Common::String actual(bufferText.get(), readSize);
        VERIFY_ARE_EQUAL(expected, actual);

        bufferText = std::make_unique<wchar_t[]>(2);
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(hConsole,
                                                                bufferText.get(),
                                                                2,
                                                                { 0, 1 },
                                                                &readSize));

        VERIFY_ARE_EQUAL(2u, readSize);
        expected = WEX::Common::String(input.c_str(), readSize);
        actual = WEX::Common::String(bufferText.get(), readSize);
        VERIFY_ARE_EQUAL(expected, actual);

        // Fill Console Line with 'b's
        VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(hConsole,
                                                                L'b',
                                                                consoleWidth,
                                                                { 2, 0 },
                                                                &charsWritten));

        // Verify first line is full of 'a's then 'b's
        bufferText = std::make_unique<wchar_t[]>(consoleWidth);
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(hConsole,
                                                                bufferText.get(),
                                                                consoleWidth,
                                                                { 0, 0 },
                                                                &readSize));

        expected = WEX::Common::String(input.c_str(), 2);
        actual = WEX::Common::String(bufferText.get(), 2);
        VERIFY_ARE_EQUAL(expected, actual);

        expected = WEX::Common::String(filled.c_str(), consoleWidth - 2);
        actual = WEX::Common::String(&bufferText[2], readSize - 2);
        VERIFY_ARE_EQUAL(expected, actual);

        // Verify second line is still has 'a's that wrapped over
        bufferText = std::make_unique<wchar_t[]>(2);
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(hConsole,
                                                                bufferText.get(),
                                                                static_cast<SHORT>(2),
                                                                { 0, 0 },
                                                                &readSize));

        VERIFY_ARE_EQUAL(2u, readSize);
        expected = WEX::Common::String(input.c_str(), 2);
        actual = WEX::Common::String(bufferText.get(), readSize);
        VERIFY_ARE_EQUAL(expected, actual);

        // Resize to be smaller by 2
        sbiex.srWindow.Right -= 2;
        sbiex.dwSize.X -= 2;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferInfoEx(hConsole, &sbiex));

        // Verify first line is full of 'a's then 'b's
        bufferText = std::make_unique<wchar_t[]>(consoleWidth - 2);
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(hConsole,
                                                                bufferText.get(),
                                                                consoleWidth - static_cast<SHORT>(2),
                                                                { 0, 0 },
                                                                &readSize));

        expected = WEX::Common::String(input.c_str(), 2);
        actual = WEX::Common::String(bufferText.get(), 2);
        VERIFY_ARE_EQUAL(expected, actual);

        expected = WEX::Common::String(filled.c_str(), consoleWidth - 4);
        actual = WEX::Common::String(&bufferText[2], readSize - 2);
        VERIFY_ARE_EQUAL(expected, actual);

        // Verify second line is still has 'a's ('b's didn't wrap over)
        bufferText = std::make_unique<wchar_t[]>(static_cast<SHORT>(2));
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(hConsole,
                                                                bufferText.get(),
                                                                static_cast<SHORT>(2),
                                                                { 0, 0 },
                                                                &readSize));

        VERIFY_ARE_EQUAL(2u, readSize);
        expected = WEX::Common::String(input.c_str(), 2);
        actual = WEX::Common::String(bufferText.get(), readSize);
        VERIFY_ARE_EQUAL(expected, actual);
    }
};
