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

        bool ActionCsiDispatch(const VTID id, const gsl::span<const size_t> parameters) override;

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
            ST_StringTerminator = VTID("\\"),
            RIS_ResetToInitialState = VTID("c"),
            LS2_LockingShift = VTID("n"),
            LS3_LockingShift = VTID("o"),
            LS1R_LockingShift = VTID("~"),
            LS2R_LockingShift = VTID("}"),
            LS3R_LockingShift = VTID("|"),
            DECALN_ScreenAlignmentPattern = VTID("#8")
        };

        enum CsiActionCodes : uint64_t
        {
            ICH_InsertCharacter = VTID("@"),
            CUU_CursorUp = VTID("A"),
            CUD_CursorDown = VTID("B"),
            CUF_CursorForward = VTID("C"),
            CUB_CursorBackward = VTID("D"),
            CNL_CursorNextLine = VTID("E"),
            CPL_CursorPrevLine = VTID("F"),
            CHA_CursorHorizontalAbsolute = VTID("G"),
            CUP_CursorPosition = VTID("H"),
            CHT_CursorForwardTab = VTID("I"),
            ED_EraseDisplay = VTID("J"),
            EL_EraseLine = VTID("K"),
            IL_InsertLine = VTID("L"),
            DL_DeleteLine = VTID("M"),
            DCH_DeleteCharacter = VTID("P"),
            SU_ScrollUp = VTID("S"),
            SD_ScrollDown = VTID("T"),
            ECH_EraseCharacters = VTID("X"),
            CBT_CursorBackTab = VTID("Z"),
            HPA_HorizontalPositionAbsolute = VTID("`"),
            HPR_HorizontalPositionRelative = VTID("a"),
            REP_RepeatCharacter = VTID("b"),
            DA_DeviceAttributes = VTID("c"),
            DA2_SecondaryDeviceAttributes = VTID(">c"),
            DA3_TertiaryDeviceAttributes = VTID("=c"),
            VPA_VerticalLinePositionAbsolute = VTID("d"),
            VPR_VerticalPositionRelative = VTID("e"),
            HVP_HorizontalVerticalPosition = VTID("f"),
            TBC_TabClear = VTID("g"),
            DECSET_PrivateModeSet = VTID("?h"),
            DECRST_PrivateModeReset = VTID("?l"),
            SGR_SetGraphicsRendition = VTID("m"),
            DSR_DeviceStatusReport = VTID("n"),
            DECSTBM_SetScrollingRegion = VTID("r"),
            ANSISYSSC_CursorSave = VTID("s"), // NOTE: Overlaps with DECLRMM/DECSLRM. Fix when/if implemented.
            DTTERM_WindowManipulation = VTID("t"), // NOTE: Overlaps with DECSLPP. Fix when/if implemented.
            ANSISYSRC_CursorRestore = VTID("u"),
            DECSCUSR_SetCursorStyle = VTID(" q"),
            DECSTR_SoftReset = VTID("!p"),
            DECSCPP_SetColumnsPerPage = VTID("$|")
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
            Hyperlink = 8,
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

        static constexpr DispatchTypes::CursorStyle DefaultCursorStyle = DispatchTypes::CursorStyle::UserDefault;
        bool _GetCursorStyle(const gsl::span<const size_t> parameters,
                             DispatchTypes::CursorStyle& cursorStyle) const noexcept;

        static constexpr size_t DefaultRepeatCount = 1;
        bool _GetRepeatCount(const gsl::span<const size_t> parameters,
                             size_t& repeatCount) const noexcept;

        bool _GetOscSetClipboard(const std::wstring_view string,
                                 std::wstring& content,
                                 bool& queryClipboard) const noexcept;

        static constexpr std::wstring_view hyperlinkIDParameter{ L"id=" };
        bool _ParseHyperlink(const std::wstring_view string,
                             std::wstring& params,
                             std::wstring& uri) const;

        void _ClearLastChar() noexcept;
    };
}
