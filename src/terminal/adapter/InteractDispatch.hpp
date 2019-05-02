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

        InteractDispatch(ConGetSet* const pConApi);

        virtual ~InteractDispatch() override = default;

        virtual bool WriteInput(_In_ std::deque<std::unique_ptr<IInputEvent>>& inputEvents) override;
        virtual bool WriteCtrlC() override;
        virtual bool WriteString(_In_reads_(cch) const wchar_t* const pws, const size_t cch) override;
        virtual bool WindowManipulation(const DispatchTypes::WindowManipulationType uiFunction,
                                        _In_reads_(cParams) const unsigned short* const rgusParams,
                                        const size_t cParams) override; // DTTERM_WindowManipulation
        virtual bool MoveCursor(const unsigned int row,
                                const unsigned int col) override;
    private:

        std::unique_ptr<ConGetSet> _pConApi;

    };
}
