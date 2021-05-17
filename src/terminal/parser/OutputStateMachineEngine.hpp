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

        bool ActionVt52EscDispatch(const VTID id, const VTParameters parameters) override;

        bool ActionCsiDispatch(const VTID id, const VTParameters parameters) override;

        StringHandler ActionDcsDispatch(const VTID id, const VTParameters parameters) noexcept override;

        bool ActionClear() noexcept override;

        bool ActionIgnore() noexcept override;

        bool ActionOscDispatch(const wchar_t wch,
                               const size_t parameter,
                               const std::wstring_view string) override;

        bool ActionSs3Dispatch(const wchar_t wch, const VTParameters parameters) noexcept override;

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
            DECID_IdentifyDevice = VTID("Z"),
            ST_StringTerminator = VTID("\\"),
            RIS_ResetToInitialState = VTID("c"),
            LS2_LockingShift = VTID("n"),
            LS3_LockingShift = VTID("o"),
            LS1R_LockingShift = VTID("~"),
            LS2R_LockingShift = VTID("}"),
            LS3R_LockingShift = VTID("|"),
            DECDHL_DoubleHeightLineTop = VTID("#3"),
            DECDHL_DoubleHeightLineBottom = VTID("#4"),
            DECSWL_SingleWidthLine = VTID("#5"),
            DECDWL_DoubleWidthLine = VTID("#6"),
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
            DECREQTPARM_RequestTerminalParameters = VTID("x"),
            DECSCUSR_SetCursorStyle = VTID(" q"),
            DECSTR_SoftReset = VTID("!p"),
            XT_PushSgrAlias = VTID("#p"),
            XT_PopSgrAlias = VTID("#q"),
            XT_PushSgr = VTID("#{"),
            XT_PopSgr = VTID("#}"),
            DECSCPP_SetColumnsPerPage = VTID("$|"),
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
            ConEmuAction = 9,
            SetForegroundColor = 10,
            SetBackgroundColor = 11,
            SetCursorColor = 12,
            SetClipboard = 52,
            ResetForegroundColor = 110, // Not implemented
            ResetBackgroundColor = 111, // Not implemented
            ResetCursorColor = 112
        };

        bool _GetOscTitle(const std::wstring_view string,
                          std::wstring& title) const;

        bool _GetOscSetColorTable(const std::wstring_view string,
                                  std::vector<size_t>& tableIndexes,
                                  std::vector<DWORD>& rgbs) const noexcept;

        bool _GetOscSetColor(const std::wstring_view string,
                             std::vector<DWORD>& rgbs) const noexcept;

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
