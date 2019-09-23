// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "Common.hpp"

#include <future>

using WEX::Logging::Log;
using WEX::TestExecution::TestData;
using namespace WEX::Common;

// This class is intended to test:
// WriteFile

class FileTests
{
    // Method isolation level will completely close and re-open the OpenConsole session for every
    // TEST_METHOD below. This saves us the time of cleaning up the mode state and the contents of
    // the buffer and cursor position for each test. Launching a new OpenConsole is much quicker.
    BEGIN_TEST_CLASS(FileTests)
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_CLASS();

    TEST_METHOD(TestUtf8WriteFileInvalid);

    TEST_METHOD(TestWriteFileRaw);
    TEST_METHOD(TestWriteFileProcessed);

    BEGIN_TEST_METHOD(TestWriteFileWrapEOL)
        TEST_METHOD_PROPERTY(L"Data:fFlagOn", L"{true, false}")
    END_TEST_METHOD();

    BEGIN_TEST_METHOD(TestWriteFileVTProcessing)
        TEST_METHOD_PROPERTY(L"Data:fVtOn", L"{true, false}")
        TEST_METHOD_PROPERTY(L"Data:fProcessedOn", L"{true, false}")
    END_TEST_METHOD();

    BEGIN_TEST_METHOD(TestWriteFileDisableNewlineAutoReturn)
        TEST_METHOD_PROPERTY(L"Data:fDisableAutoReturn", L"{true, false}")
        TEST_METHOD_PROPERTY(L"Data:fProcessedOn", L"{true, false}")
    END_TEST_METHOD();

    TEST_METHOD(TestWriteFileSuspended);

    TEST_METHOD(TestReadFileBasic);
    TEST_METHOD(TestReadFileBasicSync);
    TEST_METHOD(TestReadFileBasicEmpty);
    TEST_METHOD(TestReadFileLine);
    TEST_METHOD(TestReadFileLineSync);

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(MethodSetup);
    TEST_METHOD_CLEANUP(MethodCleanup);

    /*BEGIN_TEST_METHOD(TestReadFileEcho)
        TEST_METHOD_PROPERTY(L"Data:fUseBlockedRead", L"{true, false}")
    END_TEST_METHOD();*/
};

static HANDLE _cancellationEvent = 0;

bool FileTests::ClassSetup()
{
    _cancellationEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    VERIFY_WIN32_BOOL_SUCCEEDED(!!_cancellationEvent, L"Create cancellation event.");
    return true;
}

bool FileTests::ClassCleanup()
{
    VERIFY_WIN32_BOOL_SUCCEEDED(CloseHandle(_cancellationEvent), L"Cleanup cancellation event.");
    return true;
}

bool FileTests::MethodSetup()
{
    VERIFY_WIN32_BOOL_SUCCEEDED(ResetEvent(_cancellationEvent), L"Reset cancellation event.");
    return true;
}

bool FileTests::MethodCleanup()
{
    VERIFY_WIN32_BOOL_SUCCEEDED(SetEvent(_cancellationEvent), L"Set cancellation event.");
    return true;
}

void FileTests::TestUtf8WriteFileInvalid()
{
    Log::Comment(L"Backup original console codepage.");
    UINT const uiOriginalCP = GetConsoleOutputCP();
    auto restoreOriginalCP = wil::scope_exit([&] {
        Log::Comment(L"Restore original console codepage.");
        SetConsoleOutputCP(uiOriginalCP);
    });

    HANDLE const hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify we have the standard output handle.");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(CP_UTF8), L"Set output codepage to UTF8");

    DWORD dwWritten;
    DWORD dwExpectedWritten;
    char* str;
    DWORD cbStr;

    // \x80 is an invalid UTF-8 continuation
    // \x40 is the @ symbol which is valid.
    str = "\x80\x40";
    cbStr = (DWORD)strlen(str);
    dwWritten = 0;
    dwExpectedWritten = cbStr;

    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(hOut, str, cbStr, &dwWritten, nullptr));
    VERIFY_ARE_EQUAL(dwExpectedWritten, dwWritten);

    // \x80 is an invalid UTF-8 continuation
    // \x40 is the @ symbol which is valid.
    str = "\x80\x40\x40";
    cbStr = (DWORD)strlen(str);
    dwWritten = 0;
    dwExpectedWritten = cbStr;

    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(hOut, str, cbStr, &dwWritten, nullptr));
    VERIFY_ARE_EQUAL(dwExpectedWritten, dwWritten);

    // \x80 is an invalid UTF-8 continuation
    // \x40 is the @ symbol which is valid.
    str = "\x80\x80\x80\x40";
    cbStr = (DWORD)strlen(str);
    dwWritten = 0;
    dwExpectedWritten = cbStr;

    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(hOut, str, cbStr, &dwWritten, nullptr));
    VERIFY_ARE_EQUAL(dwExpectedWritten, dwWritten);
}

void FileTests::TestWriteFileRaw()
{
    // \x7 is bell
    // \x8 is backspace
    // \x9 is tab
    // \xa is linefeed
    // \xd is carriage return
    // All should be ignored/printed in raw mode.
    PCSTR strTest = "z\x7y\x8z\x9y\xaz\xdy";
    DWORD const cchTest = (DWORD)strlen(strTest);
    String strReadBackExpected(strTest);

    HANDLE const hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify we have the standard output handle.");

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexBefore = { 0 };
    csbiexBefore.cbSize = sizeof(csbiexBefore);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexBefore), L"Retrieve screen buffer properties before writing.");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, 0), L"Set raw write mode.");

    COORD const coordZero = { 0 };
    VERIFY_ARE_EQUAL(coordZero, csbiexBefore.dwCursorPosition, L"Cursor should be at 0,0 in fresh buffer.");

    DWORD dwWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(hOut, strTest, cchTest, &dwWritten, nullptr), L"Write text into buffer using WriteFile");
    VERIFY_ARE_EQUAL(cchTest, dwWritten);

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexAfter = { 0 };
    csbiexAfter.cbSize = sizeof(csbiexAfter);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexAfter), L"Retrieve screen buffer properties after writing.");

    csbiexBefore.dwCursorPosition.X += (SHORT)cchTest;
    VERIFY_ARE_EQUAL(csbiexBefore.dwCursorPosition, csbiexAfter.dwCursorPosition, L"Verify cursor moved expected number of squares for the write length.");

    DWORD const cbReadBackBuffer = cchTest + 2; // +1 so we can read back a "space" that should be after what we wrote. +1 more so this can be null terminated for String class comparison.
    wistd::unique_ptr<char[]> strReadBack = wil::make_unique_failfast<char[]>(cbReadBackBuffer);
    ZeroMemory(strReadBack.get(), cbReadBackBuffer * sizeof(char));

    DWORD dwRead = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterA(hOut, strReadBack.get(), cchTest + 1, coordZero, &dwRead), L"Read back the data in the buffer.");
    // +1 to read back the space that should be after the text we wrote

    strReadBackExpected += " "; // add in the space that should appear after the written text (buffer should be space filled when empty)

    VERIFY_ARE_EQUAL(strReadBackExpected, String(strReadBack.get()), L"Ensure that the buffer contents match what we expected based on what we wrote.");
}

void WriteFileHelper(HANDLE hOut,
                     CONSOLE_SCREEN_BUFFER_INFOEX& csbiexBefore,
                     CONSOLE_SCREEN_BUFFER_INFOEX& csbiexAfter,
                     PCSTR psTest,
                     DWORD cchTest)
{
    csbiexBefore.cbSize = sizeof(csbiexBefore);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexBefore), L"Retrieve screen buffer properties before writing.");

    DWORD dwWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(hOut, psTest, cchTest, &dwWritten, nullptr), L"Write text into buffer using WriteFile");
    VERIFY_ARE_EQUAL(cchTest, dwWritten, L"Verify all characters were written.");

    csbiexAfter.cbSize = sizeof(csbiexAfter);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexAfter), L"Retrieve screen buffer properties after writing.");
}

void ReadBackHelper(HANDLE hOut,
                    COORD coordReadBackPos,
                    DWORD dwReadBackLength,
                    wistd::unique_ptr<char[]>& pszReadBack)
{
    // Add one so it can be zero terminated.
    DWORD cbBuffer = dwReadBackLength + 1;
    wistd::unique_ptr<char[]> pszRead = wil::make_unique_failfast<char[]>(cbBuffer);
    ZeroMemory(pszRead.get(), cbBuffer * sizeof(char));

    DWORD dwRead = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterA(hOut, pszRead.get(), dwReadBackLength, coordReadBackPos, &dwRead), L"Read back data in the buffer.");
    VERIFY_ARE_EQUAL(dwReadBackLength, dwRead, L"Verify API reports we read back the number of characters we asked for.");

    pszReadBack.swap(pszRead);
}

void FileTests::TestWriteFileProcessed()
{
    // \x7 is bell
    // \x8 is backspace
    // \x9 is tab
    // \xa is linefeed
    // \xd is carriage return
    // All should cause activity in processed mode.

    HANDLE const hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify we have the standard output handle.");

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexOriginal = { 0 };
    csbiexOriginal.cbSize = sizeof(csbiexOriginal);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexOriginal), L"Retrieve screen buffer properties at beginning of test.");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, ENABLE_PROCESSED_OUTPUT), L"Set processed write mode.");

    COORD const coordZero = { 0 };
    VERIFY_ARE_EQUAL(coordZero, csbiexOriginal.dwCursorPosition, L"Cursor should be at 0,0 in fresh buffer.");

    // Declare variables needed for each character test.
    CONSOLE_SCREEN_BUFFER_INFOEX csbiexBefore = { 0 };
    CONSOLE_SCREEN_BUFFER_INFOEX csbiexAfter = { 0 };
    COORD coordExpected = { 0 };
    PCSTR pszTest;
    DWORD cchTest;
    PCSTR pszReadBackExpected;
    DWORD cchReadBack;
    wistd::unique_ptr<char[]> pszReadBack;

    // 1. Test bell (\x7)
    {
        pszTest = "z\x7";
        cchTest = (DWORD)strlen(pszTest);
        pszReadBackExpected = "z ";
        cchReadBack = (DWORD)strlen(pszReadBackExpected);

        // Write z and a bell. Cursor should move once as bell should have made audible noise (can't really test) and not moved or printed anything.
        WriteFileHelper(hOut, csbiexBefore, csbiexAfter, pszTest, cchTest);
        coordExpected = csbiexBefore.dwCursorPosition;
        coordExpected.X += 1;
        VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Verify cursor moved once for printable character and not for bell.");

        // Read back written data.
        ReadBackHelper(hOut, csbiexBefore.dwCursorPosition, cchReadBack, pszReadBack);
        VERIFY_ARE_EQUAL(String(pszReadBackExpected), String(pszReadBack.get()), L"Verify text matches what we expected to be written into the buffer.");
    }

    // 2. Test backspace (\x8)
    {
        pszTest = "yx\x8";
        cchTest = (DWORD)strlen(pszTest);
        pszReadBackExpected = "yx ";
        cchReadBack = (DWORD)strlen(pszReadBackExpected);

        // Write two characters and a backspace. Cursor should move only one forward as the backspace should have moved the cursor back one after printing the second character.
        // The backspace character itself is typically non-destructive so it should only affect the cursor, not the buffer contents.
        WriteFileHelper(hOut, csbiexBefore, csbiexAfter, pszTest, cchTest);
        coordExpected = csbiexBefore.dwCursorPosition;
        coordExpected.X += 1;
        VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Verify cursor moved twice forward for printable characters and once backward for backspace.");

        // Read back written data.
        ReadBackHelper(hOut, csbiexBefore.dwCursorPosition, cchReadBack, pszReadBack);
        VERIFY_ARE_EQUAL(String(pszReadBackExpected), String(pszReadBack.get()), L"Verify text matches what we expected to be written into the buffer.");
    }

    // 3. Test tab (\x9)
    {
        // The tab character will space pad out the buffer to the next multiple-of-8 boundary.
        // NOTE: This is dependent on the previous tests running first.
        pszTest = "\x9";
        cchTest = (DWORD)strlen(pszTest);
        pszReadBackExpected = "     ";
        cchReadBack = (DWORD)strlen(pszReadBackExpected);

        // Write tab character. Cursor should move out to the next multiple-of-8 and leave space characters in its wake.
        WriteFileHelper(hOut, csbiexBefore, csbiexAfter, pszTest, cchTest);
        coordExpected = csbiexBefore.dwCursorPosition;
        coordExpected.X = 8;
        VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Verify cursor moved forward to position 8 for tab.");

        // Read back written data.
        ReadBackHelper(hOut, csbiexBefore.dwCursorPosition, cchReadBack, pszReadBack);
        VERIFY_ARE_EQUAL(String(pszReadBackExpected), String(pszReadBack.get()), L"Verify text matches what we expected to be written into the buffer.");
    }

    // 4. Test linefeed (\xa)
    {
        // The line feed character should move us down to the next line.
        pszTest = "\xaQ";
        cchTest = (DWORD)strlen(pszTest);
        pszReadBackExpected = "Q ";
        cchReadBack = (DWORD)strlen(pszReadBackExpected);

        // Write line feed character. Cursor should move down a line and then the Q from our string should be printed.
        WriteFileHelper(hOut, csbiexBefore, csbiexAfter, pszTest, cchTest);
        coordExpected.X = 1;
        coordExpected.Y = 1;
        VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Verify cursor moved down a line and then one character over for linefeed + Q.");

        // Read back written data from the 2nd line.
        COORD coordRead;
        coordRead.Y = 1;
        coordRead.X = 0;
        ReadBackHelper(hOut, coordRead, cchReadBack, pszReadBack);
        VERIFY_ARE_EQUAL(String(pszReadBackExpected), String(pszReadBack.get()), L"Verify text matches what we expected to be written into the buffer.");
    }

    // 5. Test carriage return (\xd)
    {
        // The carriage return character should move us to the front of the line.
        pszTest = "J\xd";
        cchTest = (DWORD)strlen(pszTest);
        pszReadBackExpected = "QJ "; // J written, then move to beginning of line.
        cchReadBack = (DWORD)strlen(pszReadBackExpected);

        // Write text and carriage return character. Cursor should end up at the beginning of this line. The J should have been printed in the line before we moved.
        WriteFileHelper(hOut, csbiexBefore, csbiexAfter, pszTest, cchTest);
        coordExpected = csbiexBefore.dwCursorPosition;
        coordExpected.X = 0;
        VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Verify cursor moved to beginning of line for carriage return character.");

        // Read back text written from the 2nd line.
        ReadBackHelper(hOut, csbiexAfter.dwCursorPosition, cchReadBack, pszReadBack);
        VERIFY_ARE_EQUAL(String(pszReadBackExpected), String(pszReadBack.get()), L"Verify text matches what we expected to be written into the buffer.");
    }

    // 6. Print a character over the top of the existing
    {
        // After the carriage return, try typing on top of the Q with a K
        pszTest = "K";
        cchTest = (DWORD)strlen(pszTest);
        pszReadBackExpected = "KJ "; // NOTE: This is based on the previous test(s).
        cchReadBack = (DWORD)strlen(pszReadBackExpected);

        // Write text. Cursor should end up on top of the J.
        WriteFileHelper(hOut, csbiexBefore, csbiexAfter, pszTest, cchTest);
        coordExpected = csbiexBefore.dwCursorPosition;
        coordExpected.X += 1;
        VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Verify cursor moved over one for printing character.");

        // Read back text written from the 2nd line.
        ReadBackHelper(hOut, csbiexBefore.dwCursorPosition, cchReadBack, pszReadBack);
        VERIFY_ARE_EQUAL(String(pszReadBackExpected), String(pszReadBack.get()), L"Verify text matches what we expected to be written into the buffer.");
    }
}

void FileTests::TestWriteFileWrapEOL()
{
    bool fFlagOn;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fFlagOn", fFlagOn));

    HANDLE const hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify we have the standard output handle.");

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexOriginal = { 0 };
    csbiexOriginal.cbSize = sizeof(csbiexOriginal);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexOriginal), L"Retrieve screen buffer properties at beginning of test.");

    if (fFlagOn)
    {
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, ENABLE_WRAP_AT_EOL_OUTPUT), L"Set wrap at EOL.");
    }
    else
    {
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, 0), L"Make sure wrap at EOL is off.");
    }

    COORD const coordZero = { 0 };
    VERIFY_ARE_EQUAL(coordZero, csbiexOriginal.dwCursorPosition, L"Cursor should be at 0,0 in fresh buffer.");

    // Fill first row of the buffer with Z characters until 1 away from the end.
    for (SHORT i = 0; i < csbiexOriginal.dwSize.X - 1; i++)
    {
        WriteFile(hOut, "Z", 1, nullptr, nullptr);
    }

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexBefore = { 0 };
    csbiexBefore.cbSize = sizeof(csbiexBefore);
    CONSOLE_SCREEN_BUFFER_INFOEX csbiexAfter = { 0 };
    csbiexAfter.cbSize = sizeof(csbiexAfter);
    COORD coordExpected = { 0 };

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexBefore), L"Get cursor position information before attempting to wrap at end of line.");
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(hOut, "Y", 1, nullptr, nullptr), L"Write of final character in line succeeded.");
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexAfter), L"Get cursor position information after attempting to wrap at end of line.");

    if (fFlagOn)
    {
        Log::Comment(L"Cursor should go down a row if we tried to print at end of line.");
        coordExpected = csbiexBefore.dwCursorPosition;
        coordExpected.Y++;
        coordExpected.X = 0;
    }
    else
    {
        Log::Comment(L"Cursor shouldn't move when printing at end of line.");
        coordExpected = csbiexBefore.dwCursorPosition;
    }

    VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Verify cursor moved as expected based on flag state.");
}

void FileTests::TestWriteFileVTProcessing()
{
    bool fVtOn;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fVtOn", fVtOn));

    bool fProcessedOn;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fProcessedOn", fProcessedOn));

    HANDLE const hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify we have the standard output handle.");

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexOriginal = { 0 };
    csbiexOriginal.cbSize = sizeof(csbiexOriginal);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexOriginal), L"Retrieve screen buffer properties at beginning of test.");

    DWORD dwFlags = 0;
    WI_SetFlagIf(dwFlags, ENABLE_VIRTUAL_TERMINAL_PROCESSING, fVtOn);
    WI_SetFlagIf(dwFlags, ENABLE_PROCESSED_OUTPUT, fProcessedOn);
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, dwFlags), L"Turn on relevant flags for test.");

    COORD const coordZero = { 0 };
    VERIFY_ARE_EQUAL(coordZero, csbiexOriginal.dwCursorPosition, L"Cursor should be at 0,0 in fresh buffer.");

    PCSTR pszTestString = "\x1b"
                          "[14m";
    DWORD const cchTest = (DWORD)strlen(pszTestString);

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexBefore = { 0 };
    csbiexBefore.cbSize = sizeof(csbiexBefore);
    CONSOLE_SCREEN_BUFFER_INFOEX csbiexAfter = { 0 };
    csbiexAfter.cbSize = sizeof(csbiexAfter);

    WriteFileHelper(hOut, csbiexBefore, csbiexAfter, pszTestString, cchTest);

    // We only expect characters to be processed and not printed if both processed mode and VT mode are on.
    bool const fProcessedNotPrinted = fProcessedOn && fVtOn;

    if (fProcessedNotPrinted)
    {
        PCSTR pszReadBackExpected = "      ";
        DWORD const cchReadBackExpected = (DWORD)strlen(pszReadBackExpected);

        VERIFY_ARE_EQUAL(csbiexBefore.dwCursorPosition, csbiexAfter.dwCursorPosition, L"Verify cursor didn't move because the VT sequence was processed instead of printed.");

        wistd::unique_ptr<char[]> pszReadBack;
        ReadBackHelper(hOut, coordZero, cchReadBackExpected, pszReadBack);
        VERIFY_ARE_EQUAL(String(pszReadBackExpected), String(pszReadBack.get()), L"Verify that nothing was printed into the buffer.");
    }
    else
    {
        COORD coordExpected = csbiexBefore.dwCursorPosition;
        coordExpected.X += (SHORT)cchTest;
        VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Verify cursor moved as characters should have been emitted, not consumed.");

        wistd::unique_ptr<char[]> pszReadBack;
        ReadBackHelper(hOut, coordZero, cchTest, pszReadBack);
        VERIFY_ARE_EQUAL(String(pszTestString), String(pszReadBack.get()), L"Verify that original test string was printed into the buffer.");
    }
}

void FileTests::TestWriteFileDisableNewlineAutoReturn()
{
    bool fDisableAutoReturn;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fDisableAutoReturn", fDisableAutoReturn));

    bool fProcessedOn;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fProcessedOn", fProcessedOn));

    HANDLE const hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify we have the standard output handle.");

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexOriginal = { 0 };
    csbiexOriginal.cbSize = sizeof(csbiexOriginal);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexOriginal), L"Retrieve screen buffer properties at beginning of test.");

    DWORD dwMode = 0;
    WI_SetFlagIf(dwMode, DISABLE_NEWLINE_AUTO_RETURN, fDisableAutoReturn);
    WI_SetFlagIf(dwMode, ENABLE_PROCESSED_OUTPUT, fProcessedOn);
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, dwMode), L"Set console mode for test.");

    COORD const coordZero = { 0 };
    VERIFY_ARE_EQUAL(coordZero, csbiexOriginal.dwCursorPosition, L"Cursor should be at 0,0 in fresh buffer.");

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexBefore = { 0 };
    csbiexBefore.cbSize = sizeof(csbiexBefore);
    CONSOLE_SCREEN_BUFFER_INFOEX csbiexAfter = { 0 };
    csbiexAfter.cbSize = sizeof(csbiexAfter);
    COORD coordExpected = { 0 };

    WriteFileHelper(hOut, csbiexBefore, csbiexAfter, "abc", 3);
    coordExpected = csbiexBefore.dwCursorPosition;
    coordExpected.X += 3;
    VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Cursor should have moved right to the end of the text written.");

    WriteFileHelper(hOut, csbiexBefore, csbiexAfter, "\n", 1);

    if (fProcessedOn)
    {
        if (fDisableAutoReturn)
        {
            coordExpected = csbiexBefore.dwCursorPosition;
            coordExpected.Y += 1;
        }
        else
        {
            coordExpected = csbiexBefore.dwCursorPosition;
            coordExpected.Y += 1;
            coordExpected.X = 0;
        }
    }
    else
    {
        coordExpected = csbiexBefore.dwCursorPosition;
        coordExpected.X += 1;
    }

    VERIFY_ARE_EQUAL(coordExpected, csbiexAfter.dwCursorPosition, L"Cursor should move to expected position.");
}

void SendKeyHelper(HANDLE hIn, WORD vk)
{
    INPUT_RECORD irPause = { 0 };
    irPause.EventType = KEY_EVENT;
    irPause.Event.KeyEvent.bKeyDown = TRUE;
    irPause.Event.KeyEvent.wVirtualKeyCode = vk;

    DWORD dwWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(hIn, &irPause, 1u, &dwWritten), L"Key event sent.");
}

void PauseHelper(HANDLE hIn)
{
    SendKeyHelper(hIn, VK_PAUSE);
}

void UnpauseHelper(HANDLE hIn)
{
    SendKeyHelper(hIn, VK_ESCAPE);
}

void FileTests::TestWriteFileSuspended()
{
    HANDLE const hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify we have the standard output handle.");

    HANDLE const hIn = GetStdInputHandle();
    VERIFY_IS_NOT_NULL(hIn, L"Verify we have the standard input handle.");

    CONSOLE_SCREEN_BUFFER_INFOEX csbiexOriginal = { 0 };
    csbiexOriginal.cbSize = sizeof(csbiexOriginal);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexOriginal), L"Retrieve screen buffer properties at beginning of test.");

    DWORD dwMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, dwMode), L"Set console mode for test.");

    COORD const coordZero = { 0 };
    VERIFY_ARE_EQUAL(coordZero, csbiexOriginal.dwCursorPosition, L"Cursor should be at 0,0 in fresh buffer.");

    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(hOut, "abc", 3, nullptr, nullptr), L"Test first write success.");
    PauseHelper(hIn);

    auto BlockedWrite = std::async([&] {
        Log::Comment(L"Background WriteFile scheduled.");
        VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(hOut, "def", 3, nullptr, nullptr), L"Test second write success.");
    });

    UnpauseHelper(hIn);

    BlockedWrite.wait();
}

void SendFullKeyStrokeHelper(HANDLE hIn, char ch)
{
    INPUT_RECORD ir[2];
    ZeroMemory(ir, ARRAYSIZE(ir) * sizeof(INPUT_RECORD));
    ir[0].EventType = KEY_EVENT;
    ir[0].Event.KeyEvent.bKeyDown = TRUE;
    ir[0].Event.KeyEvent.dwControlKeyState = ch < 0x20 ? LEFT_CTRL_PRESSED : 0; // set left_ctrl_pressed for control keys.
    ir[0].Event.KeyEvent.uChar.AsciiChar = ch;
    ir[0].Event.KeyEvent.wVirtualKeyCode = VkKeyScanA(ir[0].Event.KeyEvent.uChar.AsciiChar);
    ir[0].Event.KeyEvent.wVirtualScanCode = (WORD)MapVirtualKeyA(ir[0].Event.KeyEvent.wVirtualKeyCode, MAPVK_VK_TO_VSC);
    ir[0].Event.KeyEvent.wRepeatCount = 1;
    ir[1] = ir[0];
    ir[1].Event.KeyEvent.bKeyDown = FALSE;

    DWORD dwWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputA(hIn, ir, (DWORD)ARRAYSIZE(ir), &dwWritten), L"Writing key stroke.");
    VERIFY_ARE_EQUAL((DWORD)ARRAYSIZE(ir), dwWritten, L"Written matches expected.");
}

void FileTests::TestReadFileBasic()
{
    HANDLE const hIn = GetStdInputHandle();
    VERIFY_IS_NOT_NULL(hIn, L"Verify we have the standard input handle.");

    DWORD dwMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hIn, dwMode), L"Set input mode for test.");

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Flush input buffer in preparation for test.");

    char ch = '\0';
    Log::Comment(L"Queue background blocking read file operation.");
    auto BackgroundRead = std::async([&] {
        DWORD dwRead = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(hIn, &ch, 1, &dwRead, nullptr), L"Read file was successful.");
        VERIFY_ARE_EQUAL(1u, dwRead, L"Verify we read 1 character.");
    });

    char const chExpected = 'a';
    Log::Comment(L"Send a key into the console.");
    SendFullKeyStrokeHelper(hIn, chExpected);

    Log::Comment(L"Wait for background to unblock.");
    BackgroundRead.wait();
    VERIFY_ARE_EQUAL(chExpected, ch);
}

void FileTests::TestReadFileBasicSync()
{
    HANDLE const hIn = GetStdInputHandle();
    VERIFY_IS_NOT_NULL(hIn, L"Verify we have the standard input handle.");

    DWORD dwMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hIn, dwMode), L"Set input mode for test.");

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Flush input buffer in preparation for test.");

    char const chExpected = 'a';
    Log::Comment(L"Send a key into the console.");
    SendFullKeyStrokeHelper(hIn, chExpected);

    char ch = '\0';
    Log::Comment(L"Read with synchronous blocking read.");
    DWORD dwRead = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(hIn, &ch, 1, &dwRead, nullptr), L"Read file was successful.");
    VERIFY_ARE_EQUAL(1u, dwRead, L"Verify we read 1 character.");

    VERIFY_ARE_EQUAL(chExpected, ch);
}

void FileTests::TestReadFileBasicEmpty()
{
    HANDLE const hIn = GetStdInputHandle();
    VERIFY_IS_NOT_NULL(hIn, L"Verify we have the standard input handle.");

    DWORD dwMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hIn, dwMode), L"Set input mode for test.");

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Flush input buffer in preparation for test.");

    char ch = '\0';
    Log::Comment(L"Queue background blocking read file operation.");
    auto BackgroundRead = std::async([&] {
        DWORD dwRead = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(hIn, &ch, 1, &dwRead, nullptr), L"Read file was successful.");
        VERIFY_ARE_EQUAL(0u, dwRead, L"We should have read nothing back. It should just return from Ctrl+Z");
    });

    char const chExpected = '\x1a'; // ctrl+z character
    Log::Comment(L"Send a key into the console.");
    SendFullKeyStrokeHelper(hIn, chExpected);

    Log::Comment(L"Wait for background to unblock.");
    BackgroundRead.wait();
    VERIFY_ARE_EQUAL('\0', ch);
}

void FileTests::TestReadFileLine()
{
    HANDLE const hIn = GetStdInputHandle();
    VERIFY_IS_NOT_NULL(hIn, L"Verify we have the standard input handle.");

    DWORD dwMode = ENABLE_LINE_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hIn, dwMode), L"Set input mode for test.");

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Flush input buffer in preparation for test.");

    char ch = '\0';
    Log::Comment(L"Queue background blocking read file operation.");
    auto BackgroundRead = std::async([&] {
        DWORD dwRead = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(hIn, &ch, 1, &dwRead, nullptr), L"Read file was successful.");
        VERIFY_ARE_EQUAL(1u, dwRead, L"Verify we read 1 character.");
    });

    char const chExpected = 'a';
    Log::Comment(L"Send a key into the console.");
    SendFullKeyStrokeHelper(hIn, chExpected);

    auto status = BackgroundRead.wait_for(std::chrono::milliseconds(250));
    VERIFY_ARE_EQUAL(std::future_status::timeout, status, L"We should still be waiting for a result.");
    VERIFY_ARE_EQUAL('\0', ch, L"Character shouldn't be filled by background read yet.");

    Log::Comment(L"Send a line feed character, we should stay blocked.");
    SendFullKeyStrokeHelper(hIn, '\n');
    status = BackgroundRead.wait_for(std::chrono::milliseconds(250));
    VERIFY_ARE_EQUAL(std::future_status::timeout, status, L"We should still be waiting for a result.");
    VERIFY_ARE_EQUAL('\0', ch, L"Character shouldn't be filled by background read yet.");

    Log::Comment(L"Now send a carriage return into the console to signify the end of the input line.");
    SendFullKeyStrokeHelper(hIn, '\r');

    Log::Comment(L"Wait for background thread to unblock.");
    BackgroundRead.wait();
    VERIFY_ARE_EQUAL(chExpected, ch);
}

void FileTests::TestReadFileLineSync()
{
    HANDLE const hIn = GetStdInputHandle();
    VERIFY_IS_NOT_NULL(hIn, L"Verify we have the standard input handle.");

    DWORD dwMode = ENABLE_LINE_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hIn, dwMode), L"Set input mode for test.");

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Flush input buffer in preparation for test.");

    char const chExpected = 'a';
    Log::Comment(L"Send a key into the console followed by a carriage return.");
    SendFullKeyStrokeHelper(hIn, chExpected);
    SendFullKeyStrokeHelper(hIn, '\r');

    char ch = '\0';
    Log::Comment(L"Read back the input with a synchronous blocking read.");
    DWORD dwRead = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(hIn, &ch, 1, nullptr, nullptr), L"Read file was successful.");
    VERIFY_ARE_EQUAL(0u, dwRead, L"Verify we read 0 characters.");

    VERIFY_ARE_EQUAL(chExpected, ch);
}

//void FileTests::TestReadFileEcho()
//{
//    bool fUseBlockedRead;
//    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fUseBlockedRead", fUseBlockedRead));
//
//    HANDLE const hOut = GetStdOutputHandle();
//    VERIFY_IS_NOT_NULL(hOut, L"Verify we have the standard output handle.");
//
//    HANDLE const hIn = GetStdInputHandle();
//    VERIFY_IS_NOT_NULL(hIn, L"Verify we have the standard input handle.");
//
//    DWORD dwMode = ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
//    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hIn, dwMode), L"Set input mode for test.");
//
//    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Flush input buffer in preparation for test.");
//
//    CONSOLE_SCREEN_BUFFER_INFOEX csbiexOriginal = { 0 };
//    csbiexOriginal.cbSize = sizeof(csbiexOriginal);
//    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexOriginal), L"Retrieve output screen buffer information.");
//
//    COORD const coordZero = { 0 };
//    VERIFY_ARE_EQUAL(coordZero, csbiexOriginal.dwCursorPosition, L"We expect the cursor to be at 0,0 for the start of this test.");
//
//    char ch = '\0';
//    std::future<void> BackgroundRead;
//    if (fUseBlockedRead)
//    {
//        Log::Comment(L"Queue background blocking read file operation.");
//        BackgroundRead = std::async([&] {
//            OVERLAPPED overlapped = { 0 };
//            wil::unique_event evt;
//            evt.create();
//            overlapped.hEvent = evt.get();
//
//            DWORD dwRead = 0;
//            VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(hIn, &ch, 1, nullptr, &overlapped), L"Read file was dispatched successfully.");
//
//            std::array<HANDLE, 2> handles;
//            handles[0] = _cancellationEvent;
//            handles[1] = overlapped.hEvent;
//
//            WaitForMultipleObjects(2, handles.data(), FALSE, INFINITE);
//            Log::Comment(L"Wait complete.");
//
//            VERIFY_ARE_EQUAL(0u, dwRead, L"Verify we read 0 characters.");
//        });
//    }
//
//    Log::Comment(L"Read back the first line of the buffer to see that it is empty.");
//    wistd::unique_ptr<char[]> pszBefore;
//    PCSTR pszBeforeExpected = "     ";
//    DWORD const cchBeforeExpected = (DWORD)strlen(pszBeforeExpected);
//    ReadBackHelper(hOut, coordZero, cchBeforeExpected, pszBefore);
//    VERIFY_ARE_EQUAL(String(pszBeforeExpected), String(pszBefore.get()), L"Verify the first few characters of the buffer are empty (spaces)");
//
//    PCSTR pszAfterExpected = "qzmp ";
//    COORD coordCursorAfter = { 0 };
//    DWORD const cchAfterExpected = (DWORD)strlen(pszAfterExpected);
//
//    Log::Comment(L"Now write in a few input characters to the buffer.");
//    for (DWORD i = 0; i < cchAfterExpected - 1; i++)
//    {
//        SendFullKeyStrokeHelper(hIn, pszAfterExpected[i]);
//        coordCursorAfter.X++;
//    }
//
//    Log::Comment(L"Read back the first line of the buffer to see if we've echoed characters.");
//    wistd::unique_ptr<char[]> pszAfter;
//    ReadBackHelper(hOut, coordZero, cchAfterExpected, pszAfter);
//
//    if (fUseBlockedRead)
//    {
//        VERIFY_ARE_EQUAL(String(pszAfterExpected), String(pszAfter.get()), L"Verify the characters written were echoed into the buffer.");
//    }
//    else
//    {
//        VERIFY_ARE_EQUAL(String(pszBeforeExpected), String(pszAfter.get()), L"Verify nothing should have been printed while no one was waiting on a read.");
//    }
//
//    CONSOLE_SCREEN_BUFFER_INFOEX csbiexAfter = { 0 };
//    csbiexAfter.cbSize = sizeof(csbiexAfter);
//    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(hOut, &csbiexAfter), L"Get the cursor position after the writes.");
//
//    if (fUseBlockedRead)
//    {
//        VERIFY_ARE_EQUAL(coordCursorAfter, csbiexAfter.dwCursorPosition, L"Cursor should have moved with the writes.");
//    }
//    else
//    {
//        VERIFY_ARE_EQUAL(coordZero, csbiexAfter.dwCursorPosition, L"Cursor shouldn't move if no one is waiting with a read.");
//    }
//
//    if (fUseBlockedRead)
//    {
//        Log::Comment(L"Send newline to unblock the read.");
//        SendFullKeyStrokeHelper(hIn, '\r');
//        BackgroundRead.wait();
//    }
//}
