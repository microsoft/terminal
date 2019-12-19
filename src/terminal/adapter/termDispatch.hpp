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

    bool CursorUp(const size_t /*distance*/) override { return false; } // CUU
    bool CursorDown(const size_t /*distance*/) override { return false; } // CUD
    bool CursorForward(const size_t /*distance*/) override { return false; } // CUF
    bool CursorBackward(const size_t /*distance*/) override { return false; } // CUB
    bool CursorNextLine(const size_t /*distance*/) override { return false; } // CNL
    bool CursorPrevLine(const size_t /*distance*/) override { return false; } // CPL
    bool CursorHorizontalPositionAbsolute(const size_t /*column*/) override { return false; } // CHA
    bool VerticalLinePositionAbsolute(const size_t /*line*/) override { return false; } // VPA
    bool CursorPosition(const size_t /*line*/, const size_t /*column*/) override { return false; } // CUP
    bool CursorSaveState() override { return false; } // DECSC
    bool CursorRestoreState() override { return false; } // DECRC
    bool CursorVisibility(const bool /*isVisible*/) override { return false; } // DECTCEM
    bool InsertCharacter(const size_t /*count*/) override { return false; } // ICH
    bool DeleteCharacter(const size_t /*count*/) override { return false; } // DCH
    bool ScrollUp(const size_t /*distance*/) override { return false; } // SU
    bool ScrollDown(const size_t /*distance*/) override { return false; } // SD
    bool InsertLine(const size_t /*distance*/) override { return false; } // IL
    bool DeleteLine(const size_t /*distance*/) override { return false; } // DL
    bool SetColumns(const size_t /*columns*/) override { return false; } // DECSCPP, DECCOLM
    bool SetCursorKeysMode(const bool /*applicationMode*/) override { return false; } // DECCKM
    bool SetKeypadMode(const bool /*applicationMode*/) override { return false; } // DECKPAM, DECKPNM
    bool EnableCursorBlinking(const bool /*enable*/) override { return false; } // ATT610
    bool SetOriginMode(const bool /*relativeMode*/) override { return false; }; // DECOM
    bool SetTopBottomScrollingMargins(const size_t /*topMargin*/, const size_t /*bottomMargin*/) override { return false; } // DECSTBM
    bool ReverseLineFeed() override { return false; } // RI
    bool SetWindowTitle(std::wstring_view /*title*/) override { return false; } // OscWindowTitle
    bool UseAlternateScreenBuffer() override { return false; } // ASBSET
    bool UseMainScreenBuffer() override { return false; } // ASBRST
    bool HorizontalTabSet() override { return false; } // HTS
    bool ForwardTab(const size_t /*numTabs*/) override { return false; } // CHT
    bool BackwardsTab(const size_t /*numTabs*/) override { return false; } // CBT
    bool TabClear(const size_t /*clearType*/) override { return false; } // TBC
    bool EnableDECCOLMSupport(const bool /*enabled*/) override { return false; } // ?40
    bool EnableVT200MouseMode(const bool /*enabled*/) override { return false; } // ?1000
    bool EnableUTF8ExtendedMouseMode(const bool /*enabled*/) override { return false; } // ?1005
    bool EnableSGRExtendedMouseMode(const bool /*enabled*/) override { return false; } // ?1006
    bool EnableButtonEventMouseMode(const bool /*enabled*/) override { return false; } // ?1002
    bool EnableAnyEventMouseMode(const bool /*enabled*/) override { return false; } // ?1003
    bool EnableAlternateScroll(const bool /*enabled*/) override { return false; } // ?1007
    bool SetColorTableEntry(const size_t /*tableIndex*/, const DWORD /*color*/) override { return false; } // OSCColorTable
    bool SetDefaultForeground(const DWORD /*color*/) override { return false; } // OSCDefaultForeground
    bool SetDefaultBackground(const DWORD /*color*/) override { return false; } // OSCDefaultBackground

    bool EraseInDisplay(const DispatchTypes::EraseType /* eraseType*/) override { return false; } // ED
    bool EraseInLine(const DispatchTypes::EraseType /* eraseType*/) override { return false; } // EL
    bool EraseCharacters(const size_t /*numChars*/) override { return false; } // ECH

    bool SetGraphicsRendition(const std::basic_string_view<DispatchTypes::GraphicsOptions> /*options*/) override { return false; } // SGR

    bool SetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> /*params*/) override { return false; } // DECSET

    bool ResetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> /*params*/) override { return false; } // DECRST

    bool DeviceStatusReport(const DispatchTypes::AnsiStatusType /*statusType*/) override { return false; } // DSR
    bool DeviceAttributes() override { return false; } // DA

    bool DesignateCharset(const wchar_t /*wchCharset*/) override { return false; } // DesignateCharset

    bool SoftReset() override { return false; } // DECSTR
    bool HardReset() override { return false; } // RIS
    bool ScreenAlignmentPattern() override { return false; } // DECALN

    bool SetCursorStyle(const DispatchTypes::CursorStyle /*cursorStyle*/) override { return false; } // DECSCUSR
    bool SetCursorColor(const COLORREF /*color*/) override { return false; } // OSCSetCursorColor, OSCResetCursorColor

    // DTTERM_WindowManipulation
    bool WindowManipulation(const DispatchTypes::WindowManipulationType /*function*/,
                            const std::basic_string_view<size_t> /*params*/) override { return false; }
};
