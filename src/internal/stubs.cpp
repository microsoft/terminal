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
