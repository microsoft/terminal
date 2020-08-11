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

        bool ActionEscDispatch(const VTID id) override;

        bool ActionVt52EscDispatch(const VTID id, const gsl::span<const size_t> parameters) override;

        bool ActionCsiDispatch(const wchar_t wch,
                               const gsl::span<const wchar_t> intermediates,
                               const gsl::span<const size_t> parameters) override;

        bool ActionClear() noexcept override;

        bool ActionIgnore() noexcept override;

        bool ActionOscDispatch(const wchar_t wch,
                               const size_t parameter,
                               const std::wstring_view string) override;

        bool ActionSs3Dispatch(const wchar_t wch,
                               const gsl::span<const size_t> parameters) noexcept override;

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

        bool _IntermediateQuestionMarkDispatch(const wchar_t wchAction,
                                               const gsl::span<const size_t> parameters);
        bool _IntermediateGreaterThanOrEqualDispatch(const wchar_t wch,
                                                     const wchar_t intermediate,
                                                     const gsl::span<const size_t> parameters);
        bool _IntermediateExclamationDispatch(const wchar_t wch);
        bool _IntermediateSpaceDispatch(const wchar_t wchAction,
                                        const gsl::span<const size_t> parameters);

        enum EscActionCodes : uint64_t
        {
            DECSC_CursorSave = VTID("7"),
            DECRC_CursorRestore = VTID("8"),
            DECKPAM_KeypadApplicationMode = VTID("="),
            DECKPNM_KeypadNumericMode = VTID(">"),
            IND_Index = VTID("D"),
            NEL_NextLine = VTID("E"),
            HTS_HorizontalTabSet = VTID("H"),
            RI_ReverseLineFeed = VTID("M"),
            SS2_SingleShift = VTID("N"),
            SS3_SingleShift = VTID("O"),
            RIS_ResetToInitialState = VTID("c"),
            LS2_LockingShift = VTID("n"),
            LS3_LockingShift = VTID("o"),
            LS1R_LockingShift = VTID("~"),
            LS2R_LockingShift = VTID("}"),
            LS3R_LockingShift = VTID("|"),
            DECALN_ScreenAlignmentPattern = VTID("#8")
        };

        enum CsiActionCodes : wchar_t
        {
            ICH_InsertCharacter = L'@',
            CUU_CursorUp = L'A',
            CUD_CursorDown = L'B',
            CUF_CursorForward = L'C',
            CUB_CursorBackward = L'D',
            CNL_CursorNextLine = L'E',
            CPL_CursorPrevLine = L'F',
            CHA_CursorHorizontalAbsolute = L'G',
            CUP_CursorPosition = L'H',
            CHT_CursorForwardTab = L'I',
            ED_EraseDisplay = L'J',
            EL_EraseLine = L'K',
            IL_InsertLine = L'L',
            DL_DeleteLine = L'M',
            DCH_DeleteCharacter = L'P',
            SU_ScrollUp = L'S',
            SD_ScrollDown = L'T',
            ECH_EraseCharacters = L'X',
            CBT_CursorBackTab = L'Z',
            HPA_HorizontalPositionAbsolute = L'`',
            HPR_HorizontalPositionRelative = L'a',
            REP_RepeatCharacter = L'b',
            DA_DeviceAttributes = L'c',
            VPA_VerticalLinePositionAbsolute = L'd',
            VPR_VerticalPositionRelative = L'e',
            HVP_HorizontalVerticalPosition = L'f',
            TBC_TabClear = L'g',
            DECSET_PrivateModeSet = L'h',
            DECRST_PrivateModeReset = L'l',
            SGR_SetGraphicsRendition = L'm',
            DSR_DeviceStatusReport = L'n',
            DECSTBM_SetScrollingRegion = L'r',
            ANSISYSSC_CursorSave = L's', // NOTE: Overlaps with DECLRMM/DECSLRM. Fix when/if implemented.
            DTTERM_WindowManipulation = L't', // NOTE: Overlaps with DECSLPP. Fix when/if implemented.
            ANSISYSRC_CursorRestore = L'u', // NOTE: Overlaps with DECSMBV. Fix when/if implemented.
            DECSCUSR_SetCursorStyle = L'q', // With SP intermediate
            DECSTR_SoftReset = L'p', // With ! intermediate
            DECSCPP_SetColumnsPerPage = L'|' // With $ intermediate
        };

        enum Vt52ActionCodes : uint64_t
        {
            CursorUp = VTID("A"),
            CursorDown = VTID("B"),
            CursorRight = VTID("C"),
            CursorLeft = VTID("D"),
            EnterGraphicsMode = VTID("F"),
            ExitGraphicsMode = VTID("G"),
            CursorToHome = VTID("H"),
            ReverseLineFeed = VTID("I"),
            EraseToEndOfScreen = VTID("J"),
            EraseToEndOfLine = VTID("K"),
            DirectCursorAddress = VTID("Y"),
            Identify = VTID("Z"),
            EnterAlternateKeypadMode = VTID("="),
            ExitAlternateKeypadMode = VTID(">"),
            ExitVt52Mode = VTID("<")
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
            SetClipboard = 52,
            ResetForegroundColor = 110, // Not implemented
            ResetBackgroundColor = 111, // Not implemented
            ResetCursorColor = 112
        };

        static constexpr DispatchTypes::GraphicsOptions DefaultGraphicsOption = DispatchTypes::GraphicsOptions::Off;
        bool _GetGraphicsOptions(const gsl::span<const size_t> parameters,
                                 std::vector<DispatchTypes::GraphicsOptions>& options) const;

        static constexpr DispatchTypes::EraseType DefaultEraseType = DispatchTypes::EraseType::ToEnd;
        bool _GetEraseOperation(const gsl::span<const size_t> parameters,
                                DispatchTypes::EraseType& eraseType) const noexcept;

        static constexpr size_t DefaultCursorDistance = 1;
        bool _GetCursorDistance(const gsl::span<const size_t> parameters,
                                size_t& distance) const noexcept;

        static constexpr size_t DefaultScrollDistance = 1;
        bool _GetScrollDistance(const gsl::span<const size_t> parameters,
                                size_t& distance) const noexcept;

        static constexpr size_t DefaultConsoleWidth = 80;
        bool _GetConsoleWidth(const gsl::span<const size_t> parameters,
                              size_t& consoleWidth) const noexcept;

        static constexpr size_t DefaultLine = 1;
        static constexpr size_t DefaultColumn = 1;
        bool _GetXYPosition(const gsl::span<const size_t> parameters,
                            size_t& line,
                            size_t& column) const noexcept;

        bool _GetDeviceStatusOperation(const gsl::span<const size_t> parameters,
                                       DispatchTypes::AnsiStatusType& statusType) const noexcept;

        bool _VerifyHasNoParameters(const gsl::span<const size_t> parameters) const noexcept;

        bool _VerifyDeviceAttributesParams(const gsl::span<const size_t> parameters) const noexcept;

        bool _GetPrivateModeParams(const gsl::span<const size_t> parameters,
                                   std::vector<DispatchTypes::PrivateModeParams>& privateModes) const;

        static constexpr size_t DefaultTopMargin = 0;
        static constexpr size_t DefaultBottomMargin = 0;
        bool _GetTopBottomMargins(const gsl::span<const size_t> parameters,
                                  size_t& topMargin,
                                  size_t& bottomMargin) const noexcept;

        bool _GetOscTitle(const std::wstring_view string,
                          std::wstring& title) const;

        static constexpr size_t DefaultTabDistance = 1;
        bool _GetTabDistance(const gsl::span<const size_t> parameters,
                             size_t& distance) const noexcept;

        static constexpr size_t DefaultTabClearType = 0;
        bool _GetTabClearType(const gsl::span<const size_t> parameters,
                              size_t& clearType) const noexcept;

        static constexpr DispatchTypes::WindowManipulationType DefaultWindowManipulationType = DispatchTypes::WindowManipulationType::Invalid;
        bool _GetWindowManipulationType(const gsl::span<const size_t> parameters,
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
        bool _GetCursorStyle(const gsl::span<const size_t> parameters,
                             DispatchTypes::CursorStyle& cursorStyle) const noexcept;

        static constexpr size_t DefaultRepeatCount = 1;
        bool _GetRepeatCount(const gsl::span<const size_t> parameters,
                             size_t& repeatCount) const noexcept;

        bool _GetOscSetClipboard(const std::wstring_view string,
                                 std::wstring& content,
                                 bool& queryClipboard) const noexcept;

        void _ClearLastChar() noexcept;
    };
}
