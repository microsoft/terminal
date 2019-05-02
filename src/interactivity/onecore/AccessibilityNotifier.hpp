/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IAccessibilityNotifier.hpp

Abstract:
- OneCore implementation of the IAccessibilityNotifier interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "..\inc\IAccessibilityNotifier.hpp"

#pragma hdrstop

namespace Microsoft::Console::Interactivity::OneCore
{
    class AccessibilityNotifier sealed : public IAccessibilityNotifier
    {
    public:
        void NotifyConsoleCaretEvent(_In_ RECT rectangle);
        void NotifyConsoleCaretEvent(_In_ ConsoleCaretEventFlags flags, _In_ LONG position);
        void NotifyConsoleUpdateScrollEvent(_In_ LONG x, _In_ LONG y);
        void NotifyConsoleUpdateSimpleEvent(_In_ LONG start, _In_ LONG charAndAttribute);
        void NotifyConsoleUpdateRegionEvent(_In_ LONG startXY, _In_ LONG endXY);
        void NotifyConsoleLayoutEvent();
        void NotifyConsoleStartApplicationEvent(_In_ DWORD processId);
        void NotifyConsoleEndApplicationEvent(_In_ DWORD processId);
    };
}
