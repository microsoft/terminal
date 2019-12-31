/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- InputStateMachineEngine.hpp

Abstract:
- This is the implementation of the client VT input state machine engine.
    This generates InputEvents from a stream of VT sequences emitted by a
    client "terminal" application.

Author(s):
- Mike Griese (migrie) 18 Aug 2017
--*/
#pragma once

#include "telemetry.hpp"
#include "IStateMachineEngine.hpp"
#include <functional>
#include "../../types/inc/IInputEvent.hpp"
#include "../adapter/IInteractDispatch.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class InputStateMachineEngine : public IStateMachineEngine
    {
    public:
        InputStateMachineEngine(std::unique_ptr<IInteractDispatch> pDispatch);
        InputStateMachineEngine(std::unique_ptr<IInteractDispatch> pDispatch,
                                const bool lookingForDSR);

        bool ActionExecute(const wchar_t wch) override;
        bool ActionExecuteFromEscape(const wchar_t wch) override;

        bool ActionPrint(const wchar_t wch) override;

        bool ActionPrintString(const std::wstring_view string) override;

        bool ActionPassThroughString(const std::wstring_view string) override;

        bool ActionEscDispatch(const wchar_t wch,
                               const std::basic_string_view<wchar_t> intermediates) override;

        bool ActionCsiDispatch(const wchar_t wch,
                               const std::basic_string_view<wchar_t> intermediates,
                               const std::basic_string_view<size_t> parameters) override;

        bool ActionClear() noexcept override;

        bool ActionIgnore() noexcept override;

        bool ActionOscDispatch(const wchar_t wch,
                               const size_t parameter,
                               const std::wstring_view string) noexcept override;

        bool ActionSs3Dispatch(const wchar_t wch,
                               const std::basic_string_view<size_t> parameters) override;

        bool FlushAtEndOfString() const noexcept override;
        bool DispatchControlCharsFromEscape() const noexcept override;
        bool DispatchIntermediatesFromEscape() const noexcept override;

    private:
        const std::unique_ptr<IInteractDispatch> _pDispatch;
        bool _lookingForDSR;

        DWORD _GetCursorKeysModifierState(const std::basic_string_view<size_t> parameters) noexcept;
        DWORD _GetGenericKeysModifierState(const std::basic_string_view<size_t> parameters) noexcept;
        bool _GenerateKeyFromChar(const wchar_t wch, short& vkey, DWORD& modifierState) noexcept;

        bool _IsModified(const size_t paramCount) noexcept;
        DWORD _GetModifier(const size_t parameter) noexcept;

        bool _GetGenericVkey(const std::basic_string_view<size_t> parameters,
                             short& vkey) const;
        bool _GetCursorKeysVkey(const wchar_t wch, short& vkey) const;
        bool _GetSs3KeysVkey(const wchar_t wch, short& vkey) const;

        bool _WriteSingleKey(const short vkey, const DWORD modifierState);
        bool _WriteSingleKey(const wchar_t wch, const short vkey, const DWORD modifierState);

        void _GenerateWrappedSequence(const wchar_t wch,
                                      const short vkey,
                                      const DWORD modifierState,
                                      std::vector<INPUT_RECORD>& input);

        void _GetSingleKeypress(const wchar_t wch,
                                const short vkey,
                                const DWORD modifierState,
                                std::vector<INPUT_RECORD>& input);

        bool _GetWindowManipulationType(const std::basic_string_view<size_t> parameters,
                                        unsigned int& function) const noexcept;

        static constexpr size_t DefaultLine = 1;
        static constexpr size_t DefaultColumn = 1;
        bool _GetXYPosition(const std::basic_string_view<size_t> parameters,
                            size_t& line,
                            size_t& column) const noexcept;

        bool _DoControlCharacter(const wchar_t wch, const bool writeAlt);
    };
}
