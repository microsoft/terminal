/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IAccessibilityNotifier.hpp

Abstract:
- Defines accessibility notification methods used by accessibility systems to
  provide accessible access to the console.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    class IAccessibilityNotifier
    {
    public:
        enum class ConsoleCaretEventFlags
        {
            CaretInvisible,
            CaretSelection,
            CaretVisible
        };

        virtual ~IAccessibilityNotifier() = 0;

        virtual void NotifyConsoleCaretEvent(_In_ RECT rectangle) = 0;
        virtual void NotifyConsoleCaretEvent(_In_ ConsoleCaretEventFlags flags, _In_ LONG position) = 0;
        virtual void NotifyConsoleUpdateScrollEvent(_In_ LONG x, _In_ LONG y) = 0;
        virtual void NotifyConsoleUpdateSimpleEvent(_In_ LONG start, _In_ LONG charAndAttribute) = 0;
        virtual void NotifyConsoleUpdateRegionEvent(_In_ LONG startXY, _In_ LONG endXY) = 0;
        virtual void NotifyConsoleLayoutEvent() = 0;
        virtual void NotifyConsoleStartApplicationEvent(_In_ DWORD processId) = 0;
        virtual void NotifyConsoleEndApplicationEvent(_In_ DWORD processId) = 0;

    protected:
        IAccessibilityNotifier() {}

        IAccessibilityNotifier(IAccessibilityNotifier const&) = delete;
        IAccessibilityNotifier& operator=(IAccessibilityNotifier const&) = delete;
    };

    inline IAccessibilityNotifier::~IAccessibilityNotifier() {}
}
