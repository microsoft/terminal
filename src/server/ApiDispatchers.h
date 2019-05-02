/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ApiDispatchers.h

Abstract:
- This file decodes the client's API request message and dispatches it to the appropriate defined routine in the server.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in srvinit.cpp
--*/

#pragma once

#include "IApiRoutines.h"

#include "ApiMessage.h"

namespace ApiDispatchers
{
    [[nodiscard]] HRESULT ServerDeprecatedApi(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);

#pragma region L1
    [[nodiscard]] HRESULT ServerGetConsoleCP(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleMode(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleMode(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetNumberOfInputEvents(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleInput(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerReadConsole(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerWriteConsole(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleLangId(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
#pragma endregion

#pragma region L2
    [[nodiscard]] HRESULT ServerFillConsoleOutput(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGenerateConsoleCtrlEvent(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleActiveScreenBuffer(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerFlushConsoleInputBuffer(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleCP(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleCursorInfo(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleCursorInfo(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleScreenBufferInfo(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleScreenBufferInfo(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleScreenBufferSize(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleCursorPosition(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetLargestConsoleWindowSize(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerScrollConsoleScreenBuffer(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleTextAttribute(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleWindowInfo(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerReadConsoleOutputString(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerWriteConsoleInput(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerWriteConsoleOutput(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerWriteConsoleOutputString(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerReadConsoleOutput(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleTitle(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleTitle(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
#pragma endregion

#pragma region L3
    [[nodiscard]] HRESULT ServerGetConsoleMouseInfo(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleFontSize(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleCurrentFont(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleDisplayMode(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleDisplayMode(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerAddConsoleAlias(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleAlias(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleAliasesLength(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleAliasExesLength(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleAliases(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleAliasExes(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);

    // CMDEXT functions
    [[nodiscard]] HRESULT ServerExpungeConsoleCommandHistory(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleNumberOfCommands(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleCommandHistoryLength(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleCommandHistory(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    // end CMDEXT functions

    [[nodiscard]] HRESULT ServerGetConsoleWindow(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleSelectionInfo(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleProcessList(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerGetConsoleHistory(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleHistory(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
    [[nodiscard]] HRESULT ServerSetConsoleCurrentFont(_Inout_ CONSOLE_API_MSG* const m, _Inout_ BOOL* const pbReplyPending);
#pragma endregion
};
