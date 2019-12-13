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
        InputStateMachineEngine(IInteractDispatch* const pDispatch);
        InputStateMachineEngine(IInteractDispatch* const pDispatch,
                                const bool lookingForDSR);

        bool ActionExecute(const wchar_t wch) override;
        bool ActionExecuteFromEscape(const wchar_t wch) override;

        bool ActionPrint(const wchar_t wch) override;

        bool ActionPrintString(const std::wstring_view string) override;

        bool ActionPassThroughString(const std::wstring_view string) override;

        bool ActionEscDispatch(const wchar_t wch,
                               const std::optional<wchar_t> intermediate) override;

        bool ActionCsiDispatch(const wchar_t wch,
                               const std::optional<wchar_t> intermediate,
                               const std::basic_string_view<size_t> parameters) override;

        bool ActionClear() override;

        bool ActionIgnore() override;

        bool ActionOscDispatch(const wchar_t wch,
                               const size_t parameter,
                               const std::wstring_view string) override;

        bool ActionSs3Dispatch(const wchar_t wch,
                               const std::basic_string_view<size_t> parameters) override;

        bool FlushAtEndOfString() const override;
        bool DispatchControlCharsFromEscape() const override;
        bool DispatchIntermediatesFromEscape() const override;

    private:
        const std::unique_ptr<IInteractDispatch> _pDispatch;
        bool _lookingForDSR;

        enum CsiActionCodes : wchar_t
        {
            ArrowUp = L'A',
            ArrowDown = L'B',
            ArrowRight = L'C',
            ArrowLeft = L'D',
            Home = L'H',
            End = L'F',
            Generic = L'~', // Used for a whole bunch of possible keys
            CSI_F1 = L'P',
            CSI_F2 = L'Q',
            CSI_F3 = L'R', // Both F3 and DSR are on R.
            // DSR_DeviceStatusReportResponse = L'R',
            CSI_F4 = L'S',
            DTTERM_WindowManipulation = L't',
            CursorBackTab = L'Z',
        };

        enum Ss3ActionCodes : wchar_t
        {
            // The "Cursor Keys" are sometimes sent as a SS3 in "application mode"
            //  But for now we'll only accept them as Normal Mode sequences, as CSI's.
            // ArrowUp = L'A',
            // ArrowDown = L'B',
            // ArrowRight = L'C',
            // ArrowLeft = L'D',
            // Home = L'H',
            // End = L'F',
            SS3_F1 = L'P',
            SS3_F2 = L'Q',
            SS3_F3 = L'R',
            SS3_F4 = L'S',
        };

        // Sequences ending in '~' use these numbers as identifiers.
        enum GenericKeyIdentifiers : unsigned short
        {
            GenericHome = 1,
            Insert = 2,
            Delete = 3,
            GenericEnd = 4,
            Prior = 5, //PgUp
            Next = 6, //PgDn
            F5 = 15,
            F6 = 17,
            F7 = 18,
            F8 = 19,
            F9 = 20,
            F10 = 21,
            F11 = 23,
            F12 = 24,
        };

        static const std::map<CsiActionCodes, short> CsiMap;
        static const std::map<GenericKeyIdentifiers, short> GenericMap;
        static const std::map<Ss3ActionCodes, short> Ss3Map;

        DWORD _GetCursorKeysModifierState(const std::basic_string_view<size_t> parameters);
        DWORD _GetGenericKeysModifierState(const std::basic_string_view<size_t> parameters);
        bool _GenerateKeyFromChar(const wchar_t wch, short& vkey, DWORD& modifierState);

        bool _IsModified(const size_t paramCount);
        DWORD _GetModifier(const size_t parameter);

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
                                        unsigned int& function) const;

        static constexpr size_t DefaultLine = 1;
        static constexpr size_t DefaultColumn = 1;
        bool _GetXYPosition(const std::basic_string_view<size_t> parameters,
                            size_t& line,
                            size_t& column) const;

        bool _DoControlCharacter(const wchar_t wch, const bool writeAlt);
    };
}
