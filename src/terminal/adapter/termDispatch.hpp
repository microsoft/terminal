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
    bool CursorVisibility(const bool /*isVisible*/) override { return false; } // DECTCEM
    bool InsertCharacter(const VTInt /*count*/) override { return false; } // ICH
    bool DeleteCharacter(const VTInt /*count*/) override { return false; } // DCH
    bool ScrollUp(const VTInt /*distance*/) override { return false; } // SU
    bool ScrollDown(const VTInt /*distance*/) override { return false; } // SD
    bool InsertLine(const VTInt /*distance*/) override { return false; } // IL
    bool DeleteLine(const VTInt /*distance*/) override { return false; } // DL
    bool SetColumns(const VTInt /*columns*/) override { return false; } // DECCOLM
    bool SetCursorKeysMode(const bool /*applicationMode*/) override { return false; } // DECCKM
    bool SetKeypadMode(const bool /*applicationMode*/) override { return false; } // DECKPAM, DECKPNM
    bool EnableWin32InputMode(const bool /*win32InputMode*/) override { return false; } // win32-input-mode
    bool EnableCursorBlinking(const bool /*enable*/) override { return false; } // ATT610
    bool SetAnsiMode(const bool /*ansiMode*/) override { return false; } // DECANM
    bool SetScreenMode(const bool /*reverseMode*/) override { return false; } // DECSCNM
    bool SetOriginMode(const bool /*relativeMode*/) override { return false; }; // DECOM
    bool SetAutoWrapMode(const bool /*wrapAtEOL*/) override { return false; }; // DECAWM
    bool SetTopBottomScrollingMargins(const VTInt /*topMargin*/, const VTInt /*bottomMargin*/) override { return false; } // DECSTBM
    bool WarningBell() override { return false; } // BEL
    bool CarriageReturn() override { return false; } // CR
    bool LineFeed(const DispatchTypes::LineFeedType /*lineFeedType*/) override { return false; } // IND, NEL, LF, FF, VT
    bool ReverseLineFeed() override { return false; } // RI
    bool SetWindowTitle(std::wstring_view /*title*/) override { return false; } // OscWindowTitle
    bool UseAlternateScreenBuffer() override { return false; } // ASBSET
    bool UseMainScreenBuffer() override { return false; } // ASBRST
    bool HorizontalTabSet() override { return false; } // HTS
    bool ForwardTab(const VTInt /*numTabs*/) override { return false; } // CHT, HT
    bool BackwardsTab(const VTInt /*numTabs*/) override { return false; } // CBT
    bool TabClear(const DispatchTypes::TabClearType /*clearType*/) override { return false; } // TBC
    bool EnableDECCOLMSupport(const bool /*enabled*/) override { return false; } // ?40
    bool EnableVT200MouseMode(const bool /*enabled*/) override { return false; } // ?1000
    bool EnableUTF8ExtendedMouseMode(const bool /*enabled*/) override { return false; } // ?1005
    bool EnableSGRExtendedMouseMode(const bool /*enabled*/) override { return false; } // ?1006
    bool EnableButtonEventMouseMode(const bool /*enabled*/) override { return false; } // ?1002
    bool EnableAnyEventMouseMode(const bool /*enabled*/) override { return false; } // ?1003
    bool EnableFocusEventMode(const bool /*enabled*/) override { return false; } // ?1004
    bool EnableAlternateScroll(const bool /*enabled*/) override { return false; } // ?1007
    bool EnableXtermBracketedPasteMode(const bool /*enabled*/) override { return false; } // ?2004
    bool SetColorTableEntry(const size_t /*tableIndex*/, const DWORD /*color*/) override { return false; } // OSCColorTable
    bool SetDefaultForeground(const DWORD /*color*/) override { return false; } // OSCDefaultForeground
    bool SetDefaultBackground(const DWORD /*color*/) override { return false; } // OSCDefaultBackground
    bool AssignColor(const DispatchTypes::ColorItem /*item*/, const VTInt /*fgIndex*/, const VTInt /*bgIndex*/) override { return false; } // DECAC

    bool EraseInDisplay(const DispatchTypes::EraseType /* eraseType*/) override { return false; } // ED
    bool EraseInLine(const DispatchTypes::EraseType /* eraseType*/) override { return false; } // EL
    bool EraseCharacters(const VTInt /*numChars*/) override { return false; } // ECH

    bool SetGraphicsRendition(const VTParameters /*options*/) override { return false; } // SGR
    bool SetLineRendition(const LineRendition /*rendition*/) override { return false; } // DECSWL, DECDWL, DECDHL

    bool PushGraphicsRendition(const VTParameters /*options*/) override { return false; } // XTPUSHSGR
    bool PopGraphicsRendition() override { return false; } // XTPOPSGR

    bool SetMode(const DispatchTypes::ModeParams /*param*/) override { return false; } // DECSET

    bool ResetMode(const DispatchTypes::ModeParams /*param*/) override { return false; } // DECRST

    bool DeviceStatusReport(const DispatchTypes::AnsiStatusType /*statusType*/) override { return false; } // DSR, DSR-OS, DSR-CPR
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

    StringHandler DownloadDRCS(const VTInt /*fontNumber*/,
                               const VTParameter /*startChar*/,
                               const DispatchTypes::DrcsEraseControl /*eraseControl*/,
                               const DispatchTypes::DrcsCellMatrix /*cellMatrix*/,
                               const DispatchTypes::DrcsFontSet /*fontSet*/,
                               const DispatchTypes::DrcsFontUsage /*fontUsage*/,
                               const VTParameter /*cellHeight*/,
                               const DispatchTypes::DrcsCharsetSize /*charsetSize*/) override { return nullptr; } // DECDLD

    StringHandler RestoreTerminalState(const DispatchTypes::ReportFormat /*format*/) override { return nullptr; }; // DECRSTS

    StringHandler RequestSetting() override { return nullptr; }; // DECRQSS

    bool PlaySounds(const VTParameters /*parameters*/) override { return false; }; // DECPS
};

#pragma warning(default : 26440) // Restore "can be declared noexcept" warning
