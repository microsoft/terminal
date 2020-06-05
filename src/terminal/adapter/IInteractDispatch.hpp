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

        virtual bool WriteInput(std::deque<std::unique_ptr<IInputEvent>>& inputEvents) = 0;

        virtual bool WriteCtrlC() = 0;

        virtual bool WriteString(const std::wstring_view string) = 0;

        virtual bool WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                        const std::basic_string_view<size_t> parameters) = 0;

        virtual bool MoveCursor(const size_t row,
                                const size_t col) = 0;

        virtual bool IsVtInputEnabled() const = 0;
    };
}
