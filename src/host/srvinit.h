/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- srvinit.h

Abstract:
- This is the main initialization file for the console Server.

Author:
- Therese Stowell (ThereseS) 11-Nov-1990

Revision History:
--*/

#pragma once

#include "conserv.h"

[[nodiscard]] NTSTATUS GetConsoleLangId(const UINT uiOutputCP, _Out_ LANGID* const pLangId);

PWSTR TranslateConsoleTitle(_In_ PCWSTR pwszConsoleTitle, const BOOL fUnexpand, const BOOL fSubstitute);

[[nodiscard]] NTSTATUS ConsoleInitializeConnectInfo(_In_ PCONSOLE_API_MSG Message, _Out_ PCONSOLE_API_CONNECTINFO Cac);
[[nodiscard]] NTSTATUS ConsoleAllocateConsole(PCONSOLE_API_CONNECTINFO p);
[[nodiscard]] NTSTATUS RemoveConsole(_In_ ConsoleProcessHandle* ProcessData);

void ConsoleCheckDebug();
