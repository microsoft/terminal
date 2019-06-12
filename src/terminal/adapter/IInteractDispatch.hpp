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
        virtual ~IInteractDispatch() = default;

        virtual bool WriteInput(_In_ std::deque<std::unique_ptr<IInputEvent>>& inputEvents) = 0;

        virtual bool WriteCtrlC() = 0;

        virtual bool WriteString(_In_reads_(cch) const wchar_t* const pws, const size_t cch) = 0;

        virtual bool WindowManipulation(const DispatchTypes::WindowManipulationType uiFunction,
                                        _In_reads_(cParams) const unsigned short* const rgusParams,
                                        const size_t cParams) = 0;

        virtual bool MoveCursor(const unsigned int row,
                                const unsigned int col) = 0;
    };
}
