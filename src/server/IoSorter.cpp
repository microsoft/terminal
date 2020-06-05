// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "IoSorter.h"

#include "IoDispatchers.h"
#include "ApiDispatchers.h"

#include "ApiSorter.h"

#include "..\host\globals.h"

#include "..\host\getset.h"
#include "..\host\stream.h"

void IoSorter::ServiceIoOperation(_In_ CONSOLE_API_MSG* const pMsg,
                                  _Out_ CONSOLE_API_MSG** ReplyMsg)
{
    NTSTATUS Status;
    HRESULT hr;
    BOOL ReplyPending = FALSE;

    ZeroMemory(&pMsg->State, sizeof(pMsg->State));
    ZeroMemory(&pMsg->Complete, sizeof(CD_IO_COMPLETE));

    pMsg->Complete.Identifier = pMsg->Descriptor.Identifier;

    switch (pMsg->Descriptor.Function)
    {
    case CONSOLE_IO_USER_DEFINED:
        *ReplyMsg = IoDispatchers::ConsoleDispatchRequest(pMsg);
        break;

    case CONSOLE_IO_CONNECT:
        *ReplyMsg = IoDispatchers::ConsoleHandleConnectionRequest(pMsg);
        break;

    case CONSOLE_IO_DISCONNECT:
        *ReplyMsg = IoDispatchers::ConsoleClientDisconnectRoutine(pMsg);
        break;

    case CONSOLE_IO_CREATE_OBJECT:
        *ReplyMsg = IoDispatchers::ConsoleCreateObject(pMsg);
        break;

    case CONSOLE_IO_CLOSE_OBJECT:
        *ReplyMsg = IoDispatchers::ConsoleCloseObject(pMsg);
        break;

    case CONSOLE_IO_RAW_WRITE:
        ZeroMemory(&pMsg->u.consoleMsgL1.WriteConsole, sizeof(CONSOLE_WRITECONSOLE_MSG));
        pMsg->msgHeader.ApiNumber = API_NUMBER_WRITECONSOLE; // Required for Wait blocks to identify the right callback.
        ReplyPending = FALSE;
        hr = ApiDispatchers::ServerWriteConsole(pMsg, &ReplyPending);
        Status = NTSTATUS_FROM_HRESULT(hr);
        if (ReplyPending)
        {
            *ReplyMsg = nullptr;
        }
        else
        {
            pMsg->SetReplyStatus(Status);
            *ReplyMsg = pMsg;
        }
        break;

    case CONSOLE_IO_RAW_READ:
        ZeroMemory(&pMsg->u.consoleMsgL1.ReadConsole, sizeof(CONSOLE_READCONSOLE_MSG));
        pMsg->msgHeader.ApiNumber = API_NUMBER_READCONSOLE; // Required for Wait blocks to identify the right callback.
        pMsg->u.consoleMsgL1.ReadConsole.ProcessControlZ = TRUE;
        ReplyPending = FALSE;
        hr = ApiDispatchers::ServerReadConsole(pMsg, &ReplyPending);
        Status = NTSTATUS_FROM_HRESULT(hr);
        if (ReplyPending)
        {
            *ReplyMsg = nullptr;
        }
        else
        {
            pMsg->SetReplyStatus(Status);
            *ReplyMsg = pMsg;
        }
        break;

    case CONSOLE_IO_RAW_FLUSH:
        ReplyPending = FALSE;

        Status = NTSTATUS_FROM_HRESULT(ApiDispatchers::ServerFlushConsoleInputBuffer(pMsg, &ReplyPending));
        FAIL_FAST_IF(!(!ReplyPending));
        pMsg->SetReplyStatus(Status);
        *ReplyMsg = pMsg;
        break;

    default:
        pMsg->SetReplyStatus(STATUS_UNSUCCESSFUL);
        *ReplyMsg = pMsg;
    }
}
