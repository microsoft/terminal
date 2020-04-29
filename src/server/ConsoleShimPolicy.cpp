// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleShimPolicy.h"

// Routine Description:
// - Constructs a new instance of the process policy class.
// Arguments:
// - All arguments specify a true/false status to a policy that could be applied to a console client app.
ConsoleShimPolicy::ConsoleShimPolicy(const bool isCmd,
                                     const bool isPowershell) :
    _isCmd{ isCmd },
    _isPowershell{ isPowershell }
{
}

// Routine Description:
// - Destructs an instance of the shim policy class.
ConsoleShimPolicy::~ConsoleShimPolicy()
{
}

// Routine Description:
// - Opens the process token for the given handle and resolves the process name.
//   We'll initialize the new ConsoleShimPolicy based on whether the client
//   process is "cmd.exe" or "powershell.exe".
// - For more info, see GH#3126
// Arguments:
// - hProcess - Handle to a connected process
// Return Value:
// - ConsoleShimPolicy object containing resolved shim policy data.
ConsoleShimPolicy ConsoleShimPolicy::s_CreateInstance(const HANDLE hProcess)
{
    // If we cannot determine the exe name, then we're probably not cmd or powershell.
    bool isCmd = false;
    bool isPowershell = false;

    try
    {
        const std::filesystem::path processName = wil::GetModuleFileNameExW<std::wstring>(hProcess, nullptr);
        auto clientName = processName.filename().wstring();
        // For whatever reason, wil::GetModuleFileNameExW leaves trailing nulls, so get rid of them.
        clientName.erase(std::find(clientName.begin(), clientName.end(), '\0'), clientName.end());

        isCmd = clientName.compare(L"cmd.exe") == 0;
        isPowershell = clientName.compare(L"powershell.exe") == 0;
    }
    CATCH_LOG();

    return ConsoleShimPolicy(isCmd, isPowershell);
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
// - Returns true if the connected client application is literally "powershell.exe". If
//   it is, we'll need to enable certain compatibility shims.
// Arguments:
// - <none>
// Return Value:
// - rue iff the process is "powershell.exe"
bool ConsoleShimPolicy::IsPowershellExe() const noexcept
{
    return _isPowershell;
}
