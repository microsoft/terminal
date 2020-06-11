/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- adaptDispatch.hpp

Abstract:
- This serves as the Windows Console API-specific implementation of the
    callbacks from our generic Virtual Terminal Input parser.

Author(s):
- Mike Griese (migrie) 11 Oct 2017
--*/
#pragma once

#include "DispatchTypes.hpp"
#include "IInteractDispatch.hpp"
#include "conGetSet.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class InteractDispatch : public IInteractDispatch
    {
    public:
        InteractDispatch(std::unique_ptr<ConGetSet> pConApi);

        bool WriteInput(std::deque<std::unique_ptr<IInputEvent>>& inputEvents) override;
        bool WriteCtrlKey(const KeyEvent& event) override;
        bool WriteString(const std::wstring_view string) override;
        bool WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                const std::basic_string_view<size_t> parameters) override; // DTTERM_WindowManipulation
        bool MoveCursor(const size_t row, const size_t col) override;

        bool IsVtInputEnabled() const override;

    private:
        std::unique_ptr<ConGetSet> _pConApi;
    };
}
