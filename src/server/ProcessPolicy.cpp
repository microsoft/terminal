// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ProcessPolicy.h"

#include "..\inc\conint.h"

// Routine Description:
// - Constructs a new instance of the process policy class.
// Arguments:
// - All arguments specify a true/false status to a policy that could be applied to a console client app.
ConsoleProcessPolicy::ConsoleProcessPolicy(const bool fCanReadOutputBuffer,
                                           const bool fCanWriteInputBuffer) :
    _fCanReadOutputBuffer(fCanReadOutputBuffer),
    _fCanWriteInputBuffer(fCanWriteInputBuffer)
{
}

// Routine Description:
// - Destructs an instance of the process policy class.
ConsoleProcessPolicy::~ConsoleProcessPolicy()
{
}

// Routine Description:
// - Opens the process token for the given handle and resolves the application model policies
//   that apply to the given process handle. This may reveal restrictions on operations that are
//   supposed to be enforced against a given console client application.
// Arguments:
// - hProcess - Handle to a connected process
// Return Value:
// - ConsoleProcessPolicy object containing resolved policy data.
ConsoleProcessPolicy ConsoleProcessPolicy::s_CreateInstance(const HANDLE hProcess)
{
    // If we cannot determine the policy status, then we block access by default.
    bool fCanReadOutputBuffer = false;
    bool fCanWriteInputBuffer = false;

    wil::unique_handle hToken;
    if (LOG_IF_WIN32_BOOL_FALSE(OpenProcessToken(hProcess, TOKEN_READ, &hToken)))
    {
        bool fIsWrongWayBlocked = true;

        // First check AppModel Policy:
        LOG_IF_FAILED(Microsoft::Console::Internal::ProcessPolicy::CheckAppModelPolicy(hToken.get(), fIsWrongWayBlocked));

        // If we're not restricted by AppModel Policy, also check for Integrity Level below our own.
        if (!fIsWrongWayBlocked)
        {
            LOG_IF_FAILED(Microsoft::Console::Internal::ProcessPolicy::CheckIntegrityLevelPolicy(hToken.get(), fIsWrongWayBlocked));
        }

        // If we're not blocking wrong way verbs, adjust the read/write policies to permit read out and write in.
        if (!fIsWrongWayBlocked)
        {
            fCanReadOutputBuffer = true;
            fCanWriteInputBuffer = true;
        }
    }

    return ConsoleProcessPolicy(fCanReadOutputBuffer, fCanWriteInputBuffer);
}

// Routine Description:
// - Determines whether a console client should be allowed to read back from the output buffers.
// - This includes any of our classic APIs which could allow retrieving data from the output "screen buffer".
// Arguments:
// - <none>
// Return Value:
// - True if read back is allowed. False otherwise.
bool ConsoleProcessPolicy::CanReadOutputBuffer() const
{
    return _fCanReadOutputBuffer;
}

// Routine Description:
// - Determines whether a console client should be allowed to write to the input buffers.
// - This includes any of our classic APIs which could allow inserting data into the input buffer.
// Arguments:
// - <none>
// Return Value:
// - True if writing input is allowed. False otherwise.
bool ConsoleProcessPolicy::CanWriteInputBuffer() const
{
    return _fCanWriteInputBuffer;
}
