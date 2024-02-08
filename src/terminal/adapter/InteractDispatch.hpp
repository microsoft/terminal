/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- InteractDispatch.hpp

Abstract:
- This serves as the Windows Console API-specific implementation of the
    callbacks from our generic Virtual Terminal Input parser.

Author(s):
- Mike Griese (migrie) 11 Oct 2017
--*/
#pragma once

#include "DispatchTypes.hpp"
#include "IInteractDispatch.hpp"
#include "../../host/outputStream.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class InteractDispatch : public IInteractDispatch
    {
    public:
        InteractDispatch();

        bool WriteInput(const std::span<const INPUT_RECORD>& inputEvents) override;
        bool WriteCtrlKey(const INPUT_RECORD& event) override;
        bool WriteString(const std::wstring_view string) override;
        bool WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                const VTParameter parameter1,
                                const VTParameter parameter2) override; // DTTERM_WindowManipulation
        bool MoveCursor(const VTInt row, const VTInt col) override;

        bool IsVtInputEnabled() const override;

        bool FocusChanged(const bool focused) const override;

    private:
        ConhostInternalGetSet _api;
    };
}
