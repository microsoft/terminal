/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IoDispatchers.h

Abstract:
- This file processes a majority of server-contained IO operations received from a client

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in srvinit.cpp 
--*/

#pragma once

#include "ApiMessage.h"

class IoDispatchers
{
public:
    // TODO: MSFT: 9115192 temp for now. going to ApiSorter and IoDispatchers
    static PCONSOLE_API_MSG ConsoleHandleConnectionRequest(_In_ PCONSOLE_API_MSG pReceiveMsg);
    static PCONSOLE_API_MSG ConsoleDispatchRequest(_In_ PCONSOLE_API_MSG pMessage);
    static PCONSOLE_API_MSG ConsoleCreateObject(_In_ PCONSOLE_API_MSG pMessage);
    static PCONSOLE_API_MSG ConsoleCloseObject(_In_ PCONSOLE_API_MSG pMessage);
    static PCONSOLE_API_MSG ConsoleClientDisconnectRoutine(_In_ PCONSOLE_API_MSG pMessage);
};
