// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "IoDispatchers.h"

#include "ApiSorter.h"

#include "..\host\conserv.h"
#include "..\host\conwinuserrefs.h"
#include "..\host\directio.h"
#include "..\host\handle.h"
#include "..\host\srvinit.h"
#include "..\host\telemetry.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity;

// From ntstatus.h, which we cannot include without causing a bunch of other conflicts. So we just include the one code we need.
//
// MessageId: STATUS_OBJECT_NAME_NOT_FOUND
//
// MessageText:
//
// Object Name not found.
//
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034L)

// Routine Description:
// - This routine handles IO requests to create new objects. It validates the request, creates the object and a "handle" to it.
// Arguments:
// - pMessage - Supplies the message representing the create IO.
// Return Value:
// - A pointer to the reply message, if this message is to be completed inline; nullptr if this message will pend now and complete later.
PCONSOLE_API_MSG IoDispatchers::ConsoleCreateObject(_In_ PCONSOLE_API_MSG pMessage)
{
    NTSTATUS Status;

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    PCD_CREATE_OBJECT_INFORMATION const CreateInformation = &pMessage->CreateObject;

    LockConsole();

    // If a generic object was requested, use the desired access to determine which type of object the caller is expecting.
    if (CreateInformation->ObjectType == CD_IO_OBJECT_TYPE_GENERIC)
    {
        if ((CreateInformation->DesiredAccess & (GENERIC_READ | GENERIC_WRITE)) == GENERIC_READ)
        {
            CreateInformation->ObjectType = CD_IO_OBJECT_TYPE_CURRENT_INPUT;
        }
        else if ((CreateInformation->DesiredAccess & (GENERIC_READ | GENERIC_WRITE)) == GENERIC_WRITE)
        {
            CreateInformation->ObjectType = CD_IO_OBJECT_TYPE_CURRENT_OUTPUT;
        }
    }

    std::unique_ptr<ConsoleHandleData> handle;
    // Check the requested type.
    switch (CreateInformation->ObjectType)
    {
    case CD_IO_OBJECT_TYPE_CURRENT_INPUT:
        Status = NTSTATUS_FROM_HRESULT(gci.pInputBuffer->AllocateIoHandle(ConsoleHandleData::HandleType::Input,
                                                                          CreateInformation->DesiredAccess,
                                                                          CreateInformation->ShareMode,
                                                                          handle));
        break;

    case CD_IO_OBJECT_TYPE_CURRENT_OUTPUT:
    {
        SCREEN_INFORMATION& ScreenInformation = gci.GetActiveOutputBuffer().GetMainBuffer();
        Status = NTSTATUS_FROM_HRESULT(ScreenInformation.AllocateIoHandle(ConsoleHandleData::HandleType::Output,
                                                                          CreateInformation->DesiredAccess,
                                                                          CreateInformation->ShareMode,
                                                                          handle));
        break;
    }
    case CD_IO_OBJECT_TYPE_NEW_OUTPUT:
        Status = ConsoleCreateScreenBuffer(handle, pMessage, CreateInformation, &pMessage->CreateScreenBuffer);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
    }

    if (!NT_SUCCESS(Status))
    {
        goto Error;
    }

    // Complete the request.
    pMessage->SetReplyStatus(STATUS_SUCCESS);
    pMessage->SetReplyInformation(reinterpret_cast<ULONG_PTR>(handle.get()));

    if (SUCCEEDED(ServiceLocator::LocateGlobals().pDeviceComm->CompleteIo(&pMessage->Complete)))
    {
        // We've successfully transfered ownership of the handle to the driver. We can release and not free it.
        handle.release();
    }

    UnlockConsole();

    return nullptr;

Error:

    FAIL_FAST_IF(NT_SUCCESS(Status));

    UnlockConsole();

    pMessage->SetReplyStatus(Status);

    return pMessage;
}

// Routine Description:
// - This routine will handle a request to specifically close one of the console objects./
// Arguments:
// - pMessage - Supplies the message representing the close object IO.
// Return Value:
// - A pointer to the reply message.
PCONSOLE_API_MSG IoDispatchers::ConsoleCloseObject(_In_ PCONSOLE_API_MSG pMessage)
{
    LockConsole();

    delete pMessage->GetObjectHandle();
    pMessage->SetReplyStatus(STATUS_SUCCESS);

    UnlockConsole();
    return pMessage;
}

// Routine Description:
// - Used when a client application establishes an initial connection to this console server.
// - This is supposed to represent accounting for the process, making the appropriate handles, etc.
// Arguments:
// - pReceiveMsg - The packet message received from the driver specifying that a client is connecting
// Return Value:
// - The response data to this request message.
PCONSOLE_API_MSG IoDispatchers::ConsoleHandleConnectionRequest(_In_ PCONSOLE_API_MSG pReceiveMsg)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::AttachConsole);

    ConsoleProcessHandle* ProcessData = nullptr;

    LockConsole();

    DWORD const dwProcessId = (DWORD)pReceiveMsg->Descriptor.Process;
    DWORD const dwThreadId = (DWORD)pReceiveMsg->Descriptor.Object;

    CONSOLE_API_CONNECTINFO Cac;
    NTSTATUS Status = ConsoleInitializeConnectInfo(pReceiveMsg, &Cac);
    if (!NT_SUCCESS(Status))
    {
        goto Error;
    }

    Status = NTSTATUS_FROM_HRESULT(gci.ProcessHandleList.AllocProcessData(dwProcessId,
                                                                          dwThreadId,
                                                                          Cac.ProcessGroupId,
                                                                          nullptr,
                                                                          &ProcessData));

    if (!NT_SUCCESS(Status))
    {
        goto Error;
    }

    ProcessData->fRootProcess = WI_IsFlagClear(gci.Flags, CONSOLE_INITIALIZED);

    // ConsoleApp will be false in the AttachConsole case.
    if (Cac.ConsoleApp)
    {
        LOG_IF_FAILED(ServiceLocator::LocateConsoleControl()->NotifyConsoleApplication(dwProcessId));
    }

    ServiceLocator::LocateAccessibilityNotifier()->NotifyConsoleStartApplicationEvent(dwProcessId);

    if (WI_IsFlagClear(gci.Flags, CONSOLE_INITIALIZED))
    {
        Status = ConsoleAllocateConsole(&Cac);
        if (!NT_SUCCESS(Status))
        {
            goto Error;
        }

        WI_SetFlag(gci.Flags, CONSOLE_INITIALIZED);
    }

    try
    {
        CommandHistory::s_Allocate({ Cac.AppName, Cac.AppNameLength / sizeof(wchar_t) }, (HANDLE)ProcessData);
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        goto Error;
    }

    gci.ProcessHandleList.ModifyConsoleProcessFocus(WI_IsFlagSet(gci.Flags, CONSOLE_HAS_FOCUS));

    // Create the handles.

    Status = NTSTATUS_FROM_HRESULT(gci.pInputBuffer->AllocateIoHandle(ConsoleHandleData::HandleType::Input,
                                                                      GENERIC_READ | GENERIC_WRITE,
                                                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                                      ProcessData->pInputHandle));

    if (!NT_SUCCESS(Status))
    {
        goto Error;
    }

    auto& screenInfo = gci.GetActiveOutputBuffer().GetMainBuffer();
    Status = NTSTATUS_FROM_HRESULT(screenInfo.AllocateIoHandle(ConsoleHandleData::HandleType::Output,
                                                               GENERIC_READ | GENERIC_WRITE,
                                                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                               ProcessData->pOutputHandle));

    if (!NT_SUCCESS(Status))
    {
        goto Error;
    }

    // Complete the request.
    pReceiveMsg->SetReplyStatus(STATUS_SUCCESS);
    pReceiveMsg->SetReplyInformation(sizeof(CD_CONNECTION_INFORMATION));

    CD_CONNECTION_INFORMATION ConnectionInformation = ProcessData->GetConnectionInformation();
    pReceiveMsg->Complete.Write.Data = &ConnectionInformation;
    pReceiveMsg->Complete.Write.Size = sizeof(CD_CONNECTION_INFORMATION);

    if (FAILED(ServiceLocator::LocateGlobals().pDeviceComm->CompleteIo(&pReceiveMsg->Complete)))
    {
        CommandHistory::s_Free((HANDLE)ProcessData);
        gci.ProcessHandleList.FreeProcessData(ProcessData);
    }

    UnlockConsole();

    return nullptr;

Error:
    FAIL_FAST_IF(NT_SUCCESS(Status));

    if (ProcessData != nullptr)
    {
        CommandHistory::s_Free((HANDLE)ProcessData);
        gci.ProcessHandleList.FreeProcessData(ProcessData);
    }

    UnlockConsole();

    pReceiveMsg->SetReplyStatus(Status);

    return pReceiveMsg;
}

// Routine Description:
// - This routine is called when a process is destroyed. It closes the process's handles and frees the console if it's the last reference.
// Arguments:
// - pProcessData - Pointer to the client's process information structure.
// Return Value:
// - A pointer to the reply message.
PCONSOLE_API_MSG IoDispatchers::ConsoleClientDisconnectRoutine(_In_ PCONSOLE_API_MSG pMessage)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::FreeConsole);

    ConsoleProcessHandle* const pProcessData = pMessage->GetProcessHandle();

    ServiceLocator::LocateAccessibilityNotifier()->NotifyConsoleEndApplicationEvent(pProcessData->dwProcessId);

    LOG_IF_FAILED(RemoveConsole(pProcessData));

    pMessage->SetReplyStatus(STATUS_SUCCESS);

    return pMessage;
}

// Routine Description:
// - This routine validates a user IO and dispatches it to the appropriate worker routine.
// Arguments:
// - pMessage - Supplies the message representing the user IO.
// Return Value:
// - A pointer to the reply message, if this message is to be completed inline; nullptr if this message will pend now and complete later.
PCONSOLE_API_MSG IoDispatchers::ConsoleDispatchRequest(_In_ PCONSOLE_API_MSG pMessage)
{
    return ApiSorter::ConsoleDispatchRequest(pMessage);
}
