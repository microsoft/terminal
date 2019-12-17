/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Common.hpp

Abstract:
- This module contains common items for the API tests

Author:
- Michael Niksa (MiNiksa)          2015
- Paul Campbell (PaulCam)          2015

Revision History:
--*/

#pragma once

#include "..\..\inc\consoletaeftemplates.hpp"

class Common
{
public:
    static bool TestBufferSetup();
    static bool TestBufferCleanup();
    static HANDLE _hConsole;
};

class CommonV1V2Helper
{
public:
    enum class ForceV2States : DWORD
    {
        V1 = 0,
        V2 = 1
    };

    CommonV1V2Helper(const ForceV2States ForceV2StateDesired);
    ~CommonV1V2Helper();

private:
    bool _fRestoreOnExit = false;
    DWORD _dwForceV2Original = 0;
    wil::unique_hkey _consoleKey;
};

// Helper to cause a VERIFY_FAIL and get the last error code for functions that return null-like things.
void VerifySucceededGLE(BOOL bResult);

void DoFailure(PCWSTR pwszFunc, DWORD dwErrorCode);
void GlePattern(PCWSTR pwszFunc);
bool CheckLastErrorNegativeOneFail(DWORD dwReturn, PCWSTR pwszFunc);
bool CheckLastErrorZeroFail(int iValue, PCWSTR pwszFunc);
bool CheckLastErrorWait(DWORD dwReturn, PCWSTR pwszFunc);
bool CheckLastError(HRESULT hr, PCWSTR pwszFunc);
bool CheckLastError(BOOL fSuccess, PCWSTR pwszFunc);
bool CheckLastError(HANDLE handle, PCWSTR pwszFunc);

[[nodiscard]] bool CheckIfFileExists(_In_ PCWSTR pwszPath) noexcept;

//http://blogs.msdn.com/b/oldnewthing/archive/2013/10/17/10457292.aspx
BOOL UnadjustWindowRectEx(
    LPRECT prc,
    DWORD dwStyle,
    BOOL fMenu,
    DWORD dwExStyle);

HANDLE GetStdInputHandle();
HANDLE GetStdOutputHandle();

bool IsConsoleStillRunning();
