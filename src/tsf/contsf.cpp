// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

CConsoleTSF* g_pConsoleTSF = NULL;

extern "C" BOOL ActivateTextServices(HWND hwndConsole, GetSuggestionWindowPos pfnPosition)
{
    if (!g_pConsoleTSF && hwndConsole)
    {
        g_pConsoleTSF = new (std::nothrow) CConsoleTSF(hwndConsole, pfnPosition);
        if (g_pConsoleTSF && SUCCEEDED(g_pConsoleTSF->Initialize()))
        {
            // Conhost calls this function only when the console window has focus.
            g_pConsoleTSF->SetFocus(TRUE);
        }
        else
        {
            SafeReleaseClear(g_pConsoleTSF);
        }
    }
    return g_pConsoleTSF ? TRUE : FALSE;
}

extern "C" void DeactivateTextServices()
{
    if (g_pConsoleTSF)
    {
        g_pConsoleTSF->Uninitialize();
        SafeReleaseClear(g_pConsoleTSF);
    }
}
