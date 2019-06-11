/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ApiMessageState.h

Abstract:
- This file extends the published structure of an API message's state to provide encapsulation and helper methods

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in util.h & conapi.h
--*/

#pragma once

typedef struct _CONSOLE_API_STATE
{
    ULONG WriteOffset;
    ULONG ReadOffset;
    ULONG InputBufferSize;
    ULONG OutputBufferSize;
    PVOID InputBuffer;
    PVOID OutputBuffer;
} CONSOLE_API_STATE, *PCONSOLE_API_STATE, * const PCCONSOLE_API_STATE;
