// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using WEX::Logging::Log;
using namespace WEX::Common;

HANDLE Common::_hConsole = INVALID_HANDLE_VALUE;
extern wil::unique_process_information pi;

bool IsConsoleStillRunning()
{
    DWORD exitCode = S_OK;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetExitCodeProcess(pi.hProcess, &exitCode));
    return exitCode == STILL_ACTIVE;
}

void VerifySucceededGLE(BOOL bResult)
{
    if (!bResult)
    {
        VERIFY_FAIL(NoThrowString().Format(L"API call failed: 0x%x", GetLastError()));
    }
}

void DoFailure(PCWSTR pwszFunc, DWORD dwErrorCode)
{
    Log::Comment(NoThrowString().Format(L"'%s' call failed with error 0x%x", pwszFunc, dwErrorCode));
    VERIFY_FAIL();
}

void GlePattern(PCWSTR pwszFunc)
{
    DoFailure(pwszFunc, GetLastError());
}

bool CheckLastErrorNegativeOneFail(DWORD dwReturn, PCWSTR pwszFunc)
{
    if (dwReturn == -1)
    {
        GlePattern(pwszFunc);
        return false;
    }
    else
    {
        return true;
    }
}

bool CheckLastErrorZeroFail(int iValue, PCWSTR pwszFunc)
{
    if (iValue == 0)
    {
        GlePattern(pwszFunc);
        return false;
    }
    else
    {
        return true;
    }
}

bool CheckLastErrorWait(DWORD dwReturn, PCWSTR pwszFunc)
{
    if (CheckLastErrorNegativeOneFail(dwReturn, pwszFunc))
    {
        if (dwReturn == STATUS_WAIT_0)
        {
            return true;
        }
        else
        {
            DoFailure(pwszFunc, dwReturn);
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool CheckLastError(HRESULT hr, PCWSTR pwszFunc)
{
    if (!SUCCEEDED(hr))
    {
        DoFailure(pwszFunc, hr);
        return false;
    }
    else
    {
        return true;
    }
}

bool CheckLastError(BOOL fSuccess, PCWSTR pwszFunc)
{
    if (!fSuccess)
    {
        GlePattern(pwszFunc);
        return false;
    }
    else
    {
        return true;
    }
}

bool CheckLastError(HANDLE handle, PCWSTR pwszFunc)
{
    if (handle == INVALID_HANDLE_VALUE)
    {
        GlePattern(pwszFunc);
        return false;
    }
    else
    {
        return true;
    }
}

bool CheckIfFileExists(_In_ PCWSTR pwszPath) noexcept
{
    wil::unique_hfile hFile(CreateFileW(pwszPath,
                                        GENERIC_READ,
                                        0,
                                        nullptr,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr));

    if (hFile.get() != nullptr && hFile.get() != INVALID_HANDLE_VALUE)
    {
        return true;
    }
    else
    {
        return false;
    }
}

BOOL UnadjustWindowRectEx(
    LPRECT prc,
    DWORD dwStyle,
    BOOL fMenu,
    DWORD dwExStyle)
{
    RECT rc;
    SetRectEmpty(&rc);
    BOOL fRc = AdjustWindowRectEx(&rc, dwStyle, fMenu, dwExStyle);
    if (fRc)
    {
        prc->left -= rc.left;
        prc->top -= rc.top;
        prc->right -= rc.right;
        prc->bottom -= rc.bottom;
    }
    return fRc;
}

static HANDLE GetStdHandleVerify(const DWORD dwHandleType)
{
    const HANDLE hConsole = GetStdHandle(dwHandleType);
    VERIFY_ARE_NOT_EQUAL(hConsole, INVALID_HANDLE_VALUE, L"Ensure we got a valid console handle");
    VERIFY_IS_NOT_NULL(hConsole, L"Ensure we got a non-null console handle");

    return hConsole;
}

HANDLE GetStdOutputHandle()
{
    return GetStdHandleVerify(STD_OUTPUT_HANDLE);
}

HANDLE GetStdInputHandle()
{
    return GetStdHandleVerify(STD_INPUT_HANDLE);
}

bool Common::TestBufferSetup()
{
    // Creating a new screen buffer for each test will make sure that it doesn't interact with the output that TAEF is spewing
    // to the default output buffer at the same time.

    _hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                          0 /*dwShareMode*/,
                                          NULL /*lpSecurityAttributes*/,
                                          CONSOLE_TEXTMODE_BUFFER,
                                          NULL /*lpReserved*/);

    VERIFY_ARE_NOT_EQUAL(_hConsole, INVALID_HANDLE_VALUE, L"Creating our test screen buffer.");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleActiveScreenBuffer(_hConsole), L"Applying test screen buffer to console");

    return true;
}

bool Common::TestBufferCleanup()
{
    if (_hConsole != INVALID_HANDLE_VALUE)
    {
        // Simply freeing the handle will restore the next screen buffer down in the stack.
        VERIFY_WIN32_BOOL_SUCCEEDED(CloseHandle(_hConsole), L"Removing our test screen buffer.");
    }

    return true;
}

static PCWSTR pwszConsoleKeyName = L"Console";
static PCWSTR pwszForceV2ValueName = L"ForceV2";

CommonV1V2Helper::CommonV1V2Helper(const ForceV2States ForceV2StateDesired)
{
    // Open console key
    if (!OneCoreDelay::IsIsWindowPresent())
    {
        Log::Comment(L"OneCore based systems don't have v1 state. Skipping.");
        _fRestoreOnExit = false;
        return;
    }

    LSTATUS lstatus = RegOpenKeyExW(HKEY_CURRENT_USER, pwszConsoleKeyName, 0, KEY_READ | KEY_WRITE, &_consoleKey);
    if (ERROR_ACCESS_DENIED == lstatus)
    {
        // UAP and some systems won't let us modify the registry. That's OK. Try to run the tests.
        // Environments where we can't modify the registry should already be set up for the new/v2 console
        // and not need further configuration.
        Log::Comment(L"Skipping backup in environment that cannot access console key.");
        _fRestoreOnExit = false;
        return;
    }

    VERIFY_ARE_EQUAL(ERROR_SUCCESS, lstatus);

    Log::Comment(L"Backing up v1/v2 console state.");
    DWORD cbForceV2Original = sizeof(_dwForceV2Original);

    lstatus = RegQueryValueExW(_consoleKey.get(), pwszForceV2ValueName, nullptr, nullptr, (LPBYTE)&_dwForceV2Original, &cbForceV2Original);
    if (ERROR_FILE_NOT_FOUND == lstatus)
    {
        Log::Comment(L"This machine doesn't have v1/v2 state. Skipping.");
        _consoleKey.reset();
        _fRestoreOnExit = false;
    }
    else
    {
        VERIFY_WIN32_BOOL_FAILED(lstatus, L"Assert querying ForceV2 key was successful.");
        _fRestoreOnExit = true;

        Log::Comment(String().Format(L"Setting v1/v2 console state to desired '%d'", ForceV2StateDesired));
        VERIFY_WIN32_BOOL_FAILED(RegSetValueExW(_consoleKey.get(), pwszForceV2ValueName, 0, REG_DWORD, (LPBYTE)&ForceV2StateDesired, sizeof(ForceV2StateDesired)));
    }
}

CommonV1V2Helper::~CommonV1V2Helper()
{
    if (_fRestoreOnExit)
    {
        Log::Comment(String().Format(L"Restoring v1/v2 console state to original '%d'", _dwForceV2Original));
        VERIFY_WIN32_BOOL_FAILED(RegSetValueExW(_consoleKey.get(), pwszForceV2ValueName, 0, REG_DWORD, (LPBYTE)&_dwForceV2Original, sizeof(_dwForceV2Original)));
    }
}
