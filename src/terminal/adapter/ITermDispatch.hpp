// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- ITermDispatch.hpp

Abstract:
- This is the interface for all output state machine callbacks. When actions
    occur, they will be dispatched to the methods on this interface which must
    be implemented by a child class and passed into the state machine on
    creation.
*/
#pragma once
#include "DispatchTypes.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class ITermDispatch;
};

class Microsoft::Console::VirtualTerminal::ITermDispatch
{
public:
    virtual ~ITermDispatch() = 0;
    virtual void Execute(const wchar_t wchControl) = 0;
    virtual void Print(const wchar_t wchPrintable) = 0;
    virtual void PrintString(const wchar_t* const rgwch, const size_t cch) = 0;

    virtual bool CursorUp(const unsigned int uiDistance) = 0; // CUU
    virtual bool CursorDown(const unsigned int uiDistance) = 0; // CUD
    virtual bool CursorForward(const unsigned int uiDistance) = 0; // CUF
    virtual bool CursorBackward(const unsigned int uiDistance) = 0; // CUB
    virtual bool CursorNextLine(const unsigned int uiDistance) = 0; // CNL
    virtual bool CursorPrevLine(const unsigned int uiDistance) = 0; // CPL
    virtual bool CursorHorizontalPositionAbsolute(const unsigned int uiColumn) = 0; // CHA
    virtual bool VerticalLinePositionAbsolute(const unsigned int uiLine) = 0; // VPA
    virtual bool CursorPosition(const unsigned int uiLine, const unsigned int uiColumn) = 0; // CUP
    virtual bool CursorSaveState() = 0; // DECSC
    virtual bool CursorRestoreState() = 0; // DECRC
    virtual bool CursorVisibility(const bool fIsVisible) = 0; // DECTCEM
    virtual bool InsertCharacter(const unsigned int uiCount) = 0; // ICH
    virtual bool DeleteCharacter(const unsigned int uiCount) = 0; // DCH
    virtual bool ScrollUp(const unsigned int uiDistance) = 0; // SU
    virtual bool ScrollDown(const unsigned int uiDistance) = 0; // SD
    virtual bool InsertLine(const unsigned int uiDistance) = 0; // IL
    virtual bool DeleteLine(const unsigned int uiDistance) = 0; // DL
    virtual bool SetColumns(const unsigned int uiColumns) = 0; // DECSCPP, DECCOLM
    virtual bool SetCursorKeysMode(const bool fApplicationMode) = 0; // DECCKM
    virtual bool SetKeypadMode(const bool fApplicationMode) = 0; // DECKPAM, DECKPNM
    virtual bool EnableCursorBlinking(const bool fEnable) = 0; // ATT610
    virtual bool SetOriginMode(const bool fRelativeMode) = 0; // DECOM
    virtual bool SetTopBottomScrollingMargins(const SHORT sTopMargin, const SHORT sBottomMargin) = 0; // DECSTBM
    virtual bool ReverseLineFeed() = 0; // RI
    virtual bool SetWindowTitle(std::wstring_view title) = 0; // OscWindowTitle
    virtual bool UseAlternateScreenBuffer() = 0; // ASBSET
    virtual bool UseMainScreenBuffer() = 0; // ASBRST
    virtual bool HorizontalTabSet() = 0; // HTS
    virtual bool ForwardTab(const SHORT sNumTabs) = 0; // CHT
    virtual bool BackwardsTab(const SHORT sNumTabs) = 0; // CBT
    virtual bool TabClear(const SHORT sClearType) = 0; // TBC
    virtual bool EnableDECCOLMSupport(const bool fEnabled) = 0; // ?40
    virtual bool EnableVT200MouseMode(const bool fEnabled) = 0; // ?1000
    virtual bool EnableUTF8ExtendedMouseMode(const bool fEnabled) = 0; // ?1005
    virtual bool EnableSGRExtendedMouseMode(const bool fEnabled) = 0; // ?1006
    virtual bool EnableButtonEventMouseMode(const bool fEnabled) = 0; // ?1002
    virtual bool EnableAnyEventMouseMode(const bool fEnabled) = 0; // ?1003
    virtual bool EnableAlternateScroll(const bool fEnabled) = 0; // ?1007
    virtual bool SetColorTableEntry(const size_t tableIndex, const DWORD dwColor) = 0; // OSCColorTable
    virtual bool SetDefaultForeground(const DWORD dwColor) = 0; // OSCDefaultForeground
    virtual bool SetDefaultBackground(const DWORD dwColor) = 0; // OSCDefaultBackground

    virtual bool EraseInDisplay(const DispatchTypes::EraseType eraseType) = 0; // ED
    virtual bool EraseInLine(const DispatchTypes::EraseType eraseType) = 0; // EL
    virtual bool EraseCharacters(const unsigned int uiNumChars) = 0; // ECH

    virtual bool SetGraphicsRendition(_In_reads_(cOptions) const DispatchTypes::GraphicsOptions* const rgOptions,
                                      const size_t cOptions) = 0; // SGR

    virtual bool SetPrivateModes(_In_reads_(cParams) const DispatchTypes::PrivateModeParams* const rgParams,
                                 const size_t cParams) = 0; // DECSET

    virtual bool ResetPrivateModes(_In_reads_(cParams) const DispatchTypes::PrivateModeParams* const rgParams,
                                   const size_t cParams) = 0; // DECRST

    virtual bool DeviceStatusReport(const DispatchTypes::AnsiStatusType statusType) = 0; // DSR
    virtual bool DeviceAttributes() = 0; // DA

    virtual bool DesignateCharset(const wchar_t wchCharset) = 0; // DesignateCharset

    virtual bool SoftReset() = 0; // DECSTR
    virtual bool HardReset() = 0; // RIS

    virtual bool SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) = 0; // DECSCUSR
    virtual bool SetCursorColor(const COLORREF Color) = 0; // OSCSetCursorColor, OSCResetCursorColor

    // DTTERM_WindowManipulation
    virtual bool WindowManipulation(const DispatchTypes::WindowManipulationType uiFunction,
                                    _In_reads_(cParams) const unsigned short* const rgusParams,
                                    const size_t cParams) = 0;
};
inline Microsoft::Console::VirtualTerminal::ITermDispatch::~ITermDispatch() {}
