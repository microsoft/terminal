// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/conint.h"

using namespace Microsoft::Console::Internal;

[[nodiscard]] HRESULT ProcessPolicy::CheckAppModelPolicy(const HANDLE /*hToken*/,
                                                         bool& fIsWrongWayBlocked) noexcept
{
    fIsWrongWayBlocked = false;
    return S_OK;
}

[[nodiscard]] HRESULT ProcessPolicy::CheckIntegrityLevelPolicy(const HANDLE /*hOtherToken*/,
                                                               bool& fIsWrongWayBlocked) noexcept
{
    fIsWrongWayBlocked = false;
    return S_OK;
}

void EdpPolicy::AuditClipboard(const std::wstring_view /*destinationName*/) noexcept
{
}

[[nodiscard]] HRESULT Theming::TrySetDarkMode(HWND /*hwnd*/) noexcept
{
    return S_FALSE;
}

[[nodiscard]] HRESULT DefaultApp::CheckDefaultAppPolicy(bool& isEnabled) noexcept
{
    // True so propsheet will show configuration options but be sure that
    // the open one won't attempt handoff from double click of OpenConsole.exe
    isEnabled = true;
    return S_OK;
}

[[nodiscard]] HRESULT DefaultApp::CheckShouldTerminalBeDefault(bool& isEnabled) noexcept
{
    // Since we toggled this feature on in Windows, Terminal (and Terminal Preview) need to
    // agree -- otherwise, they will present UI that suggests Terminal is NOT the default,
    // like the info bar.
    OSVERSIONINFOEXW osver{};
    osver.dwOSVersionInfoSize = sizeof(osver);
    osver.dwBuildNumber = 22544;

    DWORDLONG dwlConditionMask = 0;
    VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

    isEnabled = VerifyVersionInfoW(&osver, VER_BUILDNUMBER, dwlConditionMask) != FALSE;
    return S_OK;
}
