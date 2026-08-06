// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleShimPolicy.h"

// Routine Description:
// - Constructs a new instance of the shim policy class.
// Arguments:
// - All arguments specify a true/false status to a policy that could be applied to a console client app.
ConsoleShimPolicy::ConsoleShimPolicy(const HANDLE hProcess)
{
    // If we cannot determine the exe name, then we're probably not cmd or powershell.
    try
    {
        const std::filesystem::path processName = wil::GetModuleFileNameExW<std::wstring>(hProcess, nullptr);
        const auto clientName = processName.filename().native();

        _isCmd = til::equals_insensitive_ascii(clientName, L"cmd.exe");

        // For powershell, we need both Windows Powershell (powershell.exe) and
        // Powershell Core (pwsh.exe). If Powershell Core is ever updated to use
        // ^[[3J for Clear-Host, then it won't ever hit the shim code path, but
        // we're keeping this for the long tail of pwsh versions that still
        // _don't_ use that sequence.
        const auto isInboxPowershell = til::equals_insensitive_ascii(clientName, L"powershell.exe");
        const auto isPwsh = til::equals_insensitive_ascii(clientName, L"pwsh.exe");
        _isPowershell = isInboxPowershell || isPwsh;
    }
    CATCH_LOG();
}

// Method Description:
// - Returns true if the connected client application is literally "cmd.exe". If
//   it is, we'll need to enable certain compatibility shims.
// Arguments:
// - <none>
// Return Value:
// - rue iff the process is "cmd.exe"
bool ConsoleShimPolicy::IsCmdExe() const noexcept
{
    return _isCmd;
}

// Method Description:
// - Returns true if the connected client application is literally
//   "powershell.exe" or "pwsh.exe". If it is, we'll need to enable certain
//   compatibility shims.
// Arguments:
// - <none>
// Return Value:
// - rue iff the process is "powershell.exe" or "pwsh.exe"
bool ConsoleShimPolicy::IsPowershellExe() const noexcept
{
    return _isPowershell;
}
