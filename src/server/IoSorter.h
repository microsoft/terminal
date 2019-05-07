/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IoSorter.h

Abstract:
- This file sorts out the various IO requests that can occur and finds an appropriate target.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in srvinit.cpp 
--*/

#pragma once

#include "ApiMessage.h"

class IoSorter
{
public:
    // TODO: MSFT: 9115192 - probably not void.
    static void ServiceIoOperation(_In_ CONSOLE_API_MSG* const pMsg,
                                   _Out_ CONSOLE_API_MSG** ReplyMsg);
};
