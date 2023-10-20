// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "CConsoleHandoff.h"

#include "srvinit.h"

// Routine Description:
// - Helper to duplicate a handle to ourselves so we can keep holding onto it
//   after the caller frees the original one.
// Arguments:
// - in - Handle to duplicate
// - out - Where to place the duplicated value
// Return Value:
// - S_OK or Win32 error from `::DuplicateHandle`
static HRESULT _duplicateHandle(const HANDLE in, HANDLE& out)
{
    RETURN_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), in, GetCurrentProcess(), &out, 0, FALSE, DUPLICATE_SAME_ACCESS));
    return S_OK;
}

// Routine Description:
// - Takes the incoming information from COM and prepares a console hosting session in this process.
// Arguments:
// - server - Console driver server handle
// - inputEvent - Event already established that we signal when new input data is available in case the driver is waiting on us
// - msg - Portable attach message containing just enough descriptor payload to get us started in servicing it
// - inboxProcess - Handle to the inbox process so we can watch it to see if it disappears on us.
// - process - Handle to our process for waiting for us to exit
HRESULT CConsoleHandoff::EstablishHandoff(HANDLE server,
                                          HANDLE inputEvent,
                                          PCCONSOLE_PORTABLE_ATTACH_MSG msg,
                                          HANDLE signalPipe,
                                          HANDLE inboxProcess,
                                          HANDLE* process)
try
{
    RETURN_HR_IF(E_INVALIDARG, !process);

    // Fill the descriptor portion of a fresh api message with the received data.
    // The descriptor portion is the "received" packet from the last ask of the driver.
    // The other portions are unnecessary as they track the other buffer state, error codes,
    // and the return portion of the api message.
    // We will re-retrieve the connect information (title, window state, etc.) when the
    // new console session begins servicing this.
    CONSOLE_API_MSG apiMsg{};
    apiMsg.Descriptor.Identifier.HighPart = msg->IdHighPart;
    apiMsg.Descriptor.Identifier.LowPart = msg->IdLowPart;
    apiMsg.Descriptor.Process = static_cast<decltype(apiMsg.Descriptor.Process)>(msg->Process);
    apiMsg.Descriptor.Object = static_cast<decltype(apiMsg.Descriptor.Object)>(msg->Object);
    apiMsg.Descriptor.Function = msg->Function;
    apiMsg.Descriptor.InputSize = msg->InputSize;
    apiMsg.Descriptor.OutputSize = msg->OutputSize;

    // Duplicate the handles from what we received.
    // The contract with COM specifies that any HANDLEs we receive from the caller belong
    // to the caller and will be freed when we leave the scope of this method.
    // Making our own duplicate copy ensures they hang around in our lifetime.
    RETURN_IF_FAILED(_duplicateHandle(server, server));
    RETURN_IF_FAILED(_duplicateHandle(inputEvent, inputEvent));
    RETURN_IF_FAILED(_duplicateHandle(signalPipe, signalPipe));
    RETURN_IF_FAILED(_duplicateHandle(inboxProcess, inboxProcess));

    // Now perform the handoff.
    RETURN_IF_FAILED(ConsoleEstablishHandoff(server, inputEvent, signalPipe, inboxProcess, &apiMsg));

    // Give back a copy of our own process handle to be tracked.
    RETURN_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(),
                                               GetCurrentProcess(),
                                               GetCurrentProcess(),
                                               process,
                                               SYNCHRONIZE,
                                               FALSE,
                                               0));

    return S_OK;
}
CATCH_RETURN();
