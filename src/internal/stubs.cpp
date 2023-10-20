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

[[nodiscard]] bool DefaultApp::CheckDefaultAppPolicy() noexcept
{
    // True so propsheet will show configuration options but be sure that
    // the open one won't attempt handoff from double click of OpenConsole.exe
    return true;
}

[[nodiscard]] bool DefaultApp::CheckShouldTerminalBeDefault() noexcept
{
    // False since setting Terminal as the default app is an OS feature and probably
    // should not be done in the open source conhost. We can always decide to turn it
    // on in the future though.
    return false;
}
