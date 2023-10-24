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

        virtual bool WriteInput(const std::span<const INPUT_RECORD>& inputEvents) = 0;

        virtual bool WriteCtrlKey(const INPUT_RECORD& event) = 0;

        virtual bool WriteString(const std::wstring_view string) = 0;

        virtual bool WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                        const VTParameter parameter1,
                                        const VTParameter parameter2) = 0;

        virtual bool MoveCursor(const VTInt row,
                                const VTInt col) = 0;

        virtual bool IsVtInputEnabled() const = 0;

        virtual bool FocusChanged(const bool focused) const = 0;
    };
}
