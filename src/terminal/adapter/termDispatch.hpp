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

class Microsoft::Console::VirtualTerminal::TermDispatch : public Microsoft::Console::VirtualTerminal::ITermDispatch
{
public:
    void Execute(const wchar_t wchControl) override = 0;
    void Print(const wchar_t wchPrintable) override = 0;
    void PrintString(const std::wstring_view string) override = 0;

    bool CursorUp(const size_t /*distance*/) noexcept override { return false; } // CUU
    bool CursorDown(const size_t /*distance*/) noexcept override { return false; } // CUD
    bool CursorForward(const size_t /*distance*/) noexcept override { return false; } // CUF
    bool CursorBackward(const size_t /*distance*/) noexcept override { return false; } // CUB, BS
    bool CursorNextLine(const size_t /*distance*/) noexcept override { return false; } // CNL
    bool CursorPrevLine(const size_t /*distance*/) noexcept override { return false; } // CPL
    bool CursorHorizontalPositionAbsolute(const size_t /*column*/) noexcept override { return false; } // HPA, CHA
    bool VerticalLinePositionAbsolute(const size_t /*line*/) noexcept override { return false; } // VPA
    bool HorizontalPositionRelative(const size_t /*distance*/) noexcept override { return false; } // HPR
    bool VerticalPositionRelative(const size_t /*distance*/) noexcept override { return false; } // VPR
    bool CursorPosition(const size_t /*line*/, const size_t /*column*/) noexcept override { return false; } // CUP, HVP
    bool CursorSaveState() noexcept override { return false; } // DECSC
    bool CursorRestoreState() noexcept override { return false; } // DECRC
    bool CursorVisibility(const bool /*isVisible*/) noexcept override { return false; } // DECTCEM
    bool InsertCharacter(const size_t /*count*/) noexcept override { return false; } // ICH
    bool DeleteCharacter(const size_t /*count*/) noexcept override { return false; } // DCH
    bool ScrollUp(const size_t /*distance*/) noexcept override { return false; } // SU
    bool ScrollDown(const size_t /*distance*/) noexcept override { return false; } // SD
    bool InsertLine(const size_t /*distance*/) noexcept override { return false; } // IL
    bool DeleteLine(const size_t /*distance*/) noexcept override { return false; } // DL
    bool SetColumns(const size_t /*columns*/) noexcept override { return false; } // DECCOLM
    bool SetCursorKeysMode(const bool /*applicationMode*/) noexcept override { return false; } // DECCKM
    bool SetKeypadMode(const bool /*applicationMode*/) noexcept override { return false; } // DECKPAM, DECKPNM
    bool EnableWin32InputMode(const bool /*win32InputMode*/) noexcept override { return false; } // win32-input-mode
    bool EnableCursorBlinking(const bool /*enable*/) noexcept override { return false; } // ATT610
    bool SetAnsiMode(const bool /*ansiMode*/) noexcept override { return false; } // DECANM
    bool SetScreenMode(const bool /*reverseMode*/) noexcept override { return false; } // DECSCNM
    bool SetOriginMode(const bool /*relativeMode*/) noexcept override { return false; }; // DECOM
    bool SetAutoWrapMode(const bool /*wrapAtEOL*/) noexcept override { return false; }; // DECAWM
    bool SetTopBottomScrollingMargins(const size_t /*topMargin*/, const size_t /*bottomMargin*/) noexcept override { return false; } // DECSTBM
    bool WarningBell() noexcept override { return false; } // BEL
    bool CarriageReturn() noexcept override { return false; } // CR
    bool LineFeed(const DispatchTypes::LineFeedType /*lineFeedType*/) noexcept override { return false; } // IND, NEL, LF, FF, VT
    bool ReverseLineFeed() noexcept override { return false; } // RI
    bool SetWindowTitle(std::wstring_view /*title*/) noexcept override { return false; } // OscWindowTitle
    bool UseAlternateScreenBuffer() noexcept override { return false; } // ASBSET
    bool UseMainScreenBuffer() noexcept override { return false; } // ASBRST
    bool HorizontalTabSet() noexcept override { return false; } // HTS
    bool ForwardTab(const size_t /*numTabs*/) noexcept override { return false; } // CHT, HT
    bool BackwardsTab(const size_t /*numTabs*/) noexcept override { return false; } // CBT
    bool TabClear(const size_t /*clearType*/) noexcept override { return false; } // TBC
    bool EnableDECCOLMSupport(const bool /*enabled*/) noexcept override { return false; } // ?40
    bool EnableVT200MouseMode(const bool /*enabled*/) noexcept override { return false; } // ?1000
    bool EnableUTF8ExtendedMouseMode(const bool /*enabled*/) noexcept override { return false; } // ?1005
    bool EnableSGRExtendedMouseMode(const bool /*enabled*/) noexcept override { return false; } // ?1006
    bool EnableButtonEventMouseMode(const bool /*enabled*/) noexcept override { return false; } // ?1002
    bool EnableAnyEventMouseMode(const bool /*enabled*/) noexcept override { return false; } // ?1003
    bool EnableAlternateScroll(const bool /*enabled*/) noexcept override { return false; } // ?1007
    bool SetColorTableEntry(const size_t /*tableIndex*/, const DWORD /*color*/) noexcept override { return false; } // OSCColorTable
    bool SetDefaultForeground(const DWORD /*color*/) noexcept override { return false; } // OSCDefaultForeground
    bool SetDefaultBackground(const DWORD /*color*/) noexcept override { return false; } // OSCDefaultBackground

    bool EraseInDisplay(const DispatchTypes::EraseType /* eraseType*/) noexcept override { return false; } // ED
    bool EraseInLine(const DispatchTypes::EraseType /* eraseType*/) noexcept override { return false; } // EL
    bool EraseCharacters(const size_t /*numChars*/) noexcept override { return false; } // ECH

    bool SetGraphicsRendition(const std::basic_string_view<DispatchTypes::GraphicsOptions> /*options*/) noexcept override { return false; } // SGR

    bool SetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> /*params*/) noexcept override { return false; } // DECSET

    bool ResetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> /*params*/) noexcept override { return false; } // DECRST

    bool DeviceStatusReport(const DispatchTypes::AnsiStatusType /*statusType*/) noexcept override { return false; } // DSR, DSR-OS, DSR-CPR
    bool DeviceAttributes() noexcept override { return false; } // DA1
    bool Vt52DeviceAttributes() noexcept override { return false; } // VT52 Identify

    bool DesignateCodingSystem(const wchar_t /*codingSystem*/) noexcept override { return false; } // DOCS
    bool Designate94Charset(const size_t /*gsetNumber*/, const std::pair<wchar_t, wchar_t> /*charset*/) noexcept override { return false; } // SCS
    bool Designate96Charset(const size_t /*gsetNumber*/, const std::pair<wchar_t, wchar_t> /*charset*/) noexcept override { return false; } // SCS
    bool LockingShift(const size_t /*gsetNumber*/) noexcept override { return false; } // LS0, LS1, LS2, LS3
    bool LockingShiftRight(const size_t /*gsetNumber*/) noexcept override { return false; } // LS1R, LS2R, LS3R
    bool SingleShift(const size_t /*gsetNumber*/) noexcept override { return false; } // SS2, SS3

    bool SoftReset() noexcept override { return false; } // DECSTR
    bool HardReset() noexcept override { return false; } // RIS
    bool ScreenAlignmentPattern() noexcept override { return false; } // DECALN

    bool SetCursorStyle(const DispatchTypes::CursorStyle /*cursorStyle*/) noexcept override { return false; } // DECSCUSR
    bool SetCursorColor(const COLORREF /*color*/) noexcept override { return false; } // OSCSetCursorColor, OSCResetCursorColor

    // DTTERM_WindowManipulation
    bool WindowManipulation(const DispatchTypes::WindowManipulationType /*function*/,
                            const std::basic_string_view<size_t> /*params*/) noexcept override { return false; }
};
