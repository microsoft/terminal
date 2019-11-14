// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using WEX::Logging::Log;

// This class is intended to test restrictions placed on APIs from within a UWP application context
class PolicyTests
{
    BEGIN_TEST_CLASS(PolicyTests)
    END_TEST_CLASS()

// UAP test type doesn't work quite right in VSO, skip. We'll get it in the RI-TP internally.
#ifdef __INSIDE_WINDOWS
    BEGIN_TEST_METHOD(WrongWayVerbsUAP)
        TEST_METHOD_PROPERTY(L"RunAs", L"UAP")
        TEST_METHOD_PROPERTY(L"UAP:AppxManifest", L"MUA")
    END_TEST_METHOD();
#endif

    BEGIN_TEST_METHOD(WrongWayVerbsUser)
        TEST_METHOD_PROPERTY(L"RunAs", L"User")
    END_TEST_METHOD();
};

void DoWrongWayVerbTest(_In_ BOOL bResultExpected, _In_ DWORD dwStatusExpected)
{
    DWORD dwResult;
    BOOL bResultActual;

    // Try to read the output buffer
    {
        Log::Comment(L"Read the output buffer using string commands.");
        {
            wchar_t pwsz[50];
            char psz[50];
            COORD coord = { 0 };

            SetLastError(0);
            bResultActual = ReadConsoleOutputCharacterW(GetStdOutputHandle(), pwsz, ARRAYSIZE(pwsz), coord, &dwResult);
            VERIFY_ARE_EQUAL(bResultExpected, bResultActual);
            VERIFY_ARE_EQUAL(dwStatusExpected, GetLastError());

            SetLastError(0);
            bResultActual = ReadConsoleOutputCharacterA(GetStdOutputHandle(), psz, ARRAYSIZE(psz), coord, &dwResult);
            VERIFY_ARE_EQUAL(bResultExpected, bResultActual);
            VERIFY_ARE_EQUAL(dwStatusExpected, GetLastError());

            WORD attrs[50];
            SetLastError(0);
            bResultActual = ReadConsoleOutputAttribute(GetStdOutputHandle(), static_cast<WORD*>(attrs), ARRAYSIZE(attrs), coord, &dwResult);
            VERIFY_ARE_EQUAL(bResultExpected, bResultActual);
            VERIFY_ARE_EQUAL(dwStatusExpected, GetLastError());
        }

        Log::Comment(L"Read the output buffer using CHAR_INFO commands.");
        {
            CHAR_INFO pci[50];
            COORD coordPos = { 0 };
            COORD coordPci;
            coordPci.X = 50;
            coordPci.Y = 1;
            SMALL_RECT srPci;
            srPci.Top = 1;
            srPci.Bottom = 1;
            srPci.Left = 1;
            srPci.Right = 50;

            SetLastError(0);
            bResultActual = ReadConsoleOutputW(GetStdOutputHandle(), pci, coordPci, coordPos, &srPci);
            VERIFY_ARE_EQUAL(bResultExpected, bResultActual);
            VERIFY_ARE_EQUAL(dwStatusExpected, GetLastError());

            SetLastError(0);
            bResultActual = ReadConsoleOutputA(GetStdOutputHandle(), pci, coordPci, coordPos, &srPci);
            VERIFY_ARE_EQUAL(bResultExpected, bResultActual);
            VERIFY_ARE_EQUAL(dwStatusExpected, GetLastError());
        }
    }

    // Try to write the input buffer
    Log::Comment(L"Write the input buffer using INPUT_RECORD commands.");
    {
        INPUT_RECORD ir[2];
        ir[0].EventType = KEY_EVENT;
        ir[0].Event.KeyEvent.bKeyDown = TRUE;
        ir[0].Event.KeyEvent.dwControlKeyState = 0;
        ir[0].Event.KeyEvent.uChar.UnicodeChar = L'@';
        ir[0].Event.KeyEvent.wRepeatCount = 1;
        ir[0].Event.KeyEvent.wVirtualKeyCode = ir[0].Event.KeyEvent.uChar.UnicodeChar;
        ir[0].Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(ir[0].Event.KeyEvent.wVirtualKeyCode, MAPVK_VK_TO_VSC));
        ir[1] = ir[0];
        ir[1].Event.KeyEvent.bKeyDown = FALSE;

        SetLastError(0);
        bResultActual = WriteConsoleInputW(GetStdInputHandle(), ir, ARRAYSIZE(ir), &dwResult);
        VERIFY_ARE_EQUAL(bResultExpected, bResultActual);
        VERIFY_ARE_EQUAL(dwStatusExpected, GetLastError());

        SetLastError(0);
        bResultActual = WriteConsoleInputA(GetStdInputHandle(), ir, ARRAYSIZE(ir), &dwResult);
        VERIFY_ARE_EQUAL(bResultExpected, bResultActual);
        VERIFY_ARE_EQUAL(dwStatusExpected, GetLastError());
    }
}

#ifdef __INSIDE_WINDOWS
void PolicyTests::WrongWayVerbsUAP()
{
    Log::Comment(L"From the UAP environment, these functions should be access denied.");
    DoWrongWayVerbTest(FALSE, ERROR_ACCESS_DENIED);
}
#endif

void PolicyTests::WrongWayVerbsUser()
{
    Log::Comment(L"From the classic uer environment, these functions should return with a normal status code.");
    DoWrongWayVerbTest(TRUE, ERROR_SUCCESS);
}
