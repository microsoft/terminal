/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IUiaEventDispatcher.h

Abstract:
- This serves as the interface allowing accessibility providers to detect and respond to accessibility events
- This interface should allow accessibility events to be supported across different implementation patterns

Author(s):
- Carlos Zamora (CaZamor) Sept-2019
--*/

#pragma once

namespace Microsoft::Console::Types
{
    enum class ConsoleUiaEvent
    {
        SelectionChanged,
        TextChanged,
        CursorChanged
    };

    class IUiaEventDispatcher
    {
    public:
        virtual void SignalUia(ConsoleUiaEvent eventId) = 0;
    };
}
