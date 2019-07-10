// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\types\inc\viewport.hpp"

#include <thread>
#include <vector>
#include <algorithm>

using namespace Microsoft::Console::Types;
using namespace WEX::TestExecution;
using WEX::Logging::Log;
using namespace WEX::Common;

class OutputTests
{
    BEGIN_TEST_CLASS(OutputTests)
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(TestSetup);
    TEST_CLASS_CLEANUP(TestCleanup);

    TEST_METHOD(BasicReadConsoleOutputATest);
    TEST_METHOD(BasicReadConsoleOutputWTest);

    TEST_METHOD(BasicWriteConsoleOutputWTest);
    TEST_METHOD(BasicWriteConsoleOutputATest);

    TEST_METHOD(WriteConsoleOutputWOutsideBuffer);
    TEST_METHOD(WriteConsoleOutputWWithClipping);
    TEST_METHOD(WriteConsoleOutputWNegativePositions);

    TEST_METHOD(ReadConsoleOutputWOutsideBuffer);
    TEST_METHOD(ReadConsoleOutputWWithClipping);
    TEST_METHOD(ReadConsoleOutputWNegativePositions);
    TEST_METHOD(ReadConsoleOutputWPartialUserBuffer);

    TEST_METHOD(WriteConsoleOutputCharacterWRunoff);

    TEST_METHOD(WriteConsoleOutputAttributeSimpleTest);
    TEST_METHOD(WriteConsoleOutputAttributeCheckerTest);

    TEST_METHOD(WriteBackspaceTest);

    TEST_METHOD(WinPtyWrite);
};

bool OutputTests::TestSetup()
{
    return Common::TestBufferSetup();
}

bool OutputTests::TestCleanup()
{
    return Common::TestBufferCleanup();
}

void OutputTests::BasicWriteConsoleOutputWTest()
{
    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;

    // Establish a writing region that is the width of the buffer and half the height.
    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Make a test value and fill an array (via a vector) full of it.
    CHAR_INFO testValue;
    testValue.Attributes = 0x3e;
    testValue.Char.UnicodeChar = L' ';

    std::vector<CHAR_INFO> buffer(regionSize, testValue);

    // Call the API and confirm results.
    SMALL_RECT affected = region;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(region, affected);
}

void OutputTests::BasicWriteConsoleOutputATest()
{
    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;

    // Establish a writing region that is the width of the buffer and half the height.
    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Make a test value and fill an array (via a vector) full of it.
    CHAR_INFO testValue;
    testValue.Attributes = 0x3e;
    testValue.Char.AsciiChar = ' ';

    std::vector<CHAR_INFO> buffer(regionSize, testValue);

    // Call the API and confirm results.
    SMALL_RECT affected = region;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputA(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(region, affected);
}

void OutputTests::WriteConsoleOutputWOutsideBuffer()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();

    // OneCore systems can't adjust the window/buffer size, so we'll skip making it smaller.
    // On Desktop systems, make it smaller so the test runs faster.
    if (OneCoreDelay::IsIsWindowPresent())
    {
        SMALL_RECT window = { 0 };
        window.Right = 5;
        window.Bottom = 5;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleWindowInfo(consoleOutputHandle, true, &window));
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(consoleOutputHandle, { 20, 20 }));
    }

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;

    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Make a test value and fill an array (via a vector) full of it.
    CHAR_INFO testValue;
    testValue.Attributes = 0x3e;
    testValue.Char.UnicodeChar = L'A';

    std::vector<CHAR_INFO> buffer(regionSize, testValue);

    // Call the API and confirm results.

    //  move outside in X and Y directions
    auto shiftedRegion = region;
    shiftedRegion.Left += bufferSize.X;
    shiftedRegion.Right += bufferSize.X;
    shiftedRegion.Top += bufferSize.Y;
    shiftedRegion.Bottom += bufferSize.Y;

    auto affected = shiftedRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(shiftedRegion, affected);

    // Read the entire buffer back and validate that we didn't write anything anywhere
    const auto readBack = std::make_unique<CHAR_INFO[]>(sbiex.dwSize.X * sbiex.dwSize.Y);
    SMALL_RECT readRegion = { 0 };
    readRegion.Bottom = sbiex.dwSize.Y - 1;
    readRegion.Right = sbiex.dwSize.X - 1;
    const auto readBefore = readRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(consoleOutputHandle, readBack.get(), sbiex.dwSize, { 0, 0 }, &readRegion));
    VERIFY_ARE_EQUAL(readBefore, readRegion);

    for (auto row = 0; row < sbiex.dwSize.Y; row++)
    {
        for (auto col = 0; col < sbiex.dwSize.X; col++)
        {
            CHAR_INFO readItem = *(readBack.get() + (row * sbiex.dwSize.X) + col);

            CHAR_INFO expectedItem;
            expectedItem.Char.UnicodeChar = L' ';
            expectedItem.Attributes = sbiex.wAttributes;

            VERIFY_ARE_EQUAL(expectedItem, readItem);
        }
    }
}

void OutputTests::WriteConsoleOutputWWithClipping()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();

    // OneCore systems can't adjust the window/buffer size, so we'll skip making it smaller.
    // On Desktop systems, make it smaller so the test runs faster.
    if (OneCoreDelay::IsIsWindowPresent())
    {
        SMALL_RECT window = { 0 };
        window.Right = 5;
        window.Bottom = 5;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleWindowInfo(consoleOutputHandle, true, &window));
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(consoleOutputHandle, { 20, 20 }));
    }

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;

    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Make a test value and fill an array (via a vector) full of it.
    CHAR_INFO testValue;
    testValue.Attributes = 0x3e;
    testValue.Char.UnicodeChar = L'A';

    std::vector<CHAR_INFO> buffer(regionSize, testValue);

    // Move the write region to get clipped in the X and the Y dimension.
    auto adjustedRegion = region;
    adjustedRegion.Left += 5;
    adjustedRegion.Right += 5;
    adjustedRegion.Top += bufferSize.Y / 2;
    adjustedRegion.Bottom += bufferSize.Y / 2;

    auto expectedRegion = adjustedRegion;
    expectedRegion.Left = max(0, adjustedRegion.Left);
    expectedRegion.Top = max(0, adjustedRegion.Top);
    expectedRegion.Right = min(bufferSize.X - 1, adjustedRegion.Right);
    expectedRegion.Bottom = min(bufferSize.Y - 1, adjustedRegion.Bottom);

    // Call the API and confirm results.
    auto affected = adjustedRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(expectedRegion, affected);

    // Read the entire buffer back and validate that we only wrote where we expected to write
    const auto readBack = std::make_unique<CHAR_INFO[]>(sbiex.dwSize.X * sbiex.dwSize.Y);
    SMALL_RECT readRegion = { 0 };
    readRegion.Bottom = sbiex.dwSize.Y - 1;
    readRegion.Right = sbiex.dwSize.X - 1;
    const auto readBefore = readRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(consoleOutputHandle, readBack.get(), sbiex.dwSize, { 0, 0 }, &readRegion));
    VERIFY_ARE_EQUAL(readBefore, readRegion);

    for (auto row = 0; row < sbiex.dwSize.Y; row++)
    {
        for (auto col = 0; col < sbiex.dwSize.X; col++)
        {
            CHAR_INFO readItem = *(readBack.get() + (row * sbiex.dwSize.X) + col);

            CHAR_INFO expectedItem;
            if (affected.Top <= row && affected.Bottom >= row && affected.Left <= col && affected.Right >= col)
            {
                expectedItem = testValue;
            }
            else
            {
                expectedItem.Char.UnicodeChar = L' ';
                expectedItem.Attributes = sbiex.wAttributes;
            }

            VERIFY_ARE_EQUAL(expectedItem, readItem);
        }
    }
}

void OutputTests::WriteConsoleOutputWNegativePositions()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();

    // OneCore systems can't adjust the window/buffer size, so we'll skip making it smaller.
    // On Desktop systems, make it smaller so the test runs faster.
    if (OneCoreDelay::IsIsWindowPresent())
    {
        SMALL_RECT window = { 0 };
        window.Right = 5;
        window.Bottom = 5;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleWindowInfo(consoleOutputHandle, true, &window));
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(consoleOutputHandle, { 20, 20 }));
    }

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;

    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Make a test value and fill an array (via a vector) full of it.
    CHAR_INFO testValue;
    testValue.Attributes = 0x3e;
    testValue.Char.UnicodeChar = L'A';

    std::vector<CHAR_INFO> buffer(regionSize, testValue);

    // Call the API and confirm results.

    // Move the write region to negative values in the X and Y dimension
    auto adjustedRegion = region;
    adjustedRegion.Left -= 3;
    adjustedRegion.Right -= 3;
    adjustedRegion.Top -= 10;
    adjustedRegion.Bottom -= 10;

    auto expectedRegion = adjustedRegion;
    expectedRegion.Left = max(0, adjustedRegion.Left);
    expectedRegion.Top = max(0, adjustedRegion.Top);
    expectedRegion.Right = min(bufferSize.X - 1, adjustedRegion.Right);
    expectedRegion.Bottom = min(bufferSize.Y - 1, adjustedRegion.Bottom);

    // Call the API and confirm results.
    auto affected = adjustedRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(expectedRegion, affected);

    // Read the entire buffer back and validate that we only wrote where we expected to write
    const auto readBack = std::make_unique<CHAR_INFO[]>(sbiex.dwSize.X * sbiex.dwSize.Y);
    SMALL_RECT readRegion = { 0 };
    readRegion.Bottom = sbiex.dwSize.Y - 1;
    readRegion.Right = sbiex.dwSize.X - 1;
    const auto readBefore = readRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(consoleOutputHandle, readBack.get(), sbiex.dwSize, { 0, 0 }, &readRegion));
    VERIFY_ARE_EQUAL(readBefore, readRegion);

    for (auto row = 0; row < sbiex.dwSize.Y; row++)
    {
        for (auto col = 0; col < sbiex.dwSize.X; col++)
        {
            CHAR_INFO readItem = *(readBack.get() + (row * sbiex.dwSize.X) + col);

            CHAR_INFO expectedItem;
            if (affected.Top <= row && affected.Bottom >= row && affected.Left <= col && affected.Right >= col)
            {
                expectedItem = testValue;
            }
            else
            {
                expectedItem.Char.UnicodeChar = L' ';
                expectedItem.Attributes = sbiex.wAttributes;
            }

            VERIFY_ARE_EQUAL(expectedItem, readItem);
        }
    }

    // Set the region so the left will end up past the right
    adjustedRegion = region;
    adjustedRegion.Left = -(adjustedRegion.Right + 1);
    affected = adjustedRegion;
    VERIFY_WIN32_BOOL_FAILED(WriteConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
}

void OutputTests::WriteConsoleOutputCharacterWRunoff()
{
    // writes text that will not all fit on the screen to verify reported size is correct
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;

    COORD target{ bufferSize.X - 1, bufferSize.Y - 1 };

    const std::wstring text = L"hello";
    DWORD charsWritten = 0;
    VERIFY_SUCCEEDED(WriteConsoleOutputCharacterW(consoleOutputHandle,
                                                  text.c_str(),
                                                  gsl::narrow<DWORD>(text.size()),
                                                  target,
                                                  &charsWritten));
    VERIFY_ARE_EQUAL(charsWritten, 1u);
}

void OutputTests::WriteConsoleOutputAttributeSimpleTest()
{
    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    const DWORD size = 500;
    const WORD setAttr = FOREGROUND_BLUE | BACKGROUND_RED;
    const COORD coord{ 0, 0 };
    DWORD attrsWritten = 0;
    WORD attributes[size];
    std::fill_n(attributes, size, setAttr);

    // write some attribute changes
    VERIFY_SUCCEEDED(WriteConsoleOutputAttribute(consoleOutputHandle, attributes, size, coord, &attrsWritten));
    VERIFY_ARE_EQUAL(attrsWritten, size);

    // confirm change happened
    WORD resultAttrs[size];
    DWORD attrsRead = 0;
    VERIFY_SUCCEEDED(ReadConsoleOutputAttribute(consoleOutputHandle, resultAttrs, size, coord, &attrsRead));
    VERIFY_ARE_EQUAL(attrsRead, size);

    for (size_t i = 0; i < size; ++i)
    {
        VERIFY_ARE_EQUAL(attributes[i], resultAttrs[i]);
    }
}

void OutputTests::WriteConsoleOutputAttributeCheckerTest()
{
    // writes a red/green checkerboard pattern on top of some text and makes sure that the text and color attr
    // changes roundtrip properly through the API

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;

    const WORD red = BACKGROUND_RED;
    const WORD green = BACKGROUND_GREEN;

    const DWORD height = 8;
    const DWORD width = bufferSize.X;
    // todo verify less than or equal to buffer size ^^^
    const DWORD size = width * height;
    std::unique_ptr<WORD[]> attrs = std::make_unique<WORD[]>(size);

    std::generate(attrs.get(), attrs.get() + size, [=]() {
        static int i = 0;
        return i++ % 2 == 0 ? red : green;
    });

    // write text
    const COORD coord{ 0, 0 };
    DWORD charsWritten = 0;
    std::unique_ptr<wchar_t[]> wchs = std::make_unique<wchar_t[]>(size);
    std::fill_n(wchs.get(), size, L'*');
    VERIFY_SUCCEEDED(WriteConsoleOutputCharacter(consoleOutputHandle, wchs.get(), size, coord, &charsWritten));
    VERIFY_ARE_EQUAL(charsWritten, size);

    // write attribute changes
    DWORD attrsWritten = 0;
    VERIFY_SUCCEEDED(WriteConsoleOutputAttribute(consoleOutputHandle, attrs.get(), size, coord, &attrsWritten));
    VERIFY_ARE_EQUAL(attrsWritten, size);

    // get changed attributes
    std::unique_ptr<WORD[]> resultAttrs = std::make_unique<WORD[]>(size);
    DWORD attrsRead = 0;
    VERIFY_SUCCEEDED(ReadConsoleOutputAttribute(consoleOutputHandle, resultAttrs.get(), size, coord, &attrsRead));
    VERIFY_ARE_EQUAL(attrsRead, size);

    // get text
    std::unique_ptr<wchar_t[]> resultWchs = std::make_unique<wchar_t[]>(size);
    DWORD charsRead = 0;
    VERIFY_SUCCEEDED(ReadConsoleOutputCharacter(consoleOutputHandle, resultWchs.get(), size, coord, &charsRead));
    VERIFY_ARE_EQUAL(charsRead, size);

    // confirm that attributes were set without affecting text
    for (size_t i = 0; i < size; ++i)
    {
        VERIFY_ARE_EQUAL(attrs[i], resultAttrs[i]);
        VERIFY_ARE_EQUAL(wchs[i], resultWchs[i]);
    }
}

void OutputTests::WriteBackspaceTest()
{
    // Get output buffer information.
    const auto hOut = GetStdOutputHandle();
    Log::Comment(NoThrowString().Format(
        L"Outputing \"\\b \\b\" should behave the same as \"\b\", \" \", \"\b\" in seperate WriteConsoleW calls."));

    DWORD n = 0;
    CONSOLE_SCREEN_BUFFER_INFO csbi = { 0 };
    COORD c = { 0, 0 };
    VERIFY_SUCCEEDED(SetConsoleCursorPosition(hOut, c));
    VERIFY_SUCCEEDED(WriteConsoleW(hOut, L"GoodX", 5, &n, nullptr));

    VERIFY_SUCCEEDED(GetConsoleScreenBufferInfo(hOut, &csbi));
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.X, 5);
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.Y, 0);

    VERIFY_SUCCEEDED(WriteConsoleW(hOut, L"\b", 1, &n, nullptr));
    VERIFY_SUCCEEDED(WriteConsoleW(hOut, L" ", 1, &n, nullptr));
    VERIFY_SUCCEEDED(WriteConsoleW(hOut, L"\b", 1, &n, nullptr));

    VERIFY_SUCCEEDED(GetConsoleScreenBufferInfo(hOut, &csbi));
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.X, 4);
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.Y, 0);

    VERIFY_SUCCEEDED(WriteConsoleW(hOut, L"\n", 1, &n, nullptr));

    VERIFY_SUCCEEDED(GetConsoleScreenBufferInfo(hOut, &csbi));
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.X, 0);
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.Y, 1);

    VERIFY_SUCCEEDED(WriteConsoleW(hOut, L"badX", 4, &n, nullptr));

    VERIFY_SUCCEEDED(GetConsoleScreenBufferInfo(hOut, &csbi));
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.X, 4);
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.Y, 1);

    VERIFY_SUCCEEDED(WriteConsoleW(hOut, L"\b \b", 3, &n, nullptr));

    VERIFY_SUCCEEDED(GetConsoleScreenBufferInfo(hOut, &csbi));
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.X, 3);
    VERIFY_ARE_EQUAL(csbi.dwCursorPosition.Y, 1);
}

void OutputTests::BasicReadConsoleOutputATest()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    // Get buffer information
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;
    const auto bufferLength = bufferSize.X * bufferSize.Y;

    // Establish a reading region that is the width of the buffer and half the height.
    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Fill buffer with some data to read back.
    CHAR_INFO ciFill = { 0 };
    ciFill.Char.AsciiChar = 'A';
    ciFill.Attributes = FOREGROUND_RED;

    DWORD written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterA(consoleOutputHandle, ciFill.Char.AsciiChar, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);
    written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputAttribute(consoleOutputHandle, ciFill.Attributes, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);

    // Make an array that can hold the output
    std::vector<CHAR_INFO> buffer(regionSize);

    // Call the API and confirm results
    SMALL_RECT affected = region;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputA(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(region, affected);

    // Verify that all the data read matches what was expected.
    for (const auto& ci : buffer)
    {
        VERIFY_ARE_EQUAL(ciFill, ci);
    }
}

void OutputTests::BasicReadConsoleOutputWTest()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    // Get buffer information
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;
    const auto bufferLength = bufferSize.X * bufferSize.Y;

    // Establish a reading region that is the width of the buffer and half the height.
    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Fill buffer with some data to read back.
    CHAR_INFO ciFill = { 0 };
    ciFill.Char.UnicodeChar = L'Z';
    ciFill.Attributes = FOREGROUND_RED;

    DWORD written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(consoleOutputHandle, ciFill.Char.AsciiChar, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);
    written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputAttribute(consoleOutputHandle, ciFill.Attributes, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);

    // Make an array that can hold the output
    std::vector<CHAR_INFO> buffer(regionSize);

    // Call the API and confirm results
    SMALL_RECT affected = region;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(region, affected);

    // Verify that all the data read matches what was expected.
    for (const auto& ci : buffer)
    {
        VERIFY_ARE_EQUAL(ciFill, ci);
    }
}

void OutputTests::ReadConsoleOutputWOutsideBuffer()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    // OneCore systems can't adjust the window/buffer size, so we'll skip making it smaller.
    // On Desktop systems, make it smaller so the test runs faster.
    if (OneCoreDelay::IsIsWindowPresent())
    {
        SMALL_RECT window = { 0 };
        window.Right = 5;
        window.Bottom = 5;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleWindowInfo(consoleOutputHandle, true, &window));
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(consoleOutputHandle, { 20, 20 }));
    }

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    // Get buffer information
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;
    const auto bufferLength = bufferSize.X * bufferSize.Y;

    // Establish a reading region that is the width of the buffer and half the height.
    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Fill buffer with some data to read back.
    CHAR_INFO ciFill = { 0 };
    ciFill.Char.UnicodeChar = L'Z';
    ciFill.Attributes = FOREGROUND_RED;

    DWORD written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(consoleOutputHandle, ciFill.Char.AsciiChar, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);
    written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputAttribute(consoleOutputHandle, ciFill.Attributes, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);

    // Make a buffer to hold the read data
    const CHAR_INFO ciEmpty = { 0 };
    std::vector<CHAR_INFO> buffer(regionSize, ciEmpty);

    // Try to read completely outside the buffer.
    auto shiftedRegion = region;
    shiftedRegion.Left += bufferSize.X;
    shiftedRegion.Right += bufferSize.X;
    shiftedRegion.Top += bufferSize.Y;
    shiftedRegion.Bottom += bufferSize.Y;

    auto expectedRegion = shiftedRegion;
    expectedRegion.Right = expectedRegion.Left - 1;
    expectedRegion.Bottom = expectedRegion.Top - 1;

    auto affected = shiftedRegion;
    VERIFY_WIN32_BOOL_FAILED(ReadConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(expectedRegion, affected);

    // Verify that all the data read matches what was expected.
    for (const auto& ci : buffer)
    {
        VERIFY_ARE_EQUAL(ciEmpty, ci);
    }
}

void OutputTests::ReadConsoleOutputWWithClipping()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    // OneCore systems can't adjust the window/buffer size, so we'll skip making it smaller.
    // On Desktop systems, make it smaller so the test runs faster.
    if (OneCoreDelay::IsIsWindowPresent())
    {
        SMALL_RECT window = { 0 };
        window.Right = 5;
        window.Bottom = 5;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleWindowInfo(consoleOutputHandle, true, &window));
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(consoleOutputHandle, { 20, 20 }));
    }

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    // Get buffer information
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;
    const auto bufferLength = bufferSize.X * bufferSize.Y;

    // Establish a reading region that is the width of the buffer and half the height.
    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Fill buffer with some data to read back.
    CHAR_INFO ciFill = { 0 };
    ciFill.Char.UnicodeChar = L'Z';
    ciFill.Attributes = FOREGROUND_RED;

    DWORD written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(consoleOutputHandle, ciFill.Char.AsciiChar, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);
    written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputAttribute(consoleOutputHandle, ciFill.Attributes, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);

    // Make a buffer to hold the read data
    CHAR_INFO ciEmpty;
    ciEmpty.Char.UnicodeChar = L'A';
    ciEmpty.Attributes = BACKGROUND_BLUE;
    std::vector<CHAR_INFO> buffer(regionSize, ciEmpty);

    // Move the write region to get clipped in the X and the Y dimension.
    auto adjustedRegion = region;
    adjustedRegion.Left += 5;
    adjustedRegion.Right += 5;
    adjustedRegion.Top += bufferSize.Y / 2;
    adjustedRegion.Bottom += bufferSize.Y / 2;

    auto expectedRegion = adjustedRegion;
    expectedRegion.Left = max(0, adjustedRegion.Left);
    expectedRegion.Top = max(0, adjustedRegion.Top);
    expectedRegion.Right = min(bufferSize.X - 1, adjustedRegion.Right);
    expectedRegion.Bottom = min(bufferSize.Y - 1, adjustedRegion.Bottom);

    // Call the API and confirm results.
    // NOTE: We expect this to be broken for v1. It's always been wrong there (returning a clipped count of bytes instead of the whole rectangle).
    auto affected = adjustedRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(expectedRegion, affected);

    const auto affectedViewport = Viewport::FromInclusive(affected);
    const auto filledBuffer = Viewport::FromDimensions({ 0, 0 }, affectedViewport.Dimensions());

    for (SHORT row = 0; row < regionDimensions.Y; row++)
    {
        for (SHORT col = 0; col < regionDimensions.X; col++)
        {
            CHAR_INFO bufferItem = *(buffer.begin() + (row * regionDimensions.X) + col);

            CHAR_INFO expectedItem;
            if (filledBuffer.IsInBounds({ col, row }))
            {
                expectedItem = ciFill;
            }
            else
            {
                expectedItem = ciEmpty;
            }

            VERIFY_ARE_EQUAL(expectedItem, bufferItem);
        }
    }
}

void OutputTests::ReadConsoleOutputWNegativePositions()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    // OneCore systems can't adjust the window/buffer size, so we'll skip making it smaller.
    // On Desktop systems, make it smaller so the test runs faster.
    if (OneCoreDelay::IsIsWindowPresent())
    {
        SMALL_RECT window = { 0 };
        window.Right = 5;
        window.Bottom = 5;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleWindowInfo(consoleOutputHandle, true, &window));
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(consoleOutputHandle, { 20, 20 }));
    }

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    // Get buffer information
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;
    const auto bufferLength = bufferSize.X * bufferSize.Y;

    // Establish a reading region that is the width of the buffer and half the height.
    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;
    const COORD regionOrigin{ 0, 0 };

    // Fill buffer with some data to read back.
    CHAR_INFO ciFill = { 0 };
    ciFill.Char.UnicodeChar = L'Z';
    ciFill.Attributes = FOREGROUND_RED;

    DWORD written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(consoleOutputHandle, ciFill.Char.AsciiChar, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);
    written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputAttribute(consoleOutputHandle, ciFill.Attributes, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);

    // Make a buffer to hold the read data
    CHAR_INFO ciEmpty;
    ciEmpty.Char.UnicodeChar = L'A';
    ciEmpty.Attributes = BACKGROUND_BLUE;
    std::vector<CHAR_INFO> buffer(regionSize, ciEmpty);

    // Move the read region to negative values in the X and Y dimension
    auto adjustedRegion = region;
    adjustedRegion.Left -= 3;
    adjustedRegion.Right -= 3;
    adjustedRegion.Top -= 10;
    adjustedRegion.Bottom -= 10;

    auto expectedRegion = adjustedRegion;
    expectedRegion.Left = max(0, adjustedRegion.Left);
    expectedRegion.Top = max(0, adjustedRegion.Top);
    expectedRegion.Right = min(bufferSize.X - 1, adjustedRegion.Right);
    expectedRegion.Bottom = min(bufferSize.Y - 1, adjustedRegion.Bottom);

    // Call the API
    // NOTE: Due to the same reason as the ReadConsoleOutputWWithClipping test (the v1 buffer told the driver the wrong return buffer byte length)
    // we expect the test to fail on the v1 console. V2 reports the correct buffer byte length to the driver for the return payload.
    auto affected = adjustedRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(expectedRegion, affected);

    // Verify the data read affected only the expected area
    const auto affectedViewport = Viewport::FromInclusive(affected);

    // Because of the negative origin, the API will report that it filled starting at the 0 coordinate, but it believed
    // the original buffer's origin was at -3, -10. This means we have to read at that offset into the buffer we provided
    // for the data we requested.
    const auto filledBuffer = Viewport::FromDimensions({ 0, 0 }, affectedViewport.Dimensions());
    auto adjustedBuffer = Viewport::Offset(filledBuffer, { -adjustedRegion.Left, -adjustedRegion.Top });

    for (SHORT row = 0; row < regionDimensions.Y; row++)
    {
        for (SHORT col = 0; col < regionDimensions.X; col++)
        {
            CHAR_INFO bufferItem = *(buffer.begin() + (row * regionDimensions.X) + col);

            CHAR_INFO expectedItem;
            if (adjustedBuffer.IsInBounds({ col, row }))
            {
                expectedItem = ciFill;
            }
            else
            {
                expectedItem = ciEmpty;
            }

            VERIFY_ARE_EQUAL(expectedItem, bufferItem);
        }
    }
}

void OutputTests::ReadConsoleOutputWPartialUserBuffer()
{
    SetVerifyOutput vf(VerifyOutputSettings::LogOnlyFailures);

    // Get output buffer information.
    const auto consoleOutputHandle = GetStdOutputHandle();
    SetConsoleActiveScreenBuffer(consoleOutputHandle);

    // OneCore systems can't adjust the window/buffer size, so we'll skip making it smaller.
    // On Desktop systems, make it smaller so the test runs faster.
    if (OneCoreDelay::IsIsWindowPresent())
    {
        SMALL_RECT window = { 0 };
        window.Right = 5;
        window.Bottom = 5;
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleWindowInfo(consoleOutputHandle, true, &window));
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleScreenBufferSize(consoleOutputHandle, { 20, 20 }));
    }

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex{ 0 };
    sbiex.cbSize = sizeof(sbiex);

    // Get buffer information
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(consoleOutputHandle, &sbiex));
    const auto bufferSize = sbiex.dwSize;
    const auto bufferLength = bufferSize.X * bufferSize.Y;

    // Establish a reading region that is the width of the buffer and half the height.
    const SMALL_RECT region{ 0, 0, bufferSize.X - 1, bufferSize.Y / 2 };
    const COORD regionDimensions{ region.Right - region.Left + 1, region.Bottom - region.Top + 1 };
    const auto regionSize = regionDimensions.X * regionDimensions.Y;

    // Fill buffer with some data to read back.
    CHAR_INFO ciFill = { 0 };
    ciFill.Char.UnicodeChar = L'Z';
    ciFill.Attributes = FOREGROUND_RED;

    DWORD written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(consoleOutputHandle, ciFill.Char.AsciiChar, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);
    written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputAttribute(consoleOutputHandle, ciFill.Attributes, bufferLength, { 0, 0 }, &written));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(bufferLength), written);

    // Make an array that can hold the output prefilled with some data so we can confirm it is untouched
    CHAR_INFO ciEmpty;
    ciEmpty.Char.UnicodeChar = L'A';
    ciEmpty.Attributes = BACKGROUND_BLUE;
    std::vector<CHAR_INFO> buffer(regionSize, ciEmpty);

    // Only fill up a small portion of the region we allocated.
    // We're going to set the origin to the middle and say we only want to read into/out of the bottom right corner.
    const COORD regionOrigin{ regionDimensions.X / 2, regionDimensions.Y / 2 };

    // Create the area that we expect to be filled with data.
    SMALL_RECT expected;
    expected.Left = regionOrigin.X;
    expected.Right = regionDimensions.X - 1;
    expected.Top = regionOrigin.Y;
    expected.Bottom = regionDimensions.Y - 1;

    const auto filledExpected = Viewport::FromInclusive(expected);

    // translate the expected region into the origin at 0,0 because that's what the API will report.
    expected.Right -= expected.Left;
    expected.Left -= expected.Left;
    expected.Bottom -= expected.Top;
    expected.Top -= expected.Top;

    // Call the API and confirm results
    SMALL_RECT affected = region;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(consoleOutputHandle, buffer.data(), regionDimensions, regionOrigin, &affected));
    VERIFY_ARE_EQUAL(expected, affected);

    // Verify that all the data read matches what was expected.
    for (SHORT row = 0; row < regionDimensions.Y; row++)
    {
        for (SHORT col = 0; col < regionDimensions.X; col++)
        {
            CHAR_INFO bufferItem = *(buffer.begin() + (row * regionDimensions.X) + col);

            CHAR_INFO expectedItem;
            if (filledExpected.IsInBounds({ col, row }))
            {
                expectedItem = ciFill;
            }
            else
            {
                expectedItem = ciEmpty;
            }

            VERIFY_ARE_EQUAL(expectedItem, bufferItem);
        }
    }
}

// Send "Select All", then spawn a thread to hit ESC a moment later.
static void WinPtyTestStartSelection()
{
    const HWND hwnd = GetConsoleWindow();
    const int SC_CONSOLE_SELECT_ALL = 0xFFF5;
    SendMessage(hwnd, WM_SYSCOMMAND, SC_CONSOLE_SELECT_ALL, 0);
    auto press_escape = std::thread([=]() {
        Sleep(500);
        SendMessage(hwnd, WM_CHAR, 27, 0x00010001); // 0x00010001 is the repeat count (1) and scan code (1)
    });
    press_escape.detach();
}

template<typename T>
static void WinPtyDoWriteTest(
    const wchar_t* api_name,
    T* api_ptr,
    bool use_selection)
{
    if (use_selection)
        WinPtyTestStartSelection();
    char buf[] = "1234567890567890567890567890\n";
    DWORD actual = 0;
    const BOOL ret = api_ptr(
        GetStdHandle(STD_OUTPUT_HANDLE),
        buf,
        static_cast<DWORD>(strlen(buf)),
        &actual,
        NULL);
    const DWORD last_error = GetLastError();
    VERIFY_IS_TRUE(ret && actual == strlen(buf),
                   String().Format(L"%s: %s returned %d: actual=%u LastError=%u (%s)\n",
                                   ((ret && actual == strlen(buf)) ? L"SUCCESS" : L"ERROR"),
                                   api_name,
                                   ret,
                                   actual,
                                   last_error,
                                   use_selection ? L"select" : L"no-select"));
}

void OutputTests::WinPtyWrite()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:method", L"{0, 1}")
        TEST_METHOD_PROPERTY(L"Data:selection", L"{true, false}")
    END_TEST_METHOD_PROPERTIES();

    if (!OneCoreDelay::IsIsWindowPresent())
    {
        Log::Comment(L"Scenario requiring window message triggers can't be checked on platform without classic window operations.");
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    DWORD method;
    bool selection;
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"method", method), L"Get which function mode we should use");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"selection", selection), L"Get whether we should use selection.");

    switch (method)
    {
    case 0:
        WinPtyDoWriteTest(L"WriteConsoleA", WriteConsoleA, selection);
        break;
    case 1:
        WinPtyDoWriteTest(L"WriteFile", WriteConsoleA, selection);
        break;
    default:
        VERIFY_FAIL(L"Unknown test type.");
        break;
    }
}
