// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <wil/result.h>
#include <wil/tokenhelpers.h>
#include "util.h"
#include <cstdio>
#include <fstream>
#include <memory>

PCWSTR GetIntegrityLevel()
{
    DWORD dwIntegrityLevel = 0;

    // Get the Integrity level.
    wistd::unique_ptr<TOKEN_MANDATORY_LABEL> tokenLabel;
    THROW_IF_FAILED(wil::GetTokenInformationNoThrow(tokenLabel, GetCurrentProcessToken()));

    dwIntegrityLevel = *GetSidSubAuthority(tokenLabel->Label.Sid,
                                           (DWORD)(UCHAR)(*GetSidSubAuthorityCount(tokenLabel->Label.Sid) - 1));

    switch (dwIntegrityLevel)
    {
    case SECURITY_MANDATORY_LOW_RID:
        return L"Low Integrity\r\n";
    case SECURITY_MANDATORY_MEDIUM_RID:
        return L"Medium Integrity\r\n";
    case SECURITY_MANDATORY_HIGH_RID:
        return L"High Integrity\r\n";
    case SECURITY_MANDATORY_SYSTEM_RID:
        return L"System Integrity\r\n";
    default:
        return L"UNKNOWN INTEGRITY\r\n";
    }
}

void WriteToConsole(_In_ PCWSTR pwszText)
{
    DWORD dwWritten = 0;
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
                  pwszText,
                  (DWORD)wcslen(pwszText),
                  &dwWritten,
                  nullptr);
}

void FormatToConsole(_In_ PCWSTR pwszFunc, const BOOL bResult, const DWORD dwError)
{
    std::unique_ptr<wchar_t[]> pwszBuffer = std::make_unique<wchar_t[]>(MAX_PATH);
    THROW_IF_FAILED(StringCchPrintfW(pwszBuffer.get(), MAX_PATH, L"%s;%d;%d\r\n", pwszFunc, bResult, dwError));
    WriteToConsole(pwszBuffer.get());
}

PCWSTR TryReadConsoleOutputW(_Out_ BOOL* const pbResult,
                             _Out_ DWORD* const pdwError)
{
    DWORD cCharInfos = 1;
    std::unique_ptr<CHAR_INFO[]> rgCharInfos = std::make_unique<CHAR_INFO[]>(cCharInfos);
    COORD coordBuffer;
    coordBuffer.X = 1;
    coordBuffer.Y = 1;

    COORD coordRead = { 0 };
    SMALL_RECT srReadRegion = { 0 };

    SetLastError(0);
    *pbResult = ReadConsoleOutputW(GetStdHandle(STD_OUTPUT_HANDLE),
                                   rgCharInfos.get(),
                                   coordBuffer,
                                   coordRead,
                                   &srReadRegion);
    *pdwError = GetLastError();
    return L"RCOW";
}

PCWSTR TryReadConsoleOutputA(_Out_ BOOL* const pbResult,
                             _Out_ DWORD* const pdwError)
{
    DWORD cCharInfos = 1;
    std::unique_ptr<CHAR_INFO[]> rgCharInfos = std::make_unique<CHAR_INFO[]>(cCharInfos);
    COORD coordBuffer;
    coordBuffer.X = 1;
    coordBuffer.Y = 1;

    COORD coordRead = { 0 };
    SMALL_RECT srReadRegion = { 0 };

    SetLastError(0);
    *pbResult = ReadConsoleOutputA(GetStdHandle(STD_OUTPUT_HANDLE),
                                   rgCharInfos.get(),
                                   coordBuffer,
                                   coordRead,
                                   &srReadRegion);
    *pdwError = GetLastError();
    return L"RCOA";
}

PCWSTR TryReadConsoleOutputCharacterW(_Out_ BOOL* const pbResult,
                                      _Out_ DWORD* const pdwError)
{
    DWORD cchTest = 1;
    std::unique_ptr<wchar_t[]> pwszTest = std::make_unique<wchar_t[]>(cchTest);
    COORD coordRead = { 0 };
    DWORD dwRead = 0;

    SetLastError(0);
    *pbResult = ReadConsoleOutputCharacterW(GetStdHandle(STD_OUTPUT_HANDLE),
                                            pwszTest.get(),
                                            cchTest,
                                            coordRead,
                                            &dwRead);
    *pdwError = GetLastError();
    return L"RCOCW";
}

PCWSTR TryReadConsoleOutputCharacterA(_Out_ BOOL* const pbResult,
                                      _Out_ DWORD* const pdwError)
{
    DWORD cchTest = 1;
    std::unique_ptr<char[]> pszTest = std::make_unique<char[]>(cchTest);
    COORD coordRead = { 0 };
    DWORD dwRead = 0;

    SetLastError(0);
    *pbResult = ReadConsoleOutputCharacterA(GetStdHandle(STD_OUTPUT_HANDLE),
                                            pszTest.get(),
                                            cchTest,
                                            coordRead,
                                            &dwRead);
    *pdwError = GetLastError();
    return L"RCOCA";
}

PCWSTR TryReadConsoleOutputAttribute(_Out_ BOOL* const pbResult,
                                     _Out_ DWORD* const pdwError)
{
    DWORD cchTest = 1;
    std::unique_ptr<WORD[]> rgwTest = std::make_unique<WORD[]>(cchTest);
    COORD coordRead = { 0 };
    DWORD dwRead = 0;

    SetLastError(0);
    *pbResult = ReadConsoleOutputAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                                           rgwTest.get(),
                                           cchTest,
                                           coordRead,
                                           &dwRead);
    *pdwError = GetLastError();
    return L"RCOAttr";
}

PCWSTR TryWriteConsoleInputW(_Out_ BOOL* const pbResult,
                             _Out_ DWORD* const pdwError)
{
    DWORD cInputRecords = 1;
    std::unique_ptr<INPUT_RECORD[]> rgInputRecords = std::make_unique<INPUT_RECORD[]>(cInputRecords);
    rgInputRecords[0].EventType = KEY_EVENT;
    rgInputRecords[0].Event.KeyEvent.bKeyDown = TRUE;
    rgInputRecords[0].Event.KeyEvent.dwControlKeyState = 0;
    rgInputRecords[0].Event.KeyEvent.uChar.UnicodeChar = L'A';
    rgInputRecords[0].Event.KeyEvent.wRepeatCount = 1;
    rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode = L'A';
    rgInputRecords[0].Event.KeyEvent.wVirtualScanCode = L'A';

    DWORD dwWritten = 0;

    SetLastError(0);
    *pbResult = WriteConsoleInputW(GetStdHandle(STD_INPUT_HANDLE),
                                   rgInputRecords.get(),
                                   cInputRecords,
                                   &dwWritten);
    *pdwError = GetLastError();
    return L"WCIW";
}

PCWSTR TryWriteConsoleInputA(_Out_ BOOL* const pbResult,
                             _Out_ DWORD* const pdwError)
{
    DWORD cInputRecords = 1;
    std::unique_ptr<INPUT_RECORD[]> rgInputRecords = std::make_unique<INPUT_RECORD[]>(cInputRecords);
    rgInputRecords[0].EventType = KEY_EVENT;
    rgInputRecords[0].Event.KeyEvent.bKeyDown = TRUE;
    rgInputRecords[0].Event.KeyEvent.dwControlKeyState = 0;
    rgInputRecords[0].Event.KeyEvent.uChar.AsciiChar = 'A';
    rgInputRecords[0].Event.KeyEvent.wRepeatCount = 1;
    rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode = L'A';
    rgInputRecords[0].Event.KeyEvent.wVirtualScanCode = L'A';

    DWORD dwWritten = 0;

    SetLastError(0);
    *pbResult = WriteConsoleInputA(GetStdHandle(STD_INPUT_HANDLE),
                                   rgInputRecords.get(),
                                   cInputRecords,
                                   &dwWritten);
    *pdwError = GetLastError();
    return L"WCIA";
}

bool TestLibFunc()
{
    WriteToConsole(GetIntegrityLevel());

    PCWSTR pwszFuncName;
    BOOL bResult;
    DWORD dwError;

    pwszFuncName = TryReadConsoleOutputW(&bResult, &dwError);
    FormatToConsole(pwszFuncName, bResult, dwError);

    pwszFuncName = TryReadConsoleOutputA(&bResult, &dwError);
    FormatToConsole(pwszFuncName, bResult, dwError);

    pwszFuncName = TryReadConsoleOutputCharacterW(&bResult, &dwError);
    FormatToConsole(pwszFuncName, bResult, dwError);

    pwszFuncName = TryReadConsoleOutputCharacterA(&bResult, &dwError);
    FormatToConsole(pwszFuncName, bResult, dwError);

    pwszFuncName = TryReadConsoleOutputAttribute(&bResult, &dwError);
    FormatToConsole(pwszFuncName, bResult, dwError);

    pwszFuncName = TryWriteConsoleInputA(&bResult, &dwError);
    FormatToConsole(pwszFuncName, bResult, dwError);

    pwszFuncName = TryWriteConsoleInputW(&bResult, &dwError);
    FormatToConsole(pwszFuncName, bResult, dwError);

    return true;
}
