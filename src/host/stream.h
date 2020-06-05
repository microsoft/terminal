/*++
Copyright (c) Microsoft Corporation

Licensed under the MIT license.
Module Name:
- stream.h

Abstract:
- This file implements the NT console server stream API

Author:
- Therese Stowell (ThereseS) 6-Nov-1990

Revision History:
--*/

#pragma once

#include "cmdline.h"
#include "..\server\IWaitRoutine.h"
#include "readData.hpp"

#define IS_CONTROL_CHAR(wch) ((wch) < L' ')

[[nodiscard]] NTSTATUS GetChar(_Inout_ InputBuffer* const pInputBuffer,
                               _Out_ wchar_t* const pwchOut,
                               const bool Wait,
                               _Out_opt_ bool* const pCommandLineEditingKeys,
                               _Out_opt_ bool* const pPopupKeys,
                               _Out_opt_ DWORD* const pdwKeyState) noexcept;

// Routine Description:
// - This routine returns the total number of screen spaces the characters up to the specified character take up.
size_t RetrieveTotalNumberOfSpaces(const SHORT sOriginalCursorPositionX,
                                   _In_reads_(ulCurrentPosition) const WCHAR* const pwchBuffer,
                                   const size_t ulCurrentPosition);

// Routine Description:
// - This routine returns the number of screen spaces the specified character takes up.
size_t RetrieveNumberOfSpaces(_In_ SHORT sOriginalCursorPositionX,
                              _In_reads_(ulCurrentPosition + 1) const WCHAR* const pwchBuffer,
                              _In_ size_t ulCurrentPosition);

VOID UnblockWriteConsole(const DWORD dwReason);
