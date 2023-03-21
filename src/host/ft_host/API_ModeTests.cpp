// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::Logging;

// This class is intended to test:
// GetConsoleMode
// SetConsoleMode
class ModeTests
{
    BEGIN_TEST_CLASS(ModeTests)
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestGetConsoleModeInvalid);
    TEST_METHOD(TestSetConsoleModeInvalid);

    TEST_METHOD(TestConsoleModeInputScenario);
    TEST_METHOD(TestConsoleModeScreenBufferScenario);
    TEST_METHOD(TestConsoleModeAcrossMultipleBuffers);

    TEST_METHOD(TestGetConsoleDisplayMode);

    TEST_METHOD(TestGetConsoleProcessList);
};

bool ModeTests::TestSetup()
{
    return Common::TestBufferSetup();
}

bool ModeTests::TestCleanup()
{
    return Common::TestBufferCleanup();
}

void ModeTests::TestGetConsoleModeInvalid()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsPerfTest", L"true")
    END_TEST_METHOD_PROPERTIES()

    auto dwConsoleMode = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(GetConsoleMode(INVALID_HANDLE_VALUE, &dwConsoleMode));
    VERIFY_ARE_EQUAL(dwConsoleMode, (DWORD)-1);

    dwConsoleMode = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(GetConsoleMode(nullptr, &dwConsoleMode));
    VERIFY_ARE_EQUAL(dwConsoleMode, (DWORD)-1);
}

void ModeTests::TestSetConsoleModeInvalid()
{
    VERIFY_WIN32_BOOL_FAILED(SetConsoleMode(INVALID_HANDLE_VALUE, 0));
    VERIFY_WIN32_BOOL_FAILED(SetConsoleMode(nullptr, 0));

    auto hConsoleInput = GetStdInputHandle();
    VERIFY_WIN32_BOOL_FAILED(SetConsoleMode(hConsoleInput, 0xFFFFFFFF), L"Can't set invalid input flags");
    VERIFY_WIN32_BOOL_FAILED(SetConsoleMode(hConsoleInput, ENABLE_ECHO_INPUT),
                             L"Can't set ENABLE_ECHO_INPUT without ENABLE_LINE_INPUT on input handle");

    VERIFY_WIN32_BOOL_FAILED(SetConsoleMode(Common::_hConsole, 0xFFFFFFFF), L"Can't set invalid output flags");
}

void ModeTests::TestConsoleModeInputScenario()
{
    auto hConsoleInput = GetStdInputHandle();

    const DWORD dwInputModeToSet = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_WINDOW_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hConsoleInput, dwInputModeToSet), L"Set valid flags for input");

    auto dwInputMode = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(hConsoleInput, &dwInputMode), L"Get recently set flags for input");
    VERIFY_ARE_EQUAL(dwInputMode, dwInputModeToSet, L"Make sure SetConsoleMode worked for input");
}

void ModeTests::TestConsoleModeScreenBufferScenario()
{
    const DWORD dwOutputModeToSet = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(Common::_hConsole, dwOutputModeToSet),
                                L"Set initial output flags");

    auto dwOutputMode = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(Common::_hConsole, &dwOutputMode), L"Get new output flags");
    VERIFY_ARE_EQUAL(dwOutputMode, dwOutputModeToSet, L"Make sure output flags applied appropriately");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(Common::_hConsole, 0), L"Set zero output flags");

    dwOutputMode = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(Common::_hConsole, &dwOutputMode), L"Get zero output flags");
    VERIFY_ARE_EQUAL(dwOutputMode, (DWORD)0, L"Verify able to set zero output flags");
}

void ModeTests::TestConsoleModeAcrossMultipleBuffers()
{
    auto dwInitialMode = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(Common::_hConsole, &dwInitialMode),
                                L"Get initial output flags");

    Log::Comment(L"Verify initial flags match the expected defaults");
    VERIFY_IS_TRUE(WI_IsFlagSet(dwInitialMode, ENABLE_PROCESSED_OUTPUT));
    VERIFY_IS_TRUE(WI_IsFlagSet(dwInitialMode, ENABLE_WRAP_AT_EOL_OUTPUT));
    VERIFY_IS_TRUE(WI_IsFlagClear(dwInitialMode, DISABLE_NEWLINE_AUTO_RETURN));
    VERIFY_IS_TRUE(WI_IsFlagClear(dwInitialMode, ENABLE_LVB_GRID_WORLDWIDE));

    // The initial VT flag may vary, dependent on the VirtualTerminalLevel registry entry.
    const auto defaultVtFlag = WI_IsFlagSet(dwInitialMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    auto dwUpdatedMode = dwInitialMode;
    WI_ClearFlag(dwUpdatedMode, ENABLE_PROCESSED_OUTPUT);
    WI_ClearFlag(dwUpdatedMode, ENABLE_WRAP_AT_EOL_OUTPUT);
    WI_SetFlag(dwUpdatedMode, DISABLE_NEWLINE_AUTO_RETURN);
    WI_SetFlag(dwUpdatedMode, ENABLE_LVB_GRID_WORLDWIDE);
    WI_UpdateFlag(dwUpdatedMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING, !defaultVtFlag);
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(Common::_hConsole, dwUpdatedMode),
                                L"Update flags to the opposite of their initial values");

    auto hSecondBuffer = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                                   0 /*dwShareMode*/,
                                                   nullptr /*lpSecurityAttributes*/,
                                                   CONSOLE_TEXTMODE_BUFFER,
                                                   nullptr /*lpReserved*/);
    VERIFY_ARE_NOT_EQUAL(INVALID_HANDLE_VALUE, hSecondBuffer, L"Create a second screen buffer");

    auto dwSecondBufferMode = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(hSecondBuffer, &dwSecondBufferMode),
                                L"Get initial flags for second buffer");

    VERIFY_ARE_EQUAL(dwInitialMode, dwSecondBufferMode, L"Verify second buffer initialized with defaults");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hSecondBuffer, dwSecondBufferMode),
                                L"Reapply mode to test if it affects the main buffer");

    VERIFY_WIN32_BOOL_SUCCEEDED(CloseHandle(hSecondBuffer), L"Close the second buffer");

    auto dwFinalMode = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(Common::_hConsole, &dwFinalMode),
                                L"Get flags from the main buffer again");

    VERIFY_ARE_EQUAL(dwUpdatedMode, dwFinalMode, L"Verify main buffer flags haven't changed");
}

void ModeTests::TestGetConsoleDisplayMode()
{
    DWORD dwMode = 0;
    SetLastError(0);

    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleDisplayMode(&dwMode));
    VERIFY_ARE_EQUAL(0u, GetLastError());
}

void ModeTests::TestGetConsoleProcessList()
{
    // Set last error to 0.
    Log::Comment(L"Test null and 0");
    {
        SetLastError(0);
        VERIFY_ARE_EQUAL(0ul, GetConsoleProcessList(nullptr, 0), L"Return value should be 0");
        VERIFY_ARE_EQUAL(static_cast<DWORD>(ERROR_INVALID_PARAMETER), GetLastError(), L"Last error should be invalid parameter.");
    }

    Log::Comment(L"Test null and a valid length");
    {
        SetLastError(0);
        VERIFY_ARE_EQUAL(0ul, GetConsoleProcessList(nullptr, 10), L"Return value should be 0");
        VERIFY_ARE_EQUAL(static_cast<DWORD>(ERROR_INVALID_PARAMETER), GetLastError(), L"Last error should be invalid parameter.");
    }

    Log::Comment(L"Test valid buffer and a zero length");
    {
        DWORD one = 0;
        const auto oneOriginal = one;
        SetLastError(0);
        VERIFY_ARE_EQUAL(0ul, GetConsoleProcessList(&one, 0), L"Return value should be 0");
        VERIFY_ARE_EQUAL(static_cast<DWORD>(ERROR_INVALID_PARAMETER), GetLastError(), L"Last error should be invalid parameter.");
        VERIFY_ARE_EQUAL(oneOriginal, one, L"Buffer should not have been touched.");
    }

    Log::Comment(L"Test a valid buffer of length 1");
    {
        DWORD one = 0;
        const auto oneOriginal = one;
        SetLastError(0);
        VERIFY_ARE_EQUAL(2ul, GetConsoleProcessList(&one, 1), L"Return value should be 2 because there are at least two processes attached during tests.");
        VERIFY_ARE_EQUAL(static_cast<DWORD>(ERROR_SUCCESS), GetLastError(), L"Last error should be success.");
        VERIFY_ARE_EQUAL(oneOriginal, one, L"Buffer should not have been touched.");
    }

    Log::Comment(L"Test a valid buffer of length 2");
    {
        DWORD two[2] = { 0 };

        SetLastError(0);
        VERIFY_ARE_EQUAL(2ul, GetConsoleProcessList(two, 2), L"Return value should be 2 because there are at least two processes attached during tests.");
        VERIFY_ARE_EQUAL(static_cast<DWORD>(ERROR_SUCCESS), GetLastError(), L"Last error should be success.");
        VERIFY_ARE_NOT_EQUAL(0ul, two[0], L"Slot 0 was filled");
        VERIFY_ARE_NOT_EQUAL(0ul, two[1], L"Slot 1 was filled.");
    }

    Log::Comment(L"Test a valid buffer of length 5");
    {
        DWORD five[5] = { 0 };

        SetLastError(0);
        VERIFY_ARE_EQUAL(2ul, GetConsoleProcessList(five, 2), L"Return value should be 2 because there are at least two processes attached during tests.");
        VERIFY_ARE_EQUAL(static_cast<DWORD>(ERROR_SUCCESS), GetLastError(), L"Last error should be success.");
        VERIFY_ARE_NOT_EQUAL(0ul, five[0], L"Slot 0 was filled");
        VERIFY_ARE_NOT_EQUAL(0ul, five[1], L"Slot 1 was filled.");
        VERIFY_ARE_EQUAL(0ul, five[2], L"Slot 2 is still empty.");
        VERIFY_ARE_EQUAL(0ul, five[3], L"Slot 3 is still empty.");
        VERIFY_ARE_EQUAL(0ul, five[4], L"Slot 4 is still empty.");
    }
}
