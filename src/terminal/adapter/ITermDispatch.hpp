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
#include "../buffer/out/LineRendition.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class ITermDispatch;
};

class Microsoft::Console::VirtualTerminal::ITermDispatch
{
public:
    using StringHandler = std::function<bool(const wchar_t)>;

    enum class OptionalFeature
    {
        ChecksumReport,
        ClipboardWrite,
    };

#pragma warning(push)
#pragma warning(disable : 26432) // suppress rule of 5 violation on interface because tampering with this is fraught with peril
    virtual ~ITermDispatch() = 0;

    virtual void Print(const wchar_t wchPrintable) = 0;
    virtual void PrintString(const std::wstring_view string) = 0;

    virtual void CursorUp(const VTInt distance) = 0; // CUU
    virtual void CursorDown(const VTInt distance) = 0; // CUD
    virtual void CursorForward(const VTInt distance) = 0; // CUF
    virtual void CursorBackward(const VTInt distance) = 0; // CUB, BS
    virtual void CursorNextLine(const VTInt distance) = 0; // CNL
    virtual void CursorPrevLine(const VTInt distance) = 0; // CPL
    virtual void CursorHorizontalPositionAbsolute(const VTInt column) = 0; // HPA, CHA
    virtual void VerticalLinePositionAbsolute(const VTInt line) = 0; // VPA
    virtual void HorizontalPositionRelative(const VTInt distance) = 0; // HPR
    virtual void VerticalPositionRelative(const VTInt distance) = 0; // VPR
    virtual void CursorPosition(const VTInt line, const VTInt column) = 0; // CUP, HVP
    virtual void CursorSaveState() = 0; // DECSC
    virtual void CursorRestoreState() = 0; // DECRC
    virtual void InsertCharacter(const VTInt count) = 0; // ICH
    virtual void DeleteCharacter(const VTInt count) = 0; // DCH
    virtual void ScrollUp(const VTInt distance) = 0; // SU
    virtual void ScrollDown(const VTInt distance) = 0; // SD
    virtual void NextPage(const VTInt pageCount) = 0; // NP
    virtual void PrecedingPage(const VTInt pageCount) = 0; // PP
    virtual void PagePositionAbsolute(const VTInt page) = 0; // PPA
    virtual void PagePositionRelative(const VTInt pageCount) = 0; // PPR
    virtual void PagePositionBack(const VTInt pageCount) = 0; // PPB
    virtual void RequestDisplayedExtent() = 0; // DECRQDE
    virtual void InsertLine(const VTInt distance) = 0; // IL
    virtual void DeleteLine(const VTInt distance) = 0; // DL
    virtual void InsertColumn(const VTInt distance) = 0; // DECIC
    virtual void DeleteColumn(const VTInt distance) = 0; // DECDC
    virtual void SetKeypadMode(const bool applicationMode) = 0; // DECKPAM, DECKPNM
    virtual void SetAnsiMode(const bool ansiMode) = 0; // DECANM
    virtual void SetTopBottomScrollingMargins(const VTInt topMargin, const VTInt bottomMargin) = 0; // DECSTBM
    virtual void SetLeftRightScrollingMargins(const VTInt leftMargin, const VTInt rightMargin) = 0; // DECSLRM
    virtual void EnquireAnswerback() = 0; // ENQ
    virtual void WarningBell() = 0; // BEL
    virtual void CarriageReturn() = 0; // CR
    virtual void LineFeed(const DispatchTypes::LineFeedType lineFeedType) = 0; // IND, NEL, LF, FF, VT
    virtual void ReverseLineFeed() = 0; // RI
    virtual void BackIndex() = 0; // DECBI
    virtual void ForwardIndex() = 0; // DECFI
    virtual void SetWindowTitle(std::wstring_view title) = 0; // DECSWT, OscWindowTitle
    virtual void HorizontalTabSet() = 0; // HTS
    virtual void ForwardTab(const VTInt numTabs) = 0; // CHT, HT
    virtual void BackwardsTab(const VTInt numTabs) = 0; // CBT
    virtual void TabClear(const DispatchTypes::TabClearType clearType) = 0; // TBC
    virtual void TabSet(const VTParameter setType) = 0; // DECST8C
    virtual void SetColorTableEntry(const size_t tableIndex, const DWORD color) = 0; // OSCSetColorTable
    virtual void RequestColorTableEntry(const size_t tableIndex) = 0; // OSCGetColorTable
    virtual void ResetColorTable() = 0; // OSCResetColorTable
    virtual void ResetColorTableEntry(const size_t tableIndex) = 0; // OSCResetColorTable
    virtual void SetXtermColorResource(const size_t resource, const DWORD color) = 0; // OSCSetDefaultForeground, OSCSetDefaultBackground, OSCSetCursorColor
    virtual void RequestXtermColorResource(const size_t resource) = 0; // OSCGetDefaultForeground, OSCGetDefaultBackground, OSCGetCursorColor
    virtual void ResetXtermColorResource(const size_t resource) = 0; // OSCResetForegroundColor, OSCResetBackgroundColor, OSCResetCursorColor, OSCResetHighlightColor
    virtual void AssignColor(const DispatchTypes::ColorItem item, const VTInt fgIndex, const VTInt bgIndex) = 0; // DECAC

    virtual void EraseInDisplay(const DispatchTypes::EraseType eraseType) = 0; // ED
    virtual void EraseInLine(const DispatchTypes::EraseType eraseType) = 0; // EL
    virtual void EraseCharacters(const VTInt numChars) = 0; // ECH
    virtual void SelectiveEraseInDisplay(const DispatchTypes::EraseType eraseType) = 0; // DECSED
    virtual void SelectiveEraseInLine(const DispatchTypes::EraseType eraseType) = 0; // DECSEL

    virtual void ChangeAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs) = 0; // DECCARA
    virtual void ReverseAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs) = 0; // DECRARA
    virtual void CopyRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTInt page, const VTInt dstTop, const VTInt dstLeft, const VTInt dstPage) = 0; // DECCRA
    virtual void FillRectangularArea(const VTParameter ch, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) = 0; // DECFRA
    virtual void EraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) = 0; // DECERA
    virtual void SelectiveEraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) = 0; // DECSERA
    virtual void SelectAttributeChangeExtent(const DispatchTypes::ChangeExtent changeExtent) = 0; // DECSACE
    virtual void RequestChecksumRectangularArea(const VTInt id, const VTInt page, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) = 0; // DECRQCRA

    virtual void SetGraphicsRendition(const VTParameters options) = 0; // SGR
    virtual void SetLineRendition(const LineRendition rendition) = 0; // DECSWL, DECDWL, DECDHL
    virtual void SetCharacterProtectionAttribute(const VTParameters options) = 0; // DECSCA

    virtual void PushGraphicsRendition(const VTParameters options) = 0; // XTPUSHSGR
    virtual void PopGraphicsRendition() = 0; // XTPOPSGR

    virtual void SetMode(const DispatchTypes::ModeParams param) = 0; // SM, DECSET
    virtual void ResetMode(const DispatchTypes::ModeParams param) = 0; // RM, DECRST
    virtual void RequestMode(const DispatchTypes::ModeParams param) = 0; // DECRQM

    virtual void DeviceStatusReport(const DispatchTypes::StatusType statusType, const VTParameter id) = 0; // DSR
    virtual void DeviceAttributes() = 0; // DA1
    virtual void SecondaryDeviceAttributes() = 0; // DA2
    virtual void TertiaryDeviceAttributes() = 0; // DA3
    virtual void Vt52DeviceAttributes() = 0; // VT52 Identify
    virtual void RequestTerminalParameters(const DispatchTypes::ReportingPermission permission) = 0; // DECREQTPARM

    virtual void DesignateCodingSystem(const VTID codingSystem) = 0; // DOCS
    virtual void Designate94Charset(const VTInt gsetNumber, const VTID charset) = 0; // SCS
    virtual void Designate96Charset(const VTInt gsetNumber, const VTID charset) = 0; // SCS
    virtual void LockingShift(const VTInt gsetNumber) = 0; // LS0, LS1, LS2, LS3
    virtual void LockingShiftRight(const VTInt gsetNumber) = 0; // LS1R, LS2R, LS3R
    virtual void SingleShift(const VTInt gsetNumber) = 0; // SS2, SS3
    virtual void AcceptC1Controls(const bool enabled) = 0; // DECAC1
    virtual void SendC1Controls(const bool enabled) = 0; // S8C1T, S7C1T
    virtual void AnnounceCodeStructure(const VTInt ansiLevel) = 0; // ACS

    virtual void SoftReset() = 0; // DECSTR
    virtual void HardReset() = 0; // RIS
    virtual void ScreenAlignmentPattern() = 0; // DECALN

    virtual void SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) = 0; // DECSCUSR

    virtual void SetClipboard(wil::zwstring_view content) = 0; // OSCSetClipboard

    // DTTERM_WindowManipulation
    virtual void WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                    const VTParameter parameter1,
                                    const VTParameter parameter2) = 0;

    virtual void AddHyperlink(const std::wstring_view uri, const std::wstring_view params) = 0;
    virtual void EndHyperlink() = 0;

    virtual void DoConEmuAction(const std::wstring_view string) = 0;

    virtual void DoITerm2Action(const std::wstring_view string) = 0;

    virtual void DoFinalTermAction(const std::wstring_view string) = 0;

    virtual void DoVsCodeAction(const std::wstring_view string) = 0;

    virtual void DoWTAction(const std::wstring_view string) = 0;

    virtual StringHandler DefineSixelImage(const VTInt macroParameter,
                                           const DispatchTypes::SixelBackground backgroundSelect,
                                           const VTParameter backgroundColor) = 0; // SIXEL

    virtual StringHandler DownloadDRCS(const VTInt fontNumber,
                                       const VTParameter startChar,
                                       const DispatchTypes::DrcsEraseControl eraseControl,
                                       const DispatchTypes::DrcsCellMatrix cellMatrix,
                                       const DispatchTypes::DrcsFontSet fontSet,
                                       const DispatchTypes::DrcsFontUsage fontUsage,
                                       const VTParameter cellHeight,
                                       const DispatchTypes::CharsetSize charsetSize) = 0; // DECDLD

    virtual void RequestUserPreferenceCharset() = 0; // DECRQUPSS
    virtual StringHandler AssignUserPreferenceCharset(const DispatchTypes::CharsetSize charsetSize) = 0; // DECAUPSS

    virtual StringHandler DefineMacro(const VTInt macroId,
                                      const DispatchTypes::MacroDeleteControl deleteControl,
                                      const DispatchTypes::MacroEncoding encoding) = 0; // DECDMAC
    virtual void InvokeMacro(const VTInt macroId) = 0; // DECINVM

    virtual void RequestTerminalStateReport(const DispatchTypes::ReportFormat format, const VTParameter formatOption) = 0; // DECRQTSR
    virtual StringHandler RestoreTerminalState(const DispatchTypes::ReportFormat format) = 0; // DECRSTS

    virtual StringHandler RequestSetting() = 0; // DECRQSS

    virtual void RequestPresentationStateReport(const DispatchTypes::PresentationReportFormat format) = 0; // DECRQPSR
    virtual StringHandler RestorePresentationState(const DispatchTypes::PresentationReportFormat format) = 0; // DECRSPS

    virtual void PlaySounds(const VTParameters parameters) = 0; // DECPS

    virtual void SetOptionalFeatures(const til::enumset<OptionalFeature> features) = 0;
};
inline Microsoft::Console::VirtualTerminal::ITermDispatch::~ITermDispatch() = default;
#pragma warning(pop)
