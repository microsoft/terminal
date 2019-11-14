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
    void PrintString(const wchar_t* const rgwch, const size_t cch) override = 0;

    bool CursorUp(const unsigned int /*uiDistance*/) override { return false; } // CUU
    bool CursorDown(const unsigned int /*uiDistance*/) override { return false; } // CUD
    bool CursorForward(const unsigned int /*uiDistance*/) override { return false; } // CUF
    bool CursorBackward(const unsigned int /*uiDistance*/) override { return false; } // CUB
    bool CursorNextLine(const unsigned int /*uiDistance*/) override { return false; } // CNL
    bool CursorPrevLine(const unsigned int /*uiDistance*/) override { return false; } // CPL
    bool CursorHorizontalPositionAbsolute(const unsigned int /*uiColumn*/) override { return false; } // CHA
    bool VerticalLinePositionAbsolute(const unsigned int /*uiLine*/) override { return false; } // VPA
    bool CursorPosition(const unsigned int /*uiLine*/, const unsigned int /*uiColumn*/) override { return false; } // CUP
    bool CursorSaveState() override { return false; } // DECSC
    bool CursorRestoreState() override { return false; } // DECRC
    bool CursorVisibility(const bool /*fIsVisible*/) override { return false; } // DECTCEM
    bool InsertCharacter(const unsigned int /*uiCount*/) override { return false; } // ICH
    bool DeleteCharacter(const unsigned int /*uiCount*/) override { return false; } // DCH
    bool ScrollUp(const unsigned int /*uiDistance*/) override { return false; } // SU
    bool ScrollDown(const unsigned int /*uiDistance*/) override { return false; } // SD
    bool InsertLine(const unsigned int /*uiDistance*/) override { return false; } // IL
    bool DeleteLine(const unsigned int /*uiDistance*/) override { return false; } // DL
    bool SetColumns(const unsigned int /*uiColumns*/) override { return false; } // DECSCPP, DECCOLM
    bool SetCursorKeysMode(const bool /*fApplicationMode*/) override { return false; } // DECCKM
    bool SetKeypadMode(const bool /*fApplicationMode*/) override { return false; } // DECKPAM, DECKPNM
    bool EnableCursorBlinking(const bool /*fEnable*/) override { return false; } // ATT610
    bool SetOriginMode(const bool /*fRelativeMode*/) override { return false; }; // DECOM
    bool SetTopBottomScrollingMargins(const SHORT /*sTopMargin*/, const SHORT /*sBottomMargin*/) override { return false; } // DECSTBM
    bool ReverseLineFeed() override { return false; } // RI
    bool SetWindowTitle(std::wstring_view /*title*/) override { return false; } // OscWindowTitle
    bool UseAlternateScreenBuffer() override { return false; } // ASBSET
    bool UseMainScreenBuffer() override { return false; } // ASBRST
    bool HorizontalTabSet() override { return false; } // HTS
    bool ForwardTab(const SHORT /*sNumTabs*/) override { return false; } // CHT
    bool BackwardsTab(const SHORT /*sNumTabs*/) override { return false; } // CBT
    bool TabClear(const SHORT /*sClearType*/) override { return false; } // TBC
    bool EnableDECCOLMSupport(const bool /*fEnabled*/) override { return false; } // ?40
    bool EnableVT200MouseMode(const bool /*fEnabled*/) override { return false; } // ?1000
    bool EnableUTF8ExtendedMouseMode(const bool /*fEnabled*/) override { return false; } // ?1005
    bool EnableSGRExtendedMouseMode(const bool /*fEnabled*/) override { return false; } // ?1006
    bool EnableButtonEventMouseMode(const bool /*fEnabled*/) override { return false; } // ?1002
    bool EnableAnyEventMouseMode(const bool /*fEnabled*/) override { return false; } // ?1003
    bool EnableAlternateScroll(const bool /*fEnabled*/) override { return false; } // ?1007
    bool SetColorTableEntry(const size_t /*tableIndex*/, const DWORD /*dwColor*/) override { return false; } // OSCColorTable
    bool SetDefaultForeground(const DWORD /*dwColor*/) override { return false; } // OSCDefaultForeground
    bool SetDefaultBackground(const DWORD /*dwColor*/) override { return false; } // OSCDefaultBackground

    bool EraseInDisplay(const DispatchTypes::EraseType /* eraseType*/) override { return false; } // ED
    bool EraseInLine(const DispatchTypes::EraseType /* eraseType*/) override { return false; } // EL
    bool EraseCharacters(const unsigned int /*uiNumChars*/) override { return false; } // ECH

    bool SetGraphicsRendition(_In_reads_(_Param_(2)) const DispatchTypes::GraphicsOptions* const /*rgOptions*/,
                              const size_t /*cOptions*/) override { return false; } // SGR

    bool SetPrivateModes(_In_reads_(_Param_(2)) const DispatchTypes::PrivateModeParams* const /*rgParams*/,
                         const size_t /*cParams*/) override { return false; } // DECSET

    bool ResetPrivateModes(_In_reads_(_Param_(2)) const DispatchTypes::PrivateModeParams* const /*rgParams*/,
                           const size_t /*cParams*/) override { return false; } // DECRST

    bool DeviceStatusReport(const DispatchTypes::AnsiStatusType /*statusType*/) override { return false; } // DSR
    bool DeviceAttributes() override { return false; } // DA

    bool DesignateCharset(const wchar_t /*wchCharset*/) override { return false; } // DesignateCharset

    bool SoftReset() override { return false; } // DECSTR
    bool HardReset() override { return false; } // RIS

    bool SetCursorStyle(const DispatchTypes::CursorStyle /*cursorStyle*/) override { return false; } // DECSCUSR
    bool SetCursorColor(const COLORREF /*Color*/) override { return false; } // OSCSetCursorColor, OSCResetCursorColor

    // DTTERM_WindowManipulation
    bool WindowManipulation(const DispatchTypes::WindowManipulationType /*uiFunction*/,
                            _In_reads_(_Param_(3)) const unsigned short* const /*rgusParams*/,
                            const size_t /*cParams*/) override { return false; }
};
