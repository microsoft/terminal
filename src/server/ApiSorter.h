/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ApiSorter.h

Abstract:
- This file sorts out the various console host serviceable APIs.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in srvinit.cpp
--*/

#pragma once

#include "ApiMessage.h"

typedef HRESULT (*PCONSOLE_API_ROUTINE)(_Inout_ PCONSOLE_API_MSG m, _Inout_ PBOOL ReplyPending);

// These are required for wait routines to accurately identify which function is waited on and needs to be dispatched later.
// It's stored here so it can be easily aligned with the layer descriptions below.
// 0x01 stands for level 1 API (layers are 1-based)
// 0x000004 stands for the 5th one down in the layer structure (call IDs are 0-based)
// clang-format off
#define API_NUMBER_GETCONSOLEINPUT 0x01000004
#define API_NUMBER_READCONSOLE     0x01000005
#define API_NUMBER_WRITECONSOLE    0x01000006
// clang-format on

class ApiSorter
{
public:
    // Routine Description:
    // - This routine validates a user IO and dispatches it to the appropriate worker routine.
    // Arguments:
    // - Message - Supplies the message representing the user IO.
    // Return Value:
    // - A pointer to the reply message, if this message is to be completed inline; nullptr if this message will pend now and complete later.
    static PCONSOLE_API_MSG ConsoleDispatchRequest(_Inout_ PCONSOLE_API_MSG Message);
};
