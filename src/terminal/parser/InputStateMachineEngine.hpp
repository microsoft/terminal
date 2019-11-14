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

        bool ActionPrintString(const wchar_t* const rgwch,
                               const size_t cch) override;

        bool ActionPassThroughString(const wchar_t* const rgwch,
                                     size_t const cch) override;

        bool ActionEscDispatch(const wchar_t wch,
                               const unsigned short cIntermediate,
                               const wchar_t wchIntermediate) override;

        bool ActionCsiDispatch(const wchar_t wch,
                               const unsigned short cIntermediate,
                               const wchar_t wchIntermediate,
                               _In_reads_(cParams) const unsigned short* const rgusParams,
                               const unsigned short cParams);

        bool ActionClear() override;

        bool ActionIgnore() override;

        bool ActionOscDispatch(const wchar_t wch,
                               const unsigned short sOscParam,
                               _Inout_updates_(cchOscString) wchar_t* const pwchOscStringBuffer,
                               const unsigned short cchOscString) override;

        bool ActionSs3Dispatch(const wchar_t wch,
                               _In_reads_(cParams) const unsigned short* const rgusParams,
                               const unsigned short cParams) override;

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

        struct CSI_TO_VKEY
        {
            CsiActionCodes Action;
            short vkey;
        };

        struct GENERIC_TO_VKEY
        {
            GenericKeyIdentifiers Identifier;
            short vkey;
        };

        struct SS3_TO_VKEY
        {
            Ss3ActionCodes Action;
            short vkey;
        };

        static const CSI_TO_VKEY s_rgCsiMap[];
        static const GENERIC_TO_VKEY s_rgGenericMap[];
        static const SS3_TO_VKEY s_rgSs3Map[];

        DWORD _GetCursorKeysModifierState(_In_reads_(cParams) const unsigned short* const rgusParams,
                                          const unsigned short cParams);
        DWORD _GetGenericKeysModifierState(_In_reads_(cParams) const unsigned short* const rgusParams,
                                           const unsigned short cParams);
        bool _GenerateKeyFromChar(const wchar_t wch, _Out_ short* const pVkey, _Out_ DWORD* const pdwModifierState);

        bool _IsModified(const unsigned short cParams);
        DWORD _GetModifier(const unsigned short modifierParam);

        bool _GetGenericVkey(_In_reads_(cParams) const unsigned short* const rgusParams,
                             const unsigned short cParams,
                             _Out_ short* const pVkey) const;
        bool _GetCursorKeysVkey(const wchar_t wch, _Out_ short* const pVkey) const;
        bool _GetSs3KeysVkey(const wchar_t wch, _Out_ short* const pVkey) const;

        bool _WriteSingleKey(const short vkey, const DWORD dwModifierState);
        bool _WriteSingleKey(const wchar_t wch, const short vkey, const DWORD dwModifierState);

        size_t _GenerateWrappedSequence(const wchar_t wch,
                                        const short vkey,
                                        const DWORD dwModifierState,
                                        _Inout_updates_(cInput) INPUT_RECORD* rgInput,
                                        const size_t cInput);

        size_t _GetSingleKeypress(const wchar_t wch,
                                  const short vkey,
                                  const DWORD dwModifierState,
                                  _Inout_updates_(cRecords) INPUT_RECORD* const rgInput,
                                  const size_t cRecords);

        bool _GetWindowManipulationType(_In_reads_(cParams) const unsigned short* const rgusParams,
                                        const unsigned short cParams,
                                        _Out_ unsigned int* const puiFunction) const;

        static const unsigned int s_uiDefaultLine = 1;
        static const unsigned int s_uiDefaultColumn = 1;
        bool _GetXYPosition(_In_reads_(cParams) const unsigned short* const rgusParams,
                            const unsigned short cParams,
                            _Out_ unsigned int* const puiLine,
                            _Out_ unsigned int* const puiColumn) const;

        bool _DoControlCharacter(const wchar_t wch, const bool writeAlt);
    };
}
