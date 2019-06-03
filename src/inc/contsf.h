/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    contsf.h

Abstract:

    This module contains the internal structures and definitions used
    by the console IME.

Author:

    v-HirShi Jul.4.1995

Revision History:

--*/

#pragma once

#include "conime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef RECT (*GetSuggestionWindowPos)();

BOOL ActivateTextServices(HWND hwndConsole, GetSuggestionWindowPos pfnPosition);
void DeactivateTextServices();
BOOL NotifyTextServices(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* lplResult);

#ifdef __cplusplus
}
#endif
