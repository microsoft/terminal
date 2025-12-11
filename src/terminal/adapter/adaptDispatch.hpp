/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- adaptDispatch.hpp

Abstract:
- This serves as the Windows Console API-specific implementation of the callbacks from our generic Virtual Terminal parser.

Author(s):
- Michael Niksa (MiNiksa) 30-July-2015
--*/

#pragma once

#include "termDispatch.hpp"
#include "ITerminalApi.hpp"
#include "FontBuffer.hpp"
#include "MacroBuffer.hpp"
#include "PageManager.hpp"
#include "terminalOutput.hpp"
#include "../input/terminalInput.hpp"
#include "../../types/inc/sgrStack.hpp"

// fwdecl unittest classes
#ifdef UNIT_TESTING
class AdapterTest;
#endif

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch : public ITermDispatch
    {
        using Renderer = Microsoft::Console::Render::Renderer;
        using RenderSettings = Microsoft::Console::Render::RenderSettings;

    public:
        AdaptDispatch(ITerminalApi& api, Renderer* renderer, RenderSettings& renderSettings, TerminalInput& terminalInput) noexcept;

        void Print(const wchar_t wchPrintable) override;
        void PrintString(const std::wstring_view string) override;

        void CursorUp(const VTInt distance) override; // CUU
        void CursorDown(const VTInt distance) override; // CUD
        void CursorForward(const VTInt distance) override; // CUF
        void CursorBackward(const VTInt distance) override; // CUB, BS
        void CursorNextLine(const VTInt distance) override; // CNL
        void CursorPrevLine(const VTInt distance) override; // CPL
        void CursorHorizontalPositionAbsolute(const VTInt column) override; // HPA, CHA
        void VerticalLinePositionAbsolute(const VTInt line) override; // VPA
        void HorizontalPositionRelative(const VTInt distance) override; // HPR
        void VerticalPositionRelative(const VTInt distance) override; // VPR
        void CursorPosition(const VTInt line, const VTInt column) override; // CUP, HVP
        void CursorSaveState() override; // DECSC
        void CursorRestoreState() override; // DECRC
        void EraseInDisplay(const DispatchTypes::EraseType eraseType) override; // ED
        void EraseInLine(const DispatchTypes::EraseType eraseType) override; // EL
        void EraseCharacters(const VTInt numChars) override; // ECH
        void SelectiveEraseInDisplay(const DispatchTypes::EraseType eraseType) override; // DECSED
        void SelectiveEraseInLine(const DispatchTypes::EraseType eraseType) override; // DECSEL
        void InsertCharacter(const VTInt count) override; // ICH
        void DeleteCharacter(const VTInt count) override; // DCH
        void ChangeAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs) override; // DECCARA
        void ReverseAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs) override; // DECRARA
        void CopyRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTInt page, const VTInt dstTop, const VTInt dstLeft, const VTInt dstPage) override; // DECCRA
        void FillRectangularArea(const VTParameter ch, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override; // DECFRA
        void EraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override; // DECERA
        void SelectiveEraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override; // DECSERA
        void SelectAttributeChangeExtent(const DispatchTypes::ChangeExtent changeExtent) noexcept override; // DECSACE
        void RequestChecksumRectangularArea(const VTInt id, const VTInt page, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override; // DECRQCRA
        void SetGraphicsRendition(const VTParameters options) override; // SGR
        void SetLineRendition(const LineRendition rendition) override; // DECSWL, DECDWL, DECDHL
        void SetCharacterProtectionAttribute(const VTParameters options) override; // DECSCA
        void PushGraphicsRendition(const VTParameters options) override; // XTPUSHSGR
        void PopGraphicsRendition() override; // XTPOPSGR
        void DeviceStatusReport(const DispatchTypes::StatusType statusType, const VTParameter id) override; // DSR
        void DeviceAttributes() override; // DA1
        void SecondaryDeviceAttributes() override; // DA2
        void TertiaryDeviceAttributes() override; // DA3
        void Vt52DeviceAttributes() override; // VT52 Identify
        void RequestTerminalParameters(const DispatchTypes::ReportingPermission permission) override; // DECREQTPARM
        void ScrollUp(const VTInt distance) override; // SU
        void ScrollDown(const VTInt distance) override; // SD
        void NextPage(const VTInt pageCount) override; // NP
        void PrecedingPage(const VTInt pageCount) override; // PP
        void PagePositionAbsolute(const VTInt page) override; // PPA
        void PagePositionRelative(const VTInt pageCount) override; // PPR
        void PagePositionBack(const VTInt pageCount) override; // PPB
        void RequestDisplayedExtent() override; // DECRQDE
        void InsertLine(const VTInt distance) override; // IL
        void DeleteLine(const VTInt distance) override; // DL
        void InsertColumn(const VTInt distance) override; // DECIC
        void DeleteColumn(const VTInt distance) override; // DECDC
        void SetMode(const DispatchTypes::ModeParams param) override; // SM, DECSET
        void ResetMode(const DispatchTypes::ModeParams param) override; // RM, DECRST
        void RequestMode(const DispatchTypes::ModeParams param) override; // DECRQM
        void SetKeypadMode(const bool applicationMode) noexcept override; // DECKPAM, DECKPNM
        void SetAnsiMode(const bool ansiMode) override; // DECANM
        void SetTopBottomScrollingMargins(const VTInt topMargin,
                                          const VTInt bottomMargin) override; // DECSTBM
        void SetLeftRightScrollingMargins(const VTInt leftMargin,
                                          const VTInt rightMargin) override; // DECSLRM
        void EnquireAnswerback() override; // ENQ
        void WarningBell() override; // BEL
        void CarriageReturn() override; // CR
        void LineFeed(const DispatchTypes::LineFeedType lineFeedType) override; // IND, NEL, LF, FF, VT
        void ReverseLineFeed() override; // RI
        void BackIndex() override; // DECBI
        void ForwardIndex() override; // DECFI
        void SetWindowTitle(const std::wstring_view title) override; // DECSWT, OSCWindowTitle
        void HorizontalTabSet() override; // HTS
        void ForwardTab(const VTInt numTabs) override; // CHT, HT
        void BackwardsTab(const VTInt numTabs) override; // CBT
        void TabClear(const DispatchTypes::TabClearType clearType) override; // TBC
        void TabSet(const VTParameter setType) noexcept override; // DECST8C
        void DesignateCodingSystem(const VTID codingSystem) override; // DOCS
        void Designate94Charset(const VTInt gsetNumber, const VTID charset) override; // SCS
        void Designate96Charset(const VTInt gsetNumber, const VTID charset) override; // SCS
        void LockingShift(const VTInt gsetNumber) override; // LS0, LS1, LS2, LS3
        void LockingShiftRight(const VTInt gsetNumber) override; // LS1R, LS2R, LS3R
        void SingleShift(const VTInt gsetNumber) noexcept override; // SS2, SS3
        void AcceptC1Controls(const bool enabled) override; // DECAC1
        void SendC1Controls(const bool enabled) override; // S8C1T, S7C1T
        void AnnounceCodeStructure(const VTInt ansiLevel) override; // ACS
        void SoftReset() override; // DECSTR
        void HardReset() override; // RIS
        void ScreenAlignmentPattern() override; // DECALN
        void SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) override; // DECSCUSR

        void SetClipboard(const wil::zwstring_view content) override; // OSCSetClipboard

        void SetColorTableEntry(const size_t tableIndex,
                                const DWORD color) override; // OSCSetColorTable
        void RequestColorTableEntry(const size_t tableIndex) override; // OSCGetColorTable
        void ResetColorTable() override; // OSCResetColorTable
        void ResetColorTableEntry(const size_t tableIndex) override; // OSCResetColorTable
        void SetXtermColorResource(const size_t resource, const DWORD color) override; // OSCSetDefaultForeground, OSCSetDefaultBackground, OSCSetCursorColor
        void RequestXtermColorResource(const size_t resource) override; // OSCGetDefaultForeground, OSCGetDefaultBackground, OSCGetCursorColor
        void ResetXtermColorResource(const size_t resource) override; // OSCResetForegroundColor, OSCResetBackgroundColor, OSCResetCursorColor, OSCResetHighlightColor
        void AssignColor(const DispatchTypes::ColorItem item, const VTInt fgIndex, const VTInt bgIndex) override; // DECAC

        void WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                const VTParameter parameter1,
                                const VTParameter parameter2) override; // DTTERM_WindowManipulation

        void AddHyperlink(const std::wstring_view uri, const std::wstring_view params) override;
        void EndHyperlink() override;

        void DoConEmuAction(const std::wstring_view string) override;

        void DoITerm2Action(const std::wstring_view string) override;

        void DoFinalTermAction(const std::wstring_view string) override;

        void DoVsCodeAction(const std::wstring_view string) override;

        void DoWTAction(const std::wstring_view string) override;

        StringHandler DefineSixelImage(const VTInt macroParameter,
                                       const DispatchTypes::SixelBackground backgroundSelect,
                                       const VTParameter backgroundColor) override; // SIXEL

        StringHandler DownloadDRCS(const VTInt fontNumber,
                                   const VTParameter startChar,
                                   const DispatchTypes::DrcsEraseControl eraseControl,
                                   const DispatchTypes::DrcsCellMatrix cellMatrix,
                                   const DispatchTypes::DrcsFontSet fontSet,
                                   const DispatchTypes::DrcsFontUsage fontUsage,
                                   const VTParameter cellHeight,
                                   const DispatchTypes::CharsetSize charsetSize) override; // DECDLD

        void RequestUserPreferenceCharset() override; // DECRQUPSS
        StringHandler AssignUserPreferenceCharset(const DispatchTypes::CharsetSize charsetSize) override; // DECAUPSS

        StringHandler DefineMacro(const VTInt macroId,
                                  const DispatchTypes::MacroDeleteControl deleteControl,
                                  const DispatchTypes::MacroEncoding encoding) override; // DECDMAC
        void InvokeMacro(const VTInt macroId) override; // DECINVM

        void RequestTerminalStateReport(const DispatchTypes::ReportFormat format, const VTParameter formatOption) override; // DECRQTSR
        StringHandler RestoreTerminalState(const DispatchTypes::ReportFormat format) override; // DECRSTS

        StringHandler RequestSetting() override; // DECRQSS

        void RequestPresentationStateReport(const DispatchTypes::PresentationReportFormat format) override; // DECRQPSR
        StringHandler RestorePresentationState(const DispatchTypes::PresentationReportFormat format) override; // DECRSPS

        void PlaySounds(const VTParameters parameters) override; // DECPS

        void SetOptionalFeatures(const til::enumset<OptionalFeature> features) noexcept override;

    private:
        enum class Mode
        {
            InsertReplace,
            Origin,
            Column,
            AllowDECCOLM,
            AllowDECSLRM,
            SixelDisplay,
            EraseColor,
            RectangularChangeExtent,
            PageCursorCoupling
        };
        enum class ScrollDirection
        {
            Up,
            Down
        };
        struct CursorState
        {
            VTInt Row = 1;
            VTInt Column = 1;
            VTInt Page = 1;
            bool IsDelayedEOLWrap = false;
            bool IsOriginModeRelative = false;
            TextAttribute Attributes = {};
            TerminalOutput TermOutput = {};
        };
        struct Offset
        {
            VTInt Value;
            bool IsAbsolute;
            // VT origin is at 1,1 so we need to subtract 1 from absolute positions.
            static constexpr Offset Absolute(const VTInt value) { return { value - 1, true }; };
            static constexpr Offset Forward(const VTInt value) { return { value, false }; };
            static constexpr Offset Backward(const VTInt value) { return { -value, false }; };
            static constexpr Offset Unchanged() { return Forward(0); };
        };
        struct ChangeOps
        {
            CharacterAttributes andAttrMask = CharacterAttributes::All;
            CharacterAttributes xorAttrMask = CharacterAttributes::Normal;
            std::optional<TextColor> foreground;
            std::optional<TextColor> background;
            std::optional<TextColor> underlineColor;
        };

        void _WriteToBuffer(const std::wstring_view string);
        std::pair<int, int> _GetVerticalMargins(const Page& page, const bool absolute) noexcept;
        std::pair<int, int> _GetHorizontalMargins(const til::CoordType bufferWidth) noexcept;
        void _CursorMovePosition(const Offset rowOffset, const Offset colOffset, const bool clampInMargins);
        void _FillRect(const Page& page, const til::rect& fillRect, const std::wstring_view& fillChar, const TextAttribute& fillAttrs) const;
        void _SelectiveEraseRect(const Page& page, const til::rect& eraseRect);
        void _ChangeRectAttributes(const Page& page, const til::rect& changeRect, const ChangeOps& changeOps);
        void _ChangeRectOrStreamAttributes(const til::rect& changeArea, const ChangeOps& changeOps);
        til::rect _CalculateRectArea(const Page& page, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right);
        void _EraseScrollback();
        void _EraseAll();
        TextAttribute _GetEraseAttributes(const Page& page) const noexcept;
        void _ScrollRectVertically(const Page& page, const til::rect& scrollRect, const VTInt delta);
        void _ScrollRectHorizontally(const Page& page, const til::rect& scrollRect, const VTInt delta);
        void _InsertDeleteCharacterHelper(const VTInt delta);
        void _InsertDeleteLineHelper(const VTInt delta);
        void _InsertDeleteColumnHelper(const VTInt delta);
        void _ScrollMovement(const VTInt delta);

        void _DoSetTopBottomScrollingMargins(const VTInt topMargin,
                                             const VTInt bottomMargin,
                                             const bool homeCursor = false);
        void _DoSetLeftRightScrollingMargins(const VTInt leftMargin,
                                             const VTInt rightMargin,
                                             const bool homeCursor = false);

        bool _DoLineFeed(const Page& page, const bool withReturn, const bool wrapForced);

        void _DeviceStatusReport(const wchar_t* parameters) const;
        void _CursorPositionReport(const bool extendedReport);
        void _MacroSpaceReport() const;
        void _MacroChecksumReport(const VTParameter id) const;

        void _SetColumnMode(const bool enable);
        void _SetAlternateScreenBufferMode(const bool enable);
        void _ModeParamsHelper(const DispatchTypes::ModeParams param, const bool enable);

        void _ClearSingleTabStop();
        void _ClearAllTabStops() noexcept;
        void _InitTabStopsForWidth(const VTInt width);

        void _ReportColorTable(const DispatchTypes::ColorModel colorModel) const;
        StringHandler _RestoreColorTable();

        void _ReportSGRSetting() const;
        void _ReportDECSTBMSetting();
        void _ReportDECSLRMSetting();
        void _ReportDECSCUSRSetting() const;
        void _ReportDECSCASetting() const;
        void _ReportDECSACESetting() const;
        void _ReportDECACSetting(const VTInt itemNumber) const;

        void _ReportCursorInformation();
        StringHandler _RestoreCursorInformation();
        void _ReportTabStops();
        StringHandler _RestoreTabStops();

        void _ReturnCsiResponse(const std::wstring_view response) const;
        void _ReturnDcsResponse(const std::wstring_view response) const;
        void _ReturnOscResponse(const std::wstring_view response) const;

        std::vector<uint8_t> _tabStopColumns;
        bool _initDefaultTabStops = true;

        ITerminalApi& _api;
        Renderer* _renderer;
        RenderSettings& _renderSettings;
        TerminalInput& _terminalInput;
        TerminalOutput _termOutput;
        PageManager _pages;
        friend class SixelParser;
        std::shared_ptr<SixelParser> _sixelParser;
        std::unique_ptr<FontBuffer> _fontBuffer;
        std::shared_ptr<MacroBuffer> _macroBuffer;
        std::optional<unsigned int> _initialCodePage;
        til::enumset<OptionalFeature> _optionalFeatures = { OptionalFeature::ClipboardWrite };

        // We have two instances of the saved cursor state, because we need
        // one for the main buffer (at index 0), and another for the alt buffer
        // (at index 1). The _usingAltBuffer property keeps tracks of which
        // buffer is active, so can be used as an index into this array to
        // obtain the saved state that should be currently active.
        std::array<CursorState, 2> _savedCursorState;
        bool _usingAltBuffer;

        til::inclusive_rect _scrollMargins;

        til::enumset<Mode> _modes{ Mode::PageCursorCoupling };

        SgrStack _sgrStack;

        void _SetUnderlineStyleHelper(const VTParameter option, TextAttribute& attr) noexcept;
        size_t _SetRgbColorsHelper(const VTParameters options,
                                   TextAttribute& attr,
                                   const bool isForeground) noexcept;
        void _SetRgbColorsHelperFromSubParams(const VTParameter colorItem,
                                              const VTSubParameters options,
                                              TextAttribute& attr) noexcept;
        size_t _ApplyGraphicsOption(const VTParameters options,
                                    const size_t optionIndex,
                                    TextAttribute& attr) noexcept;
        void _ApplyGraphicsOptionWithSubParams(const VTParameter option,
                                               const VTSubParameters subParams,
                                               TextAttribute& attr) noexcept;
        void _ApplyGraphicsOptions(const VTParameters options,
                                   TextAttribute& attr) noexcept;

#ifdef UNIT_TESTING
        friend class AdapterTest;
#endif
    };
}
