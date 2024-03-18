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
#include "IStateMachineEngine.hpp"

namespace Microsoft::Console::Render
{
    class VtEngine;
}

namespace Microsoft::Console::VirtualTerminal
{
    class OutputStateMachineEngine : public IStateMachineEngine
    {
    public:
        static constexpr size_t MAX_URL_LENGTH = 2 * 1048576; // 2MB, like iTerm2

        OutputStateMachineEngine(std::unique_ptr<ITermDispatch> pDispatch);

        bool EncounteredWin32InputModeSequence() const noexcept override;

        bool ActionExecute(const wchar_t wch) override;
        bool ActionExecuteFromEscape(const wchar_t wch) override;

        bool ActionPrint(const wchar_t wch) override;

        bool ActionPrintString(const std::wstring_view string) override;

        bool ActionPassThroughString(const std::wstring_view string) override;

        bool ActionEscDispatch(const VTID id) override;

        bool ActionVt52EscDispatch(const VTID id, const VTParameters parameters) override;

        bool ActionCsiDispatch(const VTID id, const VTParameters parameters) override;

        StringHandler ActionDcsDispatch(const VTID id, const VTParameters parameters) override;

        bool ActionClear() noexcept override;

        bool ActionIgnore() noexcept override;

        bool ActionOscDispatch(const size_t parameter, const std::wstring_view string) override;

        bool ActionSs3Dispatch(const wchar_t wch, const VTParameters parameters) noexcept override;

        void SetTerminalConnection(Microsoft::Console::Render::VtEngine* const pTtyConnection,
                                   std::function<bool()> pfnFlushToTerminal);

        const ITermDispatch& Dispatch() const noexcept;
        ITermDispatch& Dispatch() noexcept;

    private:
        std::unique_ptr<ITermDispatch> _dispatch;
        Microsoft::Console::Render::VtEngine* _pTtyConnection;
        std::function<bool()> _pfnFlushToTerminal;
        wchar_t _lastPrintedChar;

        enum EscActionCodes : uint64_t
        {
            DECBI_BackIndex = VTID("6"),
            DECSC_CursorSave = VTID("7"),
            DECRC_CursorRestore = VTID("8"),
            DECFI_ForwardIndex = VTID("9"),
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
            DECAC1_AcceptC1Controls = VTID(" 7"),
            ACS_AnsiLevel1 = VTID(" L"),
            ACS_AnsiLevel2 = VTID(" M"),
            ACS_AnsiLevel3 = VTID(" N"),
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
            DECSED_SelectiveEraseDisplay = VTID("?J"),
            EL_EraseLine = VTID("K"),
            DECSEL_SelectiveEraseLine = VTID("?K"),
            IL_InsertLine = VTID("L"),
            DL_DeleteLine = VTID("M"),
            DCH_DeleteCharacter = VTID("P"),
            SU_ScrollUp = VTID("S"),
            SD_ScrollDown = VTID("T"),
            DECST8C_SetTabEvery8Columns = VTID("?W"),
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
            SM_SetMode = VTID("h"),
            DECSET_PrivateModeSet = VTID("?h"),
            RM_ResetMode = VTID("l"),
            DECRST_PrivateModeReset = VTID("?l"),
            SGR_SetGraphicsRendition = VTID("m"),
            DSR_DeviceStatusReport = VTID("n"),
            DSR_PrivateDeviceStatusReport = VTID("?n"),
            DECSTBM_SetTopBottomMargins = VTID("r"),
            DECSLRM_SetLeftRightMargins = VTID("s"),
            DTTERM_WindowManipulation = VTID("t"), // NOTE: Overlaps with DECSLPP. Fix when/if implemented.
            ANSISYSRC_CursorRestore = VTID("u"),
            DECREQTPARM_RequestTerminalParameters = VTID("x"),
            DECSCUSR_SetCursorStyle = VTID(" q"),
            DECSTR_SoftReset = VTID("!p"),
            DECSCA_SetCharacterProtectionAttribute = VTID("\"q"),
            XT_PushSgrAlias = VTID("#p"),
            XT_PopSgrAlias = VTID("#q"),
            XT_PushSgr = VTID("#{"),
            XT_PopSgr = VTID("#}"),
            DECRQM_RequestMode = VTID("$p"),
            DECRQM_PrivateRequestMode = VTID("?$p"),
            DECCARA_ChangeAttributesRectangularArea = VTID("$r"),
            DECRARA_ReverseAttributesRectangularArea = VTID("$t"),
            DECCRA_CopyRectangularArea = VTID("$v"),
            DECRQPSR_RequestPresentationStateReport = VTID("$w"),
            DECFRA_FillRectangularArea = VTID("$x"),
            DECERA_EraseRectangularArea = VTID("$z"),
            DECSERA_SelectiveEraseRectangularArea = VTID("${"),
            DECSCPP_SetColumnsPerPage = VTID("$|"),
            DECRQUPSS_RequestUserPreferenceSupplementalSet = VTID("&u"),
            DECIC_InsertColumn = VTID("'}"),
            DECDC_DeleteColumn = VTID("'~"),
            DECSACE_SelectAttributeChangeExtent = VTID("*x"),
            DECRQCRA_RequestChecksumRectangularArea = VTID("*y"),
            DECINVM_InvokeMacro = VTID("*z"),
            DECAC_AssignColor = VTID(",|"),
            DECPS_PlaySound = VTID(",~")
        };

        enum DcsActionCodes : uint64_t
        {
            DECDLD_DownloadDRCS = VTID("{"),
            DECAUPSS_AssignUserPreferenceSupplementalSet = VTID("!u"),
            DECDMAC_DefineMacro = VTID("!z"),
            DECRSTS_RestoreTerminalState = VTID("$p"),
            DECRQSS_RequestSetting = VTID("$q"),
            DECRSPS_RestorePresentationState = VTID("$t"),
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
            DECSWT_SetWindowTitle = 21,
            SetClipboard = 52,
            ResetForegroundColor = 110, // Not implemented
            ResetBackgroundColor = 111, // Not implemented
            ResetCursorColor = 112,
            FinalTermAction = 133,
            VsCodeAction = 633,
            ITerm2Action = 1337,
        };

        bool _GetOscSetColorTable(const std::wstring_view string,
                                  std::vector<size_t>& tableIndexes,
                                  std::vector<DWORD>& rgbs) const;

        bool _GetOscSetColor(const std::wstring_view string,
                             std::vector<DWORD>& rgbs) const;

        bool _GetOscSetClipboard(const std::wstring_view string,
                                 std::wstring& content,
                                 bool& queryClipboard) const noexcept;

        static constexpr std::wstring_view hyperlinkIDParameter{ L"id=" };
        bool _ParseHyperlink(const std::wstring_view string,
                             std::wstring& params,
                             std::wstring& uri) const;

        bool _CanSeqAcceptSubParam(const VTID id, const VTParameters& parameters) noexcept;

        void _ClearLastChar() noexcept;
    };
}
