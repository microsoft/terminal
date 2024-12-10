/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- InteractDispatch.hpp

Abstract:
- Base class for Input State Machine callbacks. When actions occur, they will
    be dispatched to the methods on this interface which must be implemented by
    a child class and passed into the state machine on creation.

Author(s):
- Mike Griese (migrie) 11 Oct 2017
--*/
#pragma once

#include "DispatchTypes.hpp"
#include "../../types/inc/IInputEvent.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class IInteractDispatch
    {
    public:
#pragma warning(push)
#pragma warning(disable : 26432) // suppress rule of 5 violation on interface because tampering with this is fraught with peril
        virtual ~IInteractDispatch() = default;
#pragma warning(pop)

        virtual bool IsVtInputEnabled() const = 0;

        virtual void WriteInput(const std::span<const INPUT_RECORD>& inputEvents) = 0;
        virtual void WriteCtrlKey(const INPUT_RECORD& event) = 0;
        virtual void WriteString(std::wstring_view string) = 0;
        virtual void WriteStringRaw(std::wstring_view string) = 0;
        virtual void WindowManipulation(DispatchTypes::WindowManipulationType function, VTParameter parameter1, VTParameter parameter2) = 0;
        virtual void MoveCursor(VTInt row, VTInt col) = 0;
        virtual void FocusChanged(bool focused) = 0;
    };
}
