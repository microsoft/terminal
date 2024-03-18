// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- termDispatch.hpp

Abstract:
- This is a useful default implementation of all of the ITermDispatch callbacks.
    Fails on every callback, but handy for tests.
*/
#include "ITermDispatch.hpp"
#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    class TermDispatch;
};

#pragma warning(disable : 26440) // Making these methods noexcept would require the same of all derived classes

class Microsoft::Console::VirtualTerminal::TermDispatch : public Microsoft::Console::VirtualTerminal::ITermDispatch
{
public:
    void Print(const wchar_t wchPrintable) override = 0;
    void PrintString(const std::wstring_view string) override = 0;

    bool CursorUp(const VTInt /*distance*/) override { return false; } // CUU
    bool CursorDown(const VTInt /*distance*/) override { return false; } // CUD
    bool CursorForward(const VTInt /*distance*/) override { return false; } // CUF
    bool CursorBackward(const VTInt /*distance*/) override { return false; } // CUB, BS
    bool CursorNextLine(const VTInt /*distance*/) override { return false; } // CNL
    bool CursorPrevLine(const VTInt /*distance*/) override { return false; } // CPL
    bool CursorHorizontalPositionAbsolute(const VTInt /*column*/) override { return false; } // HPA, CHA
    bool VerticalLinePositionAbsolute(const VTInt /*line*/) override { return false; } // VPA
    bool HorizontalPositionRelative(const VTInt /*distance*/) override { return false; } // HPR
    bool VerticalPositionRelative(const VTInt /*distance*/) override { return false; } // VPR
    bool CursorPosition(const VTInt /*line*/, const VTInt /*column*/) override { return false; } // CUP, HVP
    bool CursorSaveState() override { return false; } // DECSC
    bool CursorRestoreState() override { return false; } // DECRC
    bool InsertCharacter(const VTInt /*count*/) override { return false; } // ICH
    bool DeleteCharacter(const VTInt /*count*/) override { return false; } // DCH
    bool ScrollUp(const VTInt /*distance*/) override { return false; } // SU
    bool ScrollDown(const VTInt /*distance*/) override { return false; } // SD
    bool InsertLine(const VTInt /*distance*/) override { return false; } // IL
    bool DeleteLine(const VTInt /*distance*/) override { return false; } // DL
    bool InsertColumn(const VTInt /*distance*/) override { return false; } // DECIC
    bool DeleteColumn(const VTInt /*distance*/) override { return false; } // DECDC
    bool SetKeypadMode(const bool /*applicationMode*/) override { return false; } // DECKPAM, DECKPNM
    bool SetAnsiMode(const bool /*ansiMode*/) override { return false; } // DECANM
    bool SetTopBottomScrollingMargins(const VTInt /*topMargin*/, const VTInt /*bottomMargin*/) override { return false; } // DECSTBM
    bool SetLeftRightScrollingMargins(const VTInt /*leftMargin*/, const VTInt /*rightMargin*/) override { return false; } // DECSLRM
    bool WarningBell() override { return false; } // BEL
    bool CarriageReturn() override { return false; } // CR
    bool LineFeed(const DispatchTypes::LineFeedType /*lineFeedType*/) override { return false; } // IND, NEL, LF, FF, VT
    bool ReverseLineFeed() override { return false; } // RI
    bool BackIndex() override { return false; } // DECBI
    bool ForwardIndex() override { return false; } // DECFI
    bool SetWindowTitle(std::wstring_view /*title*/) override { return false; } // DECSWT, OscWindowTitle
    bool HorizontalTabSet() override { return false; } // HTS
    bool ForwardTab(const VTInt /*numTabs*/) override { return false; } // CHT, HT
    bool BackwardsTab(const VTInt /*numTabs*/) override { return false; } // CBT
    bool TabClear(const DispatchTypes::TabClearType /*clearType*/) override { return false; } // TBC
    bool TabSet(const VTParameter /*setType*/) override { return false; } // DECST8C
    bool SetColorTableEntry(const size_t /*tableIndex*/, const DWORD /*color*/) override { return false; } // OSCColorTable
    bool SetDefaultForeground(const DWORD /*color*/) override { return false; } // OSCDefaultForeground
    bool SetDefaultBackground(const DWORD /*color*/) override { return false; } // OSCDefaultBackground
    bool AssignColor(const DispatchTypes::ColorItem /*item*/, const VTInt /*fgIndex*/, const VTInt /*bgIndex*/) override { return false; } // DECAC

    bool EraseInDisplay(const DispatchTypes::EraseType /* eraseType*/) override { return false; } // ED
    bool EraseInLine(const DispatchTypes::EraseType /* eraseType*/) override { return false; } // EL
    bool EraseCharacters(const VTInt /*numChars*/) override { return false; } // ECH
    bool SelectiveEraseInDisplay(const DispatchTypes::EraseType /*eraseType*/) override { return false; } // DECSED
    bool SelectiveEraseInLine(const DispatchTypes::EraseType /*eraseType*/) override { return false; } // DECSEL

    bool ChangeAttributesRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/, const VTParameters /*attrs*/) override { return false; } // DECCARA
    bool ReverseAttributesRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/, const VTParameters /*attrs*/) override { return false; } // DECRARA
    bool CopyRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/, const VTInt /*page*/, const VTInt /*dstTop*/, const VTInt /*dstLeft*/, const VTInt /*dstPage*/) override { return false; } // DECCRA
    bool FillRectangularArea(const VTParameter /*ch*/, const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/) override { return false; } // DECFRA
    bool EraseRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/) override { return false; } // DECERA
    bool SelectiveEraseRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/) override { return false; } // DECSERA
    bool SelectAttributeChangeExtent(const DispatchTypes::ChangeExtent /*changeExtent*/) override { return false; } // DECSACE
    bool RequestChecksumRectangularArea(const VTInt /*id*/, const VTInt /*page*/, const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/) override { return false; } // DECRQCRA

    bool SetGraphicsRendition(const VTParameters /*options*/) override { return false; } // SGR
    bool SetLineRendition(const LineRendition /*rendition*/) override { return false; } // DECSWL, DECDWL, DECDHL
    bool SetCharacterProtectionAttribute(const VTParameters /*options*/) override { return false; } // DECSCA

    bool PushGraphicsRendition(const VTParameters /*options*/) override { return false; } // XTPUSHSGR
    bool PopGraphicsRendition() override { return false; } // XTPOPSGR

    bool SetMode(const DispatchTypes::ModeParams /*param*/) override { return false; } // SM, DECSET
    bool ResetMode(const DispatchTypes::ModeParams /*param*/) override { return false; } // RM, DECRST
    bool RequestMode(const DispatchTypes::ModeParams /*param*/) override { return false; } // DECRQM

    bool DeviceStatusReport(const DispatchTypes::StatusType /*statusType*/, const VTParameter /*id*/) override { return false; } // DSR
    bool DeviceAttributes() override { return false; } // DA1
    bool SecondaryDeviceAttributes() override { return false; } // DA2
    bool TertiaryDeviceAttributes() override { return false; } // DA3
    bool Vt52DeviceAttributes() override { return false; } // VT52 Identify
    bool RequestTerminalParameters(const DispatchTypes::ReportingPermission /*permission*/) override { return false; } // DECREQTPARM

    bool DesignateCodingSystem(const VTID /*codingSystem*/) override { return false; } // DOCS
    bool Designate94Charset(const VTInt /*gsetNumber*/, const VTID /*charset*/) override { return false; } // SCS
    bool Designate96Charset(const VTInt /*gsetNumber*/, const VTID /*charset*/) override { return false; } // SCS
    bool LockingShift(const VTInt /*gsetNumber*/) override { return false; } // LS0, LS1, LS2, LS3
    bool LockingShiftRight(const VTInt /*gsetNumber*/) override { return false; } // LS1R, LS2R, LS3R
    bool SingleShift(const VTInt /*gsetNumber*/) override { return false; } // SS2, SS3
    bool AcceptC1Controls(const bool /*enabled*/) override { return false; } // DECAC1
    bool AnnounceCodeStructure(const VTInt /*ansiLevel*/) override { return false; } // ACS

    bool SoftReset() override { return false; } // DECSTR
    bool HardReset() override { return false; } // RIS
    bool ScreenAlignmentPattern() override { return false; } // DECALN

    bool SetCursorStyle(const DispatchTypes::CursorStyle /*cursorStyle*/) override { return false; } // DECSCUSR
    bool SetCursorColor(const COLORREF /*color*/) override { return false; } // OSCSetCursorColor, OSCResetCursorColor

    bool SetClipboard(std::wstring_view /*content*/) override { return false; } // OscSetClipboard

    // DTTERM_WindowManipulation
    bool WindowManipulation(const DispatchTypes::WindowManipulationType /*function*/,
                            const VTParameter /*parameter1*/,
                            const VTParameter /*parameter2*/) override { return false; }

    bool AddHyperlink(const std::wstring_view /*uri*/, const std::wstring_view /*params*/) override { return false; }
    bool EndHyperlink() override { return false; }

    bool DoConEmuAction(const std::wstring_view /*string*/) override { return false; }

    bool DoITerm2Action(const std::wstring_view /*string*/) override { return false; }

    bool DoFinalTermAction(const std::wstring_view /*string*/) override { return false; }

    bool DoVsCodeAction(const std::wstring_view /*string*/) override { return false; }

    StringHandler DownloadDRCS(const VTInt /*fontNumber*/,
                               const VTParameter /*startChar*/,
                               const DispatchTypes::DrcsEraseControl /*eraseControl*/,
                               const DispatchTypes::DrcsCellMatrix /*cellMatrix*/,
                               const DispatchTypes::DrcsFontSet /*fontSet*/,
                               const DispatchTypes::DrcsFontUsage /*fontUsage*/,
                               const VTParameter /*cellHeight*/,
                               const DispatchTypes::CharsetSize /*charsetSize*/) override { return nullptr; } // DECDLD

    bool RequestUserPreferenceCharset() override { return false; } // DECRQUPSS
    StringHandler AssignUserPreferenceCharset(const DispatchTypes::CharsetSize /*charsetSize*/) override { return nullptr; } // DECAUPSS

    StringHandler DefineMacro(const VTInt /*macroId*/,
                              const DispatchTypes::MacroDeleteControl /*deleteControl*/,
                              const DispatchTypes::MacroEncoding /*encoding*/) override { return nullptr; } // DECDMAC
    bool InvokeMacro(const VTInt /*macroId*/) override { return false; } // DECINVM

    StringHandler RestoreTerminalState(const DispatchTypes::ReportFormat /*format*/) override { return nullptr; }; // DECRSTS

    StringHandler RequestSetting() override { return nullptr; }; // DECRQSS

    bool RequestPresentationStateReport(const DispatchTypes::PresentationReportFormat /*format*/) override { return false; } // DECRQPSR
    StringHandler RestorePresentationState(const DispatchTypes::PresentationReportFormat /*format*/) override { return nullptr; } // DECRSPS

    bool PlaySounds(const VTParameters /*parameters*/) override { return false; }; // DECPS
};

#pragma warning(default : 26440) // Restore "can be declared noexcept" warning
