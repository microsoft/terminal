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

        bool IsVtInputEnabled() const override;

        void WriteInput(const std::span<const INPUT_RECORD>& inputEvents) override;
        void WriteCtrlKey(const INPUT_RECORD& event) override;
        void WriteString(std::wstring_view string) override;
        void WriteStringRaw(std::wstring_view string) override;
        void WindowManipulation(DispatchTypes::WindowManipulationType function, VTParameter parameter1, VTParameter parameter2) override;
        void MoveCursor(VTInt row, VTInt col) override;
        void FocusChanged(bool focused) override;

    private:
        ConhostInternalGetSet _api;
    };
}
