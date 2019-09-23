// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

// This class is intended to test:
// GetConsoleCursorInfo
// SetConsoleCursorInfo
// SetConsoleCursorPosition
class CursorTests
{
    BEGIN_TEST_CLASS(CursorTests)
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    BEGIN_TEST_METHOD(TestGetSetConsoleCursorInfo)
        // 0, max, boundaries, and a value in the middle
        TEST_METHOD_PROPERTY(L"Data:dwSize", L"{0, 1, 50, 100, 101, 0xFFFFFFFF}")
        // Both possible values
        TEST_METHOD_PROPERTY(L"Data:bVisible", L"{TRUE, FALSE}")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestSetConsoleCursorPosition)
        TEST_METHOD_PROPERTY(L"HostDestructive", L"True")
    END_TEST_METHOD()
};

bool CursorTests::TestSetup()
{
    return Common::TestBufferSetup();
}

bool CursorTests::TestCleanup()
{
    return Common::TestBufferCleanup();
}

void CursorTests::TestGetSetConsoleCursorInfo()
{
    using namespace WEX::TestExecution;
    DWORD dwSize;
    bool bVisible;

    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"dwSize", dwSize), L"Get size parameter");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"bVisible", bVisible), L"Get visibility parameter");

    // Get initial state of the cursor
    CONSOLE_CURSOR_INFO cciInitial = { 0 };
    BOOL bResult = GetConsoleCursorInfo(Common::_hConsole, &cciInitial);
    VERIFY_WIN32_BOOL_SUCCEEDED(bResult, L"Retrieve initial cursor state.");

    // Fill a structure with the value under test
    CONSOLE_CURSOR_INFO cciTest = { 0 };
    cciTest.bVisible = bVisible;
    cciTest.dwSize = dwSize;

    // If the cursor size is out of range, we expect a failure on set
    BOOL fExpectedResult = TRUE;
    if (cciTest.dwSize < 1 || cciTest.dwSize > 100)
    {
        fExpectedResult = FALSE;
    }

    // Attempt to set and verify that we get the expected result
    bResult = SetConsoleCursorInfo(Common::_hConsole, &cciTest);
    VERIFY_ARE_EQUAL(bResult, fExpectedResult, L"Ensure that return matches success/failure state we were expecting.");

    // Get the state of the cursor again
    CONSOLE_CURSOR_INFO cciReturned = { 0 };
    bResult = GetConsoleCursorInfo(Common::_hConsole, &cciReturned);
    VERIFY_WIN32_BOOL_SUCCEEDED(bResult, L"GET back the cursor information we just set.");

    if (fExpectedResult)
    {
        // If we expected the set to be successful, the returned structure should match the test one
        VERIFY_ARE_EQUAL(cciReturned, cciTest, L"If we expected SET success, the values we set should match what we retrieved.");
    }
    else
    {
        // If we expected the set to fail, the returned structure should match the initial one
        VERIFY_ARE_EQUAL(cciReturned, cciInitial, L"If we expected SET failure, the initial values before the SET should match what we retrieved.");
    }
}

void TestSetConsoleCursorPositionImpl(WORD wCursorX, WORD wCursorY, BOOL bExpectedResult)
{
    COORD coordCursor;
    coordCursor.X = wCursorX;
    coordCursor.Y = wCursorY;

    // Get initial position data
    CONSOLE_SCREEN_BUFFER_INFOEX sbiInitial = { 0 };
    sbiInitial.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    BOOL bResult = GetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiInitial);
    VERIFY_WIN32_BOOL_SUCCEEDED(bResult, L"Get the initial buffer data.");

    // Attempt to set cursor into valid area
    bResult = SetConsoleCursorPosition(Common::_hConsole, coordCursor);
    VERIFY_ARE_EQUAL(bResult, bExpectedResult, L"Ensure that return from SET matches success/failure state we were expecting.");

    CONSOLE_SCREEN_BUFFER_INFOEX sbiTest = { 0 };
    sbiTest.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bResult = GetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiTest);
    VERIFY_WIN32_BOOL_SUCCEEDED(bResult, L"GET the values back to ensure they were set properly.");

    // Cursor is where it was set to if we were supposed to be successful
    if (bExpectedResult)
    {
        VERIFY_ARE_EQUAL(coordCursor, sbiTest.dwCursorPosition, L"If SET was TRUE, we expect the cursor to be where we SET it.");
    }
    else
    {
        // otherwise, it's at where it was before
        VERIFY_ARE_EQUAL(sbiInitial.dwCursorPosition, sbiTest.dwCursorPosition, L"If SET was FALSE, we expect the cursor to not have moved.");
    }

    // Verify the viewport.
    bool fViewportMoveExpected = false;

    // If we expected the cursor to be set successfully, the viewport might have moved.
    if (bExpectedResult)
    {
        // If the position we set was outside the initial rectangle, then the viewport should have moved.
        if (coordCursor.X > sbiInitial.srWindow.Right ||
            coordCursor.X < sbiInitial.srWindow.Left ||
            coordCursor.Y > sbiInitial.srWindow.Bottom ||
            coordCursor.Y < sbiInitial.srWindow.Top)
        {
            fViewportMoveExpected = true;
        }
    }

    if (fViewportMoveExpected)
    {
        // Something had to have changed in the viewport
        VERIFY_ARE_NOT_EQUAL(sbiInitial.srWindow, sbiTest.srWindow, L"The viewports must have changed if we set the cursor outside the current area.");
    }
    else
    {
        VERIFY_ARE_EQUAL(sbiInitial.srWindow, sbiTest.srWindow, L"The viewports must remain the same if the cursor was set inside the existing one.");
    }
}

void CursorTests::TestSetConsoleCursorPosition()
{
    // Get initial buffer value for boundaries
    CONSOLE_SCREEN_BUFFER_INFOEX sbiInitial = { 0 };
    sbiInitial.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    BOOL bResult = GetConsoleScreenBufferInfoEx(Common::_hConsole, &sbiInitial);
    VERIFY_WIN32_BOOL_SUCCEEDED(bResult, L"Retrieve the initial buffer information to calculate the boundaries for testing.");

    // Try several cases
    TestSetConsoleCursorPositionImpl(0, 0, TRUE); // Top left corner of buffer
    TestSetConsoleCursorPositionImpl(sbiInitial.dwSize.X - 1, sbiInitial.dwSize.Y - 1, TRUE); // Bottom right corner of buffer
    TestSetConsoleCursorPositionImpl(sbiInitial.dwSize.X, sbiInitial.dwSize.Y, FALSE); // 1 beyond bottom right corner (the size is 1 larger than the array indicies)
    TestSetConsoleCursorPositionImpl(MAXWORD, MAXWORD, FALSE); // Max values
}
