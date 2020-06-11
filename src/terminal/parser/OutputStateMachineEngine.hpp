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
        OutputStateMachineEngine(std::unique_ptr<ITermDispatch> pDispatch);

        bool ActionExecute(const wchar_t wch) override;
        bool ActionExecuteFromEscape(const wchar_t wch) override;

        bool ActionPrint(const wchar_t wch) override;

        bool ActionPrintString(const std::wstring_view string) override;

        bool ActionPassThroughString(const std::wstring_view string) override;

        bool ActionEscDispatch(const wchar_t wch,
                               const std::basic_string_view<wchar_t> intermediates) override;

        bool ActionVt52EscDispatch(const wchar_t wch,
                                   const std::basic_string_view<wchar_t> intermediates,
                                   const std::basic_string_view<size_t> parameters) override;

        bool ActionCsiDispatch(const wchar_t wch,
                               const std::basic_string_view<wchar_t> intermediates,
                               const std::basic_string_view<size_t> parameters) override;

        bool ActionClear() noexcept override;

        bool ActionIgnore() noexcept override;

        bool ActionOscDispatch(const wchar_t wch,
                               const size_t parameter,
                               const std::wstring_view string) override;

        bool ActionSs3Dispatch(const wchar_t wch,
                               const std::basic_string_view<size_t> parameters) noexcept override;

        bool ParseControlSequenceAfterSs3() const noexcept override;
        bool FlushAtEndOfString() const noexcept override;
        bool DispatchControlCharsFromEscape() const noexcept override;
        bool DispatchIntermediatesFromEscape() const noexcept override;

        void SetTerminalConnection(Microsoft::Console::ITerminalOutputConnection* const pTtyConnection,
                                   std::function<bool()> pfnFlushToTerminal);

        const ITermDispatch& Dispatch() const noexcept;
        ITermDispatch& Dispatch() noexcept;

    private:
        std::unique_ptr<ITermDispatch> _dispatch;
        Microsoft::Console::ITerminalOutputConnection* _pTtyConnection;
        std::function<bool()> _pfnFlushToTerminal;
        wchar_t _lastPrintedChar;
        std::vector<DispatchTypes::GraphicsOptions> _graphicsOptions;

        bool _IntermediateScsDispatch(const wchar_t wch,
                                      const std::basic_string_view<wchar_t> intermediates);
        bool _IntermediateQuestionMarkDispatch(const wchar_t wchAction,
                                               const std::basic_string_view<size_t> parameters);
        bool _IntermediateExclamationDispatch(const wchar_t wch);
        bool _IntermediateSpaceDispatch(const wchar_t wchAction,
                                        const std::basic_string_view<size_t> parameters);

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
            DL_DeleteLine = L'M', // Yes, this is the same as RI, however, RI is not preceded by a CSI, and DL is.
            HPA_HorizontalPositionAbsolute = L'`',
            VPA_VerticalLinePositionAbsolute = L'd',
            HPR_HorizontalPositionRelative = L'a',
            VPR_VerticalPositionRelative = L'e',
            DECSTBM_SetScrollingRegion = L'r',
            NEL_NextLine = L'E', // Not a CSI, so doesn't overlap with CNL
            IND_Index = L'D', // Not a CSI, so doesn't overlap with CUB
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
            REP_RepeatCharacter = L'b',
            SS2_SingleShift = L'N',
            SS3_SingleShift = L'O',
            LS2_LockingShift = L'n',
            LS3_LockingShift = L'o',
            LS1R_LockingShift = L'~',
            LS2R_LockingShift = L'}',
            LS3R_LockingShift = L'|',
            DECALN_ScreenAlignmentPattern = L'8'
        };

        enum Vt52ActionCodes : wchar_t
        {
            CursorUp = L'A',
            CursorDown = L'B',
            CursorRight = L'C',
            CursorLeft = L'D',
            EnterGraphicsMode = L'F',
            ExitGraphicsMode = L'G',
            CursorToHome = L'H',
            ReverseLineFeed = L'I',
            EraseToEndOfScreen = L'J',
            EraseToEndOfLine = L'K',
            DirectCursorAddress = L'Y',
            Identify = L'Z',
            EnterAlternateKeypadMode = L'=',
            ExitAlternateKeypadMode = L'>',
            ExitVt52Mode = L'<'
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
            ResetCursorColor = 112
        };

        static constexpr DispatchTypes::GraphicsOptions DefaultGraphicsOption = DispatchTypes::GraphicsOptions::Off;
        bool _GetGraphicsOptions(const std::basic_string_view<size_t> parameters,
                                 std::vector<DispatchTypes::GraphicsOptions>& options) const;

        static constexpr DispatchTypes::EraseType DefaultEraseType = DispatchTypes::EraseType::ToEnd;
        bool _GetEraseOperation(const std::basic_string_view<size_t> parameters,
                                DispatchTypes::EraseType& eraseType) const noexcept;

        static constexpr size_t DefaultCursorDistance = 1;
        bool _GetCursorDistance(const std::basic_string_view<size_t> parameters,
                                size_t& distance) const noexcept;

        static constexpr size_t DefaultScrollDistance = 1;
        bool _GetScrollDistance(const std::basic_string_view<size_t> parameters,
                                size_t& distance) const noexcept;

        static constexpr size_t DefaultConsoleWidth = 80;
        bool _GetConsoleWidth(const std::basic_string_view<size_t> parameters,
                              size_t& consoleWidth) const noexcept;

        static constexpr size_t DefaultLine = 1;
        static constexpr size_t DefaultColumn = 1;
        bool _GetXYPosition(const std::basic_string_view<size_t> parameters,
                            size_t& line,
                            size_t& column) const noexcept;

        bool _GetDeviceStatusOperation(const std::basic_string_view<size_t> parameters,
                                       DispatchTypes::AnsiStatusType& statusType) const noexcept;

        bool _VerifyHasNoParameters(const std::basic_string_view<size_t> parameters) const noexcept;

        bool _VerifyDeviceAttributesParams(const std::basic_string_view<size_t> parameters) const noexcept;

        bool _GetPrivateModeParams(const std::basic_string_view<size_t> parameters,
                                   std::vector<DispatchTypes::PrivateModeParams>& privateModes) const;

        static constexpr size_t DefaultTopMargin = 0;
        static constexpr size_t DefaultBottomMargin = 0;
        bool _GetTopBottomMargins(const std::basic_string_view<size_t> parameters,
                                  size_t& topMargin,
                                  size_t& bottomMargin) const noexcept;

        bool _GetOscTitle(const std::wstring_view string,
                          std::wstring& title) const;

        static constexpr size_t DefaultTabDistance = 1;
        bool _GetTabDistance(const std::basic_string_view<size_t> parameters,
                             size_t& distance) const noexcept;

        static constexpr size_t DefaultTabClearType = 0;
        bool _GetTabClearType(const std::basic_string_view<size_t> parameters,
                              size_t& clearType) const noexcept;

        static constexpr DispatchTypes::WindowManipulationType DefaultWindowManipulationType = DispatchTypes::WindowManipulationType::Invalid;
        bool _GetWindowManipulationType(const std::basic_string_view<size_t> parameters,
                                        unsigned int& function) const noexcept;

        static bool s_HexToUint(const wchar_t wch,
                                unsigned int& value) noexcept;
        bool _GetOscSetColorTable(const std::wstring_view string,
                                  size_t& tableIndex,
                                  DWORD& rgb) const noexcept;

        static bool s_ParseColorSpec(const std::wstring_view string,
                                     DWORD& rgb) noexcept;

        bool _GetOscSetColor(const std::wstring_view string,
                             DWORD& rgb) const noexcept;

        static constexpr DispatchTypes::CursorStyle DefaultCursorStyle = DispatchTypes::CursorStyle::BlinkingBlockDefault;
        bool _GetCursorStyle(const std::basic_string_view<size_t> parameters,
                             DispatchTypes::CursorStyle& cursorStyle) const noexcept;

        static constexpr size_t DefaultRepeatCount = 1;
        bool _GetRepeatCount(const std::basic_string_view<size_t> parameters,
                             size_t& repeatCount) const noexcept;

        void _ClearLastChar() noexcept;
    };
}
