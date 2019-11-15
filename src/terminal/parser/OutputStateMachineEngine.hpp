// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- OutputStateMachineEngine.hpp

Abstract:
- This is the implementation of the client VT output state machine engine.
*/
#pragma once

#include <functional>

#include "../adapter/termDispatch.hpp"
#include "telemetry.hpp"
#include "IStateMachineEngine.hpp"
#include "../../inc/ITerminalOutputConnection.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class OutputStateMachineEngine : public IStateMachineEngine
    {
    public:
        OutputStateMachineEngine(ITermDispatch* const pDispatch);
        ~OutputStateMachineEngine();

        bool ActionExecute(const wchar_t wch) override;
        bool ActionExecuteFromEscape(const wchar_t wch) override;

        bool ActionPrint(const wchar_t wch) override;

        bool ActionPrintString(const wchar_t* const rgwch, const size_t cch) override;

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

        void SetTerminalConnection(Microsoft::Console::ITerminalOutputConnection* const pTtyConnection,
                                   std::function<bool()> pfnFlushToTerminal);

        const ITermDispatch& Dispatch() const noexcept;
        ITermDispatch& Dispatch() noexcept;

    private:
        std::unique_ptr<ITermDispatch> _dispatch;
        Microsoft::Console::ITerminalOutputConnection* _pTtyConnection;
        std::function<bool()> _pfnFlushToTerminal;
        wchar_t _lastPrintedChar;

        bool _IntermediateQuestionMarkDispatch(const wchar_t wchAction,
                                               _In_reads_(cParams) const unsigned short* const rgusParams,
                                               const unsigned short cParams);
        bool _IntermediateExclamationDispatch(const wchar_t wch);
        bool _IntermediateSpaceDispatch(const wchar_t wchAction,
                                        _In_reads_(cParams) const unsigned short* const rgusParams,
                                        const unsigned short cParams);

        enum VTActionCodes : wchar_t
        {
            CUU_CursorUp = L'A',
            CUD_CursorDown = L'B',
            CUF_CursorForward = L'C',
            CUB_CursorBackward = L'D',
            CNL_CursorNextLine = L'E',
            CPL_CursorPrevLine = L'F',
            CHA_CursorHorizontalAbsolute = L'G',
            CUP_CursorPosition = L'H',
            ED_EraseDisplay = L'J',
            EL_EraseLine = L'K',
            SU_ScrollUp = L'S',
            SD_ScrollDown = L'T',
            ICH_InsertCharacter = L'@',
            DCH_DeleteCharacter = L'P',
            SGR_SetGraphicsRendition = L'm',
            DECSC_CursorSave = L'7',
            DECRC_CursorRestore = L'8',
            DECSET_PrivateModeSet = L'h',
            DECRST_PrivateModeReset = L'l',
            ANSISYSSC_CursorSave = L's', // NOTE: Overlaps with DECLRMM/DECSLRM. Fix when/if implemented.
            ANSISYSRC_CursorRestore = L'u', // NOTE: Overlaps with DECSMBV. Fix when/if implemented.
            DECKPAM_KeypadApplicationMode = L'=',
            DECKPNM_KeypadNumericMode = L'>',
            DSR_DeviceStatusReport = L'n',
            DA_DeviceAttributes = L'c',
            DECSCPP_SetColumnsPerPage = L'|',
            IL_InsertLine = L'L',
            DL_DeleteLine = L'M', // Yes, this is the same as RI, however, RI is not preceeded by a CSI, and DL is.
            HPA_HorizontalPositionAbsolute = L'`',
            VPA_VerticalLinePositionAbsolute = L'd',
            DECSTBM_SetScrollingRegion = L'r',
            RI_ReverseLineFeed = L'M',
            HTS_HorizontalTabSet = L'H', // Not a CSI, so doesn't overlap with CUP
            CHT_CursorForwardTab = L'I',
            CBT_CursorBackTab = L'Z',
            TBC_TabClear = L'g',
            ECH_EraseCharacters = L'X',
            HVP_HorizontalVerticalPosition = L'f',
            DECSTR_SoftReset = L'p',
            RIS_ResetToInitialState = L'c', // DA is prefaced by CSI, RIS by ESC
            // 'q' is overloaded - no postfix is DECLL, ' ' postfix is DECSCUSR, and '"' is DECSCA
            DECSCUSR_SetCursorStyle = L'q', // I believe we'll only ever implement DECSCUSR
            DTTERM_WindowManipulation = L't',
            REP_RepeatCharacter = L'b'
        };

        enum OscActionCodes : unsigned int
        {
            SetIconAndWindowTitle = 0,
            SetWindowIcon = 1,
            SetWindowTitle = 2,
            SetWindowProperty = 3, // Not implemented
            SetColor = 4,
            SetForegroundColor = 10,
            SetBackgroundColor = 11,
            SetCursorColor = 12,
            ResetForegroundColor = 110, // Not implemented
            ResetBackgroundColor = 111, // Not implemented
            ResetCursorColor = 112,
        };

        enum class DesignateCharsetTypes
        {
            G0,
            G1,
            G2,
            G3
        };

        static const DispatchTypes::GraphicsOptions s_defaultGraphicsOption = DispatchTypes::GraphicsOptions::Off;
        _Success_(return ) bool _GetGraphicsOptions(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                    const unsigned short cParams,
                                                    _Out_writes_(*pcOptions) DispatchTypes::GraphicsOptions* const rgGraphicsOptions,
                                                    _Inout_ size_t* const pcOptions) const;

        static const DispatchTypes::EraseType s_defaultEraseType = DispatchTypes::EraseType::ToEnd;
        _Success_(return ) bool _GetEraseOperation(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                   const unsigned short cParams,
                                                   _Out_ DispatchTypes::EraseType* const pEraseType) const;

        static const unsigned int s_uiDefaultCursorDistance = 1;
        _Success_(return ) bool _GetCursorDistance(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                   const unsigned short cParams,
                                                   _Out_ unsigned int* const puiDistance) const;

        static const unsigned int s_uiDefaultScrollDistance = 1;
        _Success_(return ) bool _GetScrollDistance(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                   const unsigned short cParams,
                                                   _Out_ unsigned int* const puiDistance) const;

        static const unsigned int s_uiDefaultConsoleWidth = 80;
        _Success_(return ) bool _GetConsoleWidth(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                 const unsigned short cParams,
                                                 _Out_ unsigned int* const puiConsoleWidth) const;

        static const unsigned int s_uiDefaultLine = 1;
        static const unsigned int s_uiDefaultColumn = 1;
        _Success_(return ) bool _GetXYPosition(_In_reads_(cParams) const unsigned short* const rgusParams,
                                               const unsigned short cParams,
                                               _Out_ unsigned int* const puiLine,
                                               _Out_ unsigned int* const puiColumn) const;

        _Success_(return ) bool _GetDeviceStatusOperation(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                          const unsigned short cParams,
                                                          _Out_ DispatchTypes::AnsiStatusType* const pStatusType) const;

        _Success_(return ) bool _VerifyHasNoParameters(const unsigned short cParams) const;

        _Success_(return ) bool _VerifyDeviceAttributesParams(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                              const unsigned short cParams) const;

        _Success_(return ) bool _GetPrivateModeParams(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                      const unsigned short cParams,
                                                      _Out_writes_(*pcParams) DispatchTypes::PrivateModeParams* const rgPrivateModeParams,
                                                      _Inout_ size_t* const pcParams) const;

        static const SHORT s_sDefaultTopMargin = 0;
        static const SHORT s_sDefaultBottomMargin = 0;
        _Success_(return ) bool _GetTopBottomMargins(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                     const unsigned short cParams,
                                                     _Out_ SHORT* const psTopMargin,
                                                     _Out_ SHORT* const psBottomMargin) const;

        _Success_(return ) bool _GetOscTitle(_Inout_updates_(cchOscString) wchar_t* const pwchOscStringBuffer,
                                             const unsigned short cchOscString,
                                             _Outptr_result_buffer_(*pcchTitle) wchar_t** const ppwchTitle,
                                             _Out_ unsigned short* pcchTitle) const;

        static const SHORT s_sDefaultTabDistance = 1;
        _Success_(return ) bool _GetTabDistance(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                const unsigned short cParams,
                                                _Out_ SHORT* const psDistance) const;

        static const SHORT s_sDefaultTabClearType = 0;
        _Success_(return ) bool _GetTabClearType(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                 const unsigned short cParams,
                                                 _Out_ SHORT* const psClearType) const;

        static const DesignateCharsetTypes s_DefaultDesignateCharsetType = DesignateCharsetTypes::G0;
        _Success_(return ) bool _GetDesignateType(const wchar_t wchIntermediate,
                                                  _Out_ DesignateCharsetTypes* const pDesignateType) const;

        static const DispatchTypes::WindowManipulationType s_DefaultWindowManipulationType = DispatchTypes::WindowManipulationType::Invalid;
        _Success_(return ) bool _GetWindowManipulationType(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                           const unsigned short cParams,
                                                           _Out_ unsigned int* const puiFunction) const;

        static bool s_HexToUint(const wchar_t wch,
                                _Out_ unsigned int* const puiValue);
        static bool s_IsNumber(const wchar_t wch);
        static bool s_IsHexNumber(const wchar_t wch);
        bool _GetOscSetColorTable(_In_reads_(cchOscString) const wchar_t* const pwchOscStringBuffer,
                                  const size_t cchOscString,
                                  _Out_ size_t* const pTableIndex,
                                  _Out_ DWORD* const pRgb) const;

        static bool s_ParseColorSpec(_In_reads_(cchBuffer) const wchar_t* const pwchBuffer,
                                     const size_t cchBuffer,
                                     _Out_ DWORD* const pRgb);

        bool _GetOscSetColor(_In_reads_(cchOscString) const wchar_t* const pwchOscStringBuffer,
                             const size_t cchOscString,
                             _Out_ DWORD* const pRgb) const;

        static const DispatchTypes::CursorStyle s_defaultCursorStyle = DispatchTypes::CursorStyle::BlinkingBlockDefault;
        _Success_(return ) bool _GetCursorStyle(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                const unsigned short cParams,
                                                _Out_ DispatchTypes::CursorStyle* const pCursorStyle) const;

        static const unsigned int s_uiDefaultRepeatCount = 1;
        _Success_(return ) bool _GetRepeatCount(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                const unsigned short cParams,
                                                _Out_ unsigned int* const puiRepeatCount) const noexcept;

        void _ClearLastChar() noexcept;
    };
}
