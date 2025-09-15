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

    void CursorUp(const VTInt /*distance*/) override {} // CUU
    void CursorDown(const VTInt /*distance*/) override {} // CUD
    void CursorForward(const VTInt /*distance*/) override {} // CUF
    void CursorBackward(const VTInt /*distance*/) override {} // CUB, BS
    void CursorNextLine(const VTInt /*distance*/) override {} // CNL
    void CursorPrevLine(const VTInt /*distance*/) override {} // CPL
    void CursorHorizontalPositionAbsolute(const VTInt /*column*/) override {} // HPA, CHA
    void VerticalLinePositionAbsolute(const VTInt /*line*/) override {} // VPA
    void HorizontalPositionRelative(const VTInt /*distance*/) override {} // HPR
    void VerticalPositionRelative(const VTInt /*distance*/) override {} // VPR
    void CursorPosition(const VTInt /*line*/, const VTInt /*column*/) override {} // CUP, HVP
    void CursorSaveState() override {} // DECSC
    void CursorRestoreState() override {} // DECRC
    void InsertCharacter(const VTInt /*count*/) override {} // ICH
    void DeleteCharacter(const VTInt /*count*/) override {} // DCH
    void ScrollUp(const VTInt /*distance*/) override {} // SU
    void ScrollDown(const VTInt /*distance*/) override {} // SD
    void NextPage(const VTInt /*pageCount*/) override {} // NP
    void PrecedingPage(const VTInt /*pageCount*/) override {} // PP
    void PagePositionAbsolute(const VTInt /*page*/) override {} // PPA
    void PagePositionRelative(const VTInt /*pageCount*/) override {} // PPR
    void PagePositionBack(const VTInt /*pageCount*/) override {} // PPB
    void RequestDisplayedExtent() override {} // DECRQDE
    void InsertLine(const VTInt /*distance*/) override {} // IL
    void DeleteLine(const VTInt /*distance*/) override {} // DL
    void InsertColumn(const VTInt /*distance*/) override {} // DECIC
    void DeleteColumn(const VTInt /*distance*/) override {} // DECDC
    void SetKeypadMode(const bool /*applicationMode*/) override {} // DECKPAM, DECKPNM
    void SetAnsiMode(const bool /*ansiMode*/) override {} // DECANM
    void SetTopBottomScrollingMargins(const VTInt /*topMargin*/, const VTInt /*bottomMargin*/) override {} // DECSTBM
    void SetLeftRightScrollingMargins(const VTInt /*leftMargin*/, const VTInt /*rightMargin*/) override {} // DECSLRM
    void EnquireAnswerback() override {} // ENQ
    void WarningBell() override {} // BEL
    void CarriageReturn() override {} // CR
    void LineFeed(const DispatchTypes::LineFeedType /*lineFeedType*/) override {} // IND, NEL, LF, FF, VT
    void ReverseLineFeed() override {} // RI
    void BackIndex() override {} // DECBI
    void ForwardIndex() override {} // DECFI
    void SetWindowTitle(std::wstring_view /*title*/) override {} // DECSWT, OscWindowTitle
    void HorizontalTabSet() override {} // HTS
    void ForwardTab(const VTInt /*numTabs*/) override {} // CHT, HT
    void BackwardsTab(const VTInt /*numTabs*/) override {} // CBT
    void TabClear(const DispatchTypes::TabClearType /*clearType*/) override {} // TBC
    void TabSet(const VTParameter /*setType*/) override {} // DECST8C
    void SetColorTableEntry(const size_t /*tableIndex*/, const DWORD /*color*/) override {} // OSCSetColorTable
    void RequestColorTableEntry(const size_t /*tableIndex*/) override {} // OSCGetColorTable
    void ResetColorTable() override {} // OSCResetColorTable
    void ResetColorTableEntry(const size_t /*tableIndex*/) override {} // OSCResetColorTable
    void SetXtermColorResource(const size_t /*resource*/, const DWORD /*color*/) override {} // OSCSetDefaultForeground, OSCSetDefaultBackground, OSCSetCursorColor
    void RequestXtermColorResource(const size_t /*resource*/) override {} // OSCGetDefaultForeground, OSCGetDefaultBackground, OSCGetCursorColor
    void ResetXtermColorResource(const size_t /*resource*/) override {} // OSCResetForegroundColor, OSCResetBackgroundColor, OSCResetCursorColor, OSCResetHighlightColor
    void AssignColor(const DispatchTypes::ColorItem /*item*/, const VTInt /*fgIndex*/, const VTInt /*bgIndex*/) override {} // DECAC

    void EraseInDisplay(const DispatchTypes::EraseType /* eraseType*/) override {} // ED
    void EraseInLine(const DispatchTypes::EraseType /* eraseType*/) override {} // EL
    void EraseCharacters(const VTInt /*numChars*/) override {} // ECH
    void SelectiveEraseInDisplay(const DispatchTypes::EraseType /*eraseType*/) override {} // DECSED
    void SelectiveEraseInLine(const DispatchTypes::EraseType /*eraseType*/) override {} // DECSEL

    void ChangeAttributesRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/, const VTParameters /*attrs*/) override {} // DECCARA
    void ReverseAttributesRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/, const VTParameters /*attrs*/) override {} // DECRARA
    void CopyRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/, const VTInt /*page*/, const VTInt /*dstTop*/, const VTInt /*dstLeft*/, const VTInt /*dstPage*/) override {} // DECCRA
    void FillRectangularArea(const VTParameter /*ch*/, const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/) override {} // DECFRA
    void EraseRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/) override {} // DECERA
    void SelectiveEraseRectangularArea(const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/) override {} // DECSERA
    void SelectAttributeChangeExtent(const DispatchTypes::ChangeExtent /*changeExtent*/) override {} // DECSACE
    void RequestChecksumRectangularArea(const VTInt /*id*/, const VTInt /*page*/, const VTInt /*top*/, const VTInt /*left*/, const VTInt /*bottom*/, const VTInt /*right*/) override {} // DECRQCRA

    void SetGraphicsRendition(const VTParameters /*options*/) override {} // SGR
    void SetLineRendition(const LineRendition /*rendition*/) override {} // DECSWL, DECDWL, DECDHL
    void SetCharacterProtectionAttribute(const VTParameters /*options*/) override {} // DECSCA

    void PushGraphicsRendition(const VTParameters /*options*/) override {} // XTPUSHSGR
    void PopGraphicsRendition() override {} // XTPOPSGR

    void SetMode(const DispatchTypes::ModeParams /*param*/) override {} // SM, DECSET
    void ResetMode(const DispatchTypes::ModeParams /*param*/) override {} // RM, DECRST
    void RequestMode(const DispatchTypes::ModeParams /*param*/) override {} // DECRQM

    void DeviceStatusReport(const DispatchTypes::StatusType /*statusType*/, const VTParameter /*id*/) override {} // DSR
    void DeviceAttributes() override {} // DA1
    void SecondaryDeviceAttributes() override {} // DA2
    void TertiaryDeviceAttributes() override {} // DA3
    void Vt52DeviceAttributes() override {} // VT52 Identify
    void RequestTerminalParameters(const DispatchTypes::ReportingPermission /*permission*/) override {} // DECREQTPARM

    void DesignateCodingSystem(const VTID /*codingSystem*/) override {} // DOCS
    void Designate94Charset(const VTInt /*gsetNumber*/, const VTID /*charset*/) override {} // SCS
    void Designate96Charset(const VTInt /*gsetNumber*/, const VTID /*charset*/) override {} // SCS
    void LockingShift(const VTInt /*gsetNumber*/) override {} // LS0, LS1, LS2, LS3
    void LockingShiftRight(const VTInt /*gsetNumber*/) override {} // LS1R, LS2R, LS3R
    void SingleShift(const VTInt /*gsetNumber*/) override {} // SS2, SS3
    void AcceptC1Controls(const bool /*enabled*/) override {} // DECAC1
    void SendC1Controls(const bool /*enabled*/) override {} // S8C1T, S7C1T
    void AnnounceCodeStructure(const VTInt /*ansiLevel*/) override {} // ACS

    void SoftReset() override {} // DECSTR
    void HardReset() override {} // RIS
    void ScreenAlignmentPattern() override {} // DECALN

    void SetCursorStyle(const DispatchTypes::CursorStyle /*cursorStyle*/) override {} // DECSCUSR

    void SetClipboard(wil::zwstring_view /*content*/) override {} // OscSetClipboard

    // DTTERM_WindowManipulation
    void WindowManipulation(const DispatchTypes::WindowManipulationType /*function*/,
                            const VTParameter /*parameter1*/,
                            const VTParameter /*parameter2*/) override {}

    void AddHyperlink(const std::wstring_view /*uri*/, const std::wstring_view /*params*/) override {}
    void EndHyperlink() override {}

    void DoConEmuAction(const std::wstring_view /*string*/) override {}

    void DoITerm2Action(const std::wstring_view /*string*/) override {}

    void DoFinalTermAction(const std::wstring_view /*string*/) override {}

    void DoVsCodeAction(const std::wstring_view /*string*/) override {}

    void DoWTAction(const std::wstring_view /*string*/) override {}

    StringHandler DefineSixelImage(const VTInt /*macroParameter*/,
                                   const DispatchTypes::SixelBackground /*backgroundSelect*/,
                                   const VTParameter /*backgroundColor*/) override { return nullptr; }; // SIXEL

    StringHandler DownloadDRCS(const VTInt /*fontNumber*/,
                               const VTParameter /*startChar*/,
                               const DispatchTypes::DrcsEraseControl /*eraseControl*/,
                               const DispatchTypes::DrcsCellMatrix /*cellMatrix*/,
                               const DispatchTypes::DrcsFontSet /*fontSet*/,
                               const DispatchTypes::DrcsFontUsage /*fontUsage*/,
                               const VTParameter /*cellHeight*/,
                               const DispatchTypes::CharsetSize /*charsetSize*/) override { return nullptr; } // DECDLD

    void RequestUserPreferenceCharset() override {} // DECRQUPSS
    StringHandler AssignUserPreferenceCharset(const DispatchTypes::CharsetSize /*charsetSize*/) override { return nullptr; } // DECAUPSS

    StringHandler DefineMacro(const VTInt /*macroId*/,
                              const DispatchTypes::MacroDeleteControl /*deleteControl*/,
                              const DispatchTypes::MacroEncoding /*encoding*/) override { return nullptr; } // DECDMAC
    void InvokeMacro(const VTInt /*macroId*/) override {} // DECINVM

    void RequestTerminalStateReport(const DispatchTypes::ReportFormat /*format*/, const VTParameter /*formatOption*/) override {} // DECRQTSR
    StringHandler RestoreTerminalState(const DispatchTypes::ReportFormat /*format*/) override { return nullptr; }; // DECRSTS

    StringHandler RequestSetting() override { return nullptr; }; // DECRQSS

    void RequestPresentationStateReport(const DispatchTypes::PresentationReportFormat /*format*/) override {} // DECRQPSR
    StringHandler RestorePresentationState(const DispatchTypes::PresentationReportFormat /*format*/) override { return nullptr; } // DECRSPS

    void PlaySounds(const VTParameters /*parameters*/) override{}; // DECPS

    void SetOptionalFeatures(const til::enumset<OptionalFeature> /*features*/) override{};
};

#pragma warning(default : 26440) // Restore "can be declared noexcept" warning
