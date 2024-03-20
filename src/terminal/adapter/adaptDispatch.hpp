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
        AdaptDispatch(ITerminalApi& api, Renderer& renderer, RenderSettings& renderSettings, TerminalInput& terminalInput);

        void Print(const wchar_t wchPrintable) override;
        void PrintString(const std::wstring_view string) override;

        bool CursorUp(const VTInt distance) override; // CUU
        bool CursorDown(const VTInt distance) override; // CUD
        bool CursorForward(const VTInt distance) override; // CUF
        bool CursorBackward(const VTInt distance) override; // CUB, BS
        bool CursorNextLine(const VTInt distance) override; // CNL
        bool CursorPrevLine(const VTInt distance) override; // CPL
        bool CursorHorizontalPositionAbsolute(const VTInt column) override; // HPA, CHA
        bool VerticalLinePositionAbsolute(const VTInt line) override; // VPA
        bool HorizontalPositionRelative(const VTInt distance) override; // HPR
        bool VerticalPositionRelative(const VTInt distance) override; // VPR
        bool CursorPosition(const VTInt line, const VTInt column) override; // CUP, HVP
        bool CursorSaveState() override; // DECSC
        bool CursorRestoreState() override; // DECRC
        bool EraseInDisplay(const DispatchTypes::EraseType eraseType) override; // ED
        bool EraseInLine(const DispatchTypes::EraseType eraseType) override; // EL
        bool EraseCharacters(const VTInt numChars) override; // ECH
        bool SelectiveEraseInDisplay(const DispatchTypes::EraseType eraseType) override; // DECSED
        bool SelectiveEraseInLine(const DispatchTypes::EraseType eraseType) override; // DECSEL
        bool InsertCharacter(const VTInt count) override; // ICH
        bool DeleteCharacter(const VTInt count) override; // DCH
        bool ChangeAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs) override; // DECCARA
        bool ReverseAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs) override; // DECRARA
        bool CopyRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTInt page, const VTInt dstTop, const VTInt dstLeft, const VTInt dstPage) override; // DECCRA
        bool FillRectangularArea(const VTParameter ch, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override; // DECFRA
        bool EraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override; // DECERA
        bool SelectiveEraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override; // DECSERA
        bool SelectAttributeChangeExtent(const DispatchTypes::ChangeExtent changeExtent) noexcept override; // DECSACE
        bool RequestChecksumRectangularArea(const VTInt id, const VTInt page, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override; // DECRQCRA
        bool SetGraphicsRendition(const VTParameters options) override; // SGR
        bool SetLineRendition(const LineRendition rendition) override; // DECSWL, DECDWL, DECDHL
        bool SetCharacterProtectionAttribute(const VTParameters options) override; // DECSCA
        bool PushGraphicsRendition(const VTParameters options) override; // XTPUSHSGR
        bool PopGraphicsRendition() override; // XTPOPSGR
        bool DeviceStatusReport(const DispatchTypes::StatusType statusType, const VTParameter id) override; // DSR
        bool DeviceAttributes() override; // DA1
        bool SecondaryDeviceAttributes() override; // DA2
        bool TertiaryDeviceAttributes() override; // DA3
        bool Vt52DeviceAttributes() override; // VT52 Identify
        bool RequestTerminalParameters(const DispatchTypes::ReportingPermission permission) override; // DECREQTPARM
        bool ScrollUp(const VTInt distance) override; // SU
        bool ScrollDown(const VTInt distance) override; // SD
        bool InsertLine(const VTInt distance) override; // IL
        bool DeleteLine(const VTInt distance) override; // DL
        bool InsertColumn(const VTInt distance) override; // DECIC
        bool DeleteColumn(const VTInt distance) override; // DECDC
        bool SetMode(const DispatchTypes::ModeParams param) override; // SM, DECSET
        bool ResetMode(const DispatchTypes::ModeParams param) override; // RM, DECRST
        bool RequestMode(const DispatchTypes::ModeParams param) override; // DECRQM
        bool SetKeypadMode(const bool applicationMode) override; // DECKPAM, DECKPNM
        bool SetAnsiMode(const bool ansiMode) override; // DECANM
        bool SetTopBottomScrollingMargins(const VTInt topMargin,
                                          const VTInt bottomMargin) override; // DECSTBM
        bool SetLeftRightScrollingMargins(const VTInt leftMargin,
                                          const VTInt rightMargin) override; // DECSLRM
        bool WarningBell() override; // BEL
        bool CarriageReturn() override; // CR
        bool LineFeed(const DispatchTypes::LineFeedType lineFeedType) override; // IND, NEL, LF, FF, VT
        bool ReverseLineFeed() override; // RI
        bool BackIndex() override; // DECBI
        bool ForwardIndex() override; // DECFI
        bool SetWindowTitle(const std::wstring_view title) override; // DECSWT, OSCWindowTitle
        bool HorizontalTabSet() override; // HTS
        bool ForwardTab(const VTInt numTabs) override; // CHT, HT
        bool BackwardsTab(const VTInt numTabs) override; // CBT
        bool TabClear(const DispatchTypes::TabClearType clearType) override; // TBC
        bool TabSet(const VTParameter setType) noexcept override; // DECST8C
        bool DesignateCodingSystem(const VTID codingSystem) override; // DOCS
        bool Designate94Charset(const VTInt gsetNumber, const VTID charset) override; // SCS
        bool Designate96Charset(const VTInt gsetNumber, const VTID charset) override; // SCS
        bool LockingShift(const VTInt gsetNumber) override; // LS0, LS1, LS2, LS3
        bool LockingShiftRight(const VTInt gsetNumber) override; // LS1R, LS2R, LS3R
        bool SingleShift(const VTInt gsetNumber) noexcept override; // SS2, SS3
        bool AcceptC1Controls(const bool enabled) override; // DECAC1
        bool AnnounceCodeStructure(const VTInt ansiLevel) override; // ACS
        bool SoftReset() override; // DECSTR
        bool HardReset() override; // RIS
        bool ScreenAlignmentPattern() override; // DECALN
        bool SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) override; // DECSCUSR
        bool SetCursorColor(const COLORREF cursorColor) override;

        bool SetClipboard(const std::wstring_view content) override; // OSCSetClipboard

        bool SetColorTableEntry(const size_t tableIndex,
                                const DWORD color) override; // OSCColorTable
        bool SetDefaultForeground(const DWORD color) override; // OSCDefaultForeground
        bool SetDefaultBackground(const DWORD color) override; // OSCDefaultBackground
        bool AssignColor(const DispatchTypes::ColorItem item, const VTInt fgIndex, const VTInt bgIndex) override; // DECAC

        bool WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                const VTParameter parameter1,
                                const VTParameter parameter2) override; // DTTERM_WindowManipulation

        bool AddHyperlink(const std::wstring_view uri, const std::wstring_view params) override;
        bool EndHyperlink() override;

        bool DoConEmuAction(const std::wstring_view string) override;

        bool DoITerm2Action(const std::wstring_view string) override;

        bool DoFinalTermAction(const std::wstring_view string) override;

        bool DoVsCodeAction(const std::wstring_view string) override;

        StringHandler DownloadDRCS(const VTInt fontNumber,
                                   const VTParameter startChar,
                                   const DispatchTypes::DrcsEraseControl eraseControl,
                                   const DispatchTypes::DrcsCellMatrix cellMatrix,
                                   const DispatchTypes::DrcsFontSet fontSet,
                                   const DispatchTypes::DrcsFontUsage fontUsage,
                                   const VTParameter cellHeight,
                                   const DispatchTypes::CharsetSize charsetSize) override; // DECDLD

        bool RequestUserPreferenceCharset() override; // DECRQUPSS
        StringHandler AssignUserPreferenceCharset(const DispatchTypes::CharsetSize charsetSize) override; // DECAUPSS

        StringHandler DefineMacro(const VTInt macroId,
                                  const DispatchTypes::MacroDeleteControl deleteControl,
                                  const DispatchTypes::MacroEncoding encoding) override; // DECDMAC
        bool InvokeMacro(const VTInt macroId) override; // DECINVM

        StringHandler RestoreTerminalState(const DispatchTypes::ReportFormat format) override; // DECRSTS

        StringHandler RequestSetting() override; // DECRQSS

        bool RequestPresentationStateReport(const DispatchTypes::PresentationReportFormat format) override; // DECRQPSR
        StringHandler RestorePresentationState(const DispatchTypes::PresentationReportFormat format) override; // DECRSPS

        bool PlaySounds(const VTParameters parameters) override; // DECPS

    private:
        enum class Mode
        {
            InsertReplace,
            Origin,
            Column,
            AllowDECCOLM,
            AllowDECSLRM,
            EraseColor,
            RectangularChangeExtent
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
        std::pair<int, int> _GetVerticalMargins(const til::rect& viewport, const bool absolute) noexcept;
        std::pair<int, int> _GetHorizontalMargins(const til::CoordType bufferWidth) noexcept;
        bool _CursorMovePosition(const Offset rowOffset, const Offset colOffset, const bool clampInMargins);
        void _ApplyCursorMovementFlags(Cursor& cursor) noexcept;
        void _FillRect(TextBuffer& textBuffer, const til::rect& fillRect, const std::wstring_view& fillChar, const TextAttribute& fillAttrs) const;
        void _SelectiveEraseRect(TextBuffer& textBuffer, const til::rect& eraseRect);
        void _ChangeRectAttributes(TextBuffer& textBuffer, const til::rect& changeRect, const ChangeOps& changeOps);
        void _ChangeRectOrStreamAttributes(const til::rect& changeArea, const ChangeOps& changeOps);
        til::rect _CalculateRectArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const til::size bufferSize);
        bool _EraseScrollback();
        bool _EraseAll();
        TextAttribute _GetEraseAttributes(const TextBuffer& textBuffer) const noexcept;
        void _ScrollRectVertically(TextBuffer& textBuffer, const til::rect& scrollRect, const VTInt delta);
        void _ScrollRectHorizontally(TextBuffer& textBuffer, const til::rect& scrollRect, const VTInt delta);
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

        void _DoLineFeed(TextBuffer& textBuffer, const bool withReturn, const bool wrapForced);

        void _DeviceStatusReport(const wchar_t* parameters) const;
        void _CursorPositionReport(const bool extendedReport);
        void _MacroSpaceReport() const;
        void _MacroChecksumReport(const VTParameter id) const;

        void _SetColumnMode(const bool enable);
        void _SetAlternateScreenBufferMode(const bool enable);
        bool _PassThroughInputModes();
        bool _ModeParamsHelper(const DispatchTypes::ModeParams param, const bool enable);

        void _ClearSingleTabStop();
        void _ClearAllTabStops() noexcept;
        void _InitTabStopsForWidth(const VTInt width);

        StringHandler _RestoreColorTable();

        void _ReportSGRSetting() const;
        void _ReportDECSTBMSetting();
        void _ReportDECSLRMSetting();
        void _ReportDECSCASetting() const;
        void _ReportDECSACESetting() const;
        void _ReportDECACSetting(const VTInt itemNumber) const;

        void _ReportCursorInformation();
        StringHandler _RestoreCursorInformation();
        void _ReportTabStops();
        StringHandler _RestoreTabStops();

        StringHandler _CreateDrcsPassthroughHandler(const DispatchTypes::CharsetSize charsetSize);
        StringHandler _CreatePassthroughHandler();

        std::vector<uint8_t> _tabStopColumns;
        bool _initDefaultTabStops = true;

        ITerminalApi& _api;
        Renderer& _renderer;
        RenderSettings& _renderSettings;
        TerminalInput& _terminalInput;
        TerminalOutput _termOutput;
        std::unique_ptr<FontBuffer> _fontBuffer;
        std::shared_ptr<MacroBuffer> _macroBuffer;
        std::optional<unsigned int> _initialCodePage;

        // We have two instances of the saved cursor state, because we need
        // one for the main buffer (at index 0), and another for the alt buffer
        // (at index 1). The _usingAltBuffer property keeps tracks of which
        // buffer is active, so can be used as an index into this array to
        // obtain the saved state that should be currently active.
        std::array<CursorState, 2> _savedCursorState;
        bool _usingAltBuffer;

        til::inclusive_rect _scrollMargins;

        til::enumset<Mode> _modes;

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
