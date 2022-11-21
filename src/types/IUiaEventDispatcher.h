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
    class IUiaEventDispatcher
    {
    public:
        virtual ~IUiaEventDispatcher() = default;
        virtual void SignalSelectionChanged() = 0;
        virtual void SignalTextChanged() = 0;
        virtual void SignalCursorChanged() = 0;
        virtual void NotifyNewOutput(std::wstring_view newOutput) = 0;
    };
}
