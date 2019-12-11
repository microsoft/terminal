// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

using WEX::Logging::Log;
using namespace WEX::Common;

// This class is intended to test boundary conditions for:
// SetConsoleActiveScreenBuffer
class BufferTests
{
    BEGIN_TEST_CLASS(BufferTests)
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_CLASS()

    TEST_METHOD(TestSetConsoleActiveScreenBufferInvalid);

    TEST_METHOD(TestCookedReadOnNonShareableScreenBuffer);

    BEGIN_TEST_METHOD(TestWritingInactiveScreenBuffer)
        TEST_METHOD_PROPERTY(L"Data:UseVtOutput", L"{true, false}")
    END_TEST_METHOD()

    TEST_METHOD(ScrollLargeBufferPerformance);

    TEST_METHOD(ChafaGifPerformance);
};

void BufferTests::TestSetConsoleActiveScreenBufferInvalid()
{
    VERIFY_WIN32_BOOL_FAILED(SetConsoleActiveScreenBuffer(INVALID_HANDLE_VALUE));
    VERIFY_WIN32_BOOL_FAILED(SetConsoleActiveScreenBuffer(nullptr));
}

void BufferTests::TestCookedReadOnNonShareableScreenBuffer()
{
    Log::Comment(L"Get original handles");
    const auto in = GetStdInputHandle();
    const auto out = GetStdOutputHandle();

    Log::Comment(L"Ensure cooked input is on (line input mode) and echoing to the screen.");
    DWORD inMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &inMode));
    inMode |= ENABLE_LINE_INPUT;
    inMode |= ENABLE_ECHO_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, inMode));

    Log::Comment(L"Create alternate buffer that is read/writeable but not shareable.");
    const auto otherBuffer = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                                       0, // This says non-sharable
                                                       nullptr,
                                                       CONSOLE_TEXTMODE_BUFFER,
                                                       nullptr);
    VERIFY_WIN32_BOOL_SUCCEEDED(INVALID_HANDLE_VALUE != otherBuffer);

    Log::Comment(L"Set the alternate buffer as active.");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleActiveScreenBuffer(otherBuffer));

    // On a cooked read with echoing, the act of reading from the buffer will cause a handle to be
    // taken to the active output buffer such that the cooked/line reading handler can display
    // what is being typed on the screen as it is being typed before the enter key is hit.
    // This should fail because we've denied anyone sharing access with us and we hold the primary
    // active handle above.
    Log::Comment(L"Perform a read operation to attempt to take handle to output buffer and hopefully fail.");
    char buffer[1];
    DWORD read = 0;
    SetLastError(S_OK);
    VERIFY_WIN32_BOOL_FAILED(ReadFile(in, buffer, sizeof(buffer), &read, nullptr));
    VERIFY_ARE_EQUAL(static_cast<DWORD>(ERROR_SHARING_VIOLATION), GetLastError());

    Log::Comment(L"Put the buffer back.");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleActiveScreenBuffer(out));

    Log::Comment(L"Close the alternate buffer.");
    VERIFY_WIN32_BOOL_SUCCEEDED(CloseHandle(otherBuffer));

    Sleep(2000);

    Log::Comment(L"Ensure that the console didn't die/crash");
    VERIFY_IS_TRUE(IsConsoleStillRunning());
}

void BufferTests::TestWritingInactiveScreenBuffer()
{
    bool useVtOutput;
    VERIFY_SUCCEEDED_RETURN(WEX::TestExecution::TestData::TryGetValue(L"UseVtOutput", useVtOutput), L"Get whether this test should check VT output mode.");

    const std::wstring primary(L"You should see me");
    const std::wstring alternative(L"You should NOT see me!");
    const std::wstring newline(L"\n");

    Log::Comment(L"Set up the output mode to either use VT processing or not (see test parameter)");
    const auto out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(out, &mode));
    WI_UpdateFlag(mode, ENABLE_VIRTUAL_TERMINAL_PROCESSING, useVtOutput);
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(out, mode));

    Log::Comment(L"Write one line of text to the active/main output buffer.");
    DWORD written = 0;
    // Ok in legacy mode, ok in modern mode
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleW(out, primary.data(), gsl::narrow<DWORD>(primary.size()), &written, nullptr));
    VERIFY_ARE_EQUAL(primary.size(), written);

    Log::Comment(L"Write a newline character to move the cursor down to the left most cell on the next line down.");
    // write a newline too to move the cursor down
    written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleW(out, newline.data(), gsl::narrow<DWORD>(newline.size()), &written, nullptr));
    VERIFY_ARE_EQUAL(newline.size(), written);

    Log::Comment(L"Create an alternative backing screen buffer that we will NOT be setting as active.");
    const auto handle = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
    VERIFY_IS_NOT_NULL(handle);

    // Ok in legacy mode, NOT ok in modern mode.
    Log::Comment(L"Try to write a second line of different text but to the alternative backing screen buffer.");
    written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleW(handle, alternative.data(), gsl::narrow<DWORD>(alternative.size()), &written, nullptr));
    VERIFY_ARE_EQUAL(alternative.size(), written);

    std::unique_ptr<wchar_t[]> primaryBuffer = std::make_unique<wchar_t[]>(primary.size());
    std::unique_ptr<wchar_t[]> alternativeBuffer = std::make_unique<wchar_t[]>(alternative.size());

    Log::Comment(L"Read the first line out of the main/visible screen buffer. It should contain the first thing we wrote.");
    DWORD read = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(out, primaryBuffer.get(), gsl::narrow<DWORD>(primary.size()), { 0, 0 }, &read));
    VERIFY_ARE_EQUAL(primary.size(), read);
    VERIFY_ARE_EQUAL(String(primary.data()), String(primaryBuffer.get(), gsl::narrow<int>(primary.size())));

    Log::Comment(L"Read the second line out of the main/visible screen buffer. It should be full of blanks. The second thing we wrote wasn't to this buffer so it shouldn't show.");
    const std::wstring alternativeExpected(alternative.size(), L'\x20');
    read = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(out, alternativeBuffer.get(), gsl::narrow<DWORD>(alternative.size()), { 0, 1 }, &read));
    VERIFY_ARE_EQUAL(alternative.size(), read);
    VERIFY_ARE_EQUAL(String(alternativeExpected.data()), String(alternativeBuffer.get(), gsl::narrow<int>(alternative.size())));

    Log::Comment(L"Now read the first line from the alternative/non-visible screen buffer. It should contain the second thing we wrote.");
    read = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(handle, alternativeBuffer.get(), gsl::narrow<DWORD>(alternative.size()), { 0, 0 }, &read));
    VERIFY_ARE_EQUAL(alternative.size(), read);
    VERIFY_ARE_EQUAL(String(alternative.data()), String(alternativeBuffer.get(), gsl::narrow<int>(alternative.size())));
}

void BufferTests::ScrollLargeBufferPerformance()
{
    // Cribbed from https://github.com/Microsoft/console/issues/279 issue report.

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsPerfTest", L"true")
    END_TEST_METHOD_PROPERTIES()

    const auto Out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO Info;
    GetConsoleScreenBufferInfo(Out, &Info);

    // We need a large buffer
    Info.dwSize.Y = 9999;
    SetConsoleScreenBufferSize(Out, Info.dwSize);

    SetConsoleCursorPosition(Out, { 0, Info.dwSize.Y - 1 });
    Log::Comment(L"Working. Please wait...");

    const auto count = 20;

    const auto WindowHeight = Info.srWindow.Bottom - Info.srWindow.Top + 1;

    // Set this to false to scroll the entire buffer. The issue will disappear!
    const auto ScrollOnlyInvisibleArea = true;

    const SMALL_RECT Rect{
        0,
        0,
        Info.dwSize.X - 1,
        static_cast<short>(Info.dwSize.Y - (ScrollOnlyInvisibleArea ? WindowHeight : 0) - 1)
    };

    const CHAR_INFO CharInfo{ '^', Info.wAttributes };

    const auto now = std::chrono::steady_clock::now();

    // Scroll the buffer 1 line up several times
    for (int i = 0; i != count; ++i)
    {
        ScrollConsoleScreenBuffer(Out, &Rect, nullptr, { 0, -1 }, &CharInfo);
    }

    const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now).count();

    SetConsoleCursorPosition(Out, { 0, Info.dwSize.Y - 1 });
    Log::Comment(String().Format(L"%d calls took %d ms. Avg %d ms per call", count, delta, delta / count));
}

void BufferTests::ChafaGifPerformance()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsPerfTest", L"true")
    END_TEST_METHOD_PROPERTIES()

    const auto Out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO Info;
    GetConsoleScreenBufferInfo(Out, &Info);

    // We need a large buffer
    Info.dwSize.Y = 9999;
    SetConsoleScreenBufferSize(Out, Info.dwSize);

    SetConsoleCursorPosition(Out, { 0 });

    DWORD Mode = 0;
    GetConsoleMode(Out, &Mode);
    Mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(Out, Mode);

    SetConsoleOutputCP(CP_UTF8);

    // Taken from: https://blog.kowalczyk.info/article/zy/Embedding-binary-resources-on-Windows.html
    HGLOBAL res_handle = NULL;
    HRSRC res;
    char* res_data;
    DWORD res_size;

    // NOTE: providing g_hInstance is important, NULL might not work
    HMODULE hModule = (HMODULE)&__ImageBase;

    res = FindResource(hModule, MAKEINTRESOURCE(CHAFA_CONTENT), RT_RCDATA);
    if (!res)
    {
        VERIFY_FAIL(L"Couldn't find resource.");
        return;
    }
    res_handle = LoadResource(hModule, res);
    if (!res_handle)
    {
        VERIFY_FAIL(L"Couldn't load resource.");
        return;
    }
    res_data = (char*)LockResource(res_handle);
    res_size = SizeofResource(hModule, res);
    /* you can now use the resource data */

    Log::Comment(L"Working. Please wait...");
    const auto now = std::chrono::steady_clock::now();

    DWORD count = 0;
    for (DWORD pos = 0; pos < res_size; pos += 1000)
    {
        DWORD written = 0;
        WriteConsoleA(Out, res_data + pos, min(1000, res_size - pos), &written, nullptr);
        count++;
    }

    const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - now).count();
    Log::Comment(String().Format(L"%d calls took %d ms. Avg %d ms per call", count, delta, delta / count));
}
