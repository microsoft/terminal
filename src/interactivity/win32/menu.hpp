/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- menu.h

Abstract:
- This module contains the definitions for console system menu

Author:
- Therese Stowell (ThereseS) Feb-3-1992 (swiped from Win3.1)

Revision History:
--*/

#pragma once

#include "precomp.h"

#include "resource.h"

namespace Microsoft::Console::Interactivity::Win32
{
    class Menu sealed
    {
    public:
        Menu(_In_ HMENU hMenu,
             _In_ HMENU hHeirMenu);
        [[nodiscard]] static NTSTATUS CreateInstance(_In_ HWND hWnd);
        static Menu* Instance();
        ~Menu();

        void Initialize();

        static void s_ShowPropertiesDialog(const HWND hwnd, const BOOL Defaults);
        [[nodiscard]] static HRESULT s_GetConsoleState(_Out_ CONSOLE_STATE_INFO* const pStateInfo);

        static HMENU s_GetMenuHandle();
        static HMENU s_GetHeirMenuHandle();

    private:
        static void s_PropertiesUpdate(_In_ PCONSOLE_STATE_INFO pStateInfo);

        static Menu* s_Instance;

        HMENU _hMenu; // handle to system menu
        HMENU _hHeirMenu; // handle to menu we append to system menu
    };
}
