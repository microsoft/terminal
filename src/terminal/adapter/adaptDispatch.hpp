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
#include "DispatchCommon.hpp"
#include "conGetSet.hpp"
#include "adaptDefaults.hpp"
#include "terminalOutput.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch : public ITermDispatch
    {
    public:
        AdaptDispatch(std::unique_ptr<ConGetSet> pConApi,
                      std::unique_ptr<AdaptDefaults> pDefaults);

        void Execute(const wchar_t wchControl) override
        {
            _pDefaults->Execute(wchControl);
        }

        void PrintString(const std::wstring_view string) override;
        void Print(const wchar_t wchPrintable) override;

        bool CursorUp(const size_t distance) override; // CUU
        bool CursorDown(const size_t distance) override; // CUD
        bool CursorForward(const size_t distance) override; // CUF
        bool CursorBackward(const size_t distance) override; // CUB, BS
        bool CursorNextLine(const size_t distance) override; // CNL
        bool CursorPrevLine(const size_t distance) override; // CPL
        bool CursorHorizontalPositionAbsolute(const size_t column) override; // HPA, CHA
        bool VerticalLinePositionAbsolute(const size_t line) override; // VPA
        bool HorizontalPositionRelative(const size_t distance) override; // HPR
        bool VerticalPositionRelative(const size_t distance) override; // VPR
        bool CursorPosition(const size_t line, const size_t column) override; // CUP, HVP
        bool CursorSaveState() override; // DECSC
        bool CursorRestoreState() override; // DECRC
        bool CursorVisibility(const bool isVisible) override; // DECTCEM
        bool EraseInDisplay(const DispatchTypes::EraseType eraseType) override; // ED
        bool EraseInLine(const DispatchTypes::EraseType eraseType) override; // EL
        bool EraseCharacters(const size_t numChars) override; // ECH
        bool InsertCharacter(const size_t count) override; // ICH
        bool DeleteCharacter(const size_t count) override; // DCH
        bool SetGraphicsRendition(const std::basic_string_view<DispatchTypes::GraphicsOptions> options) override; // SGR
        bool DeviceStatusReport(const DispatchTypes::AnsiStatusType statusType) override; // DSR, DSR-OS, DSR-CPR
        bool DeviceAttributes() override; // DA1
        bool Vt52DeviceAttributes() override; // VT52 Identify
        bool ScrollUp(const size_t distance) override; // SU
        bool ScrollDown(const size_t distance) override; // SD
        bool InsertLine(const size_t distance) override; // IL
        bool DeleteLine(const size_t distance) override; // DL
        bool SetColumns(const size_t columns) override; // DECCOLM
        bool SetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> params) override; // DECSET
        bool ResetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> params) override; // DECRST
        bool SetCursorKeysMode(const bool applicationMode) override; // DECCKM
        bool SetKeypadMode(const bool applicationMode) override; // DECKPAM, DECKPNM
        bool EnableWin32InputMode(const bool win32InputMode) override; // win32-input-mode
        bool EnableCursorBlinking(const bool enable) override; // ATT610
        bool SetAnsiMode(const bool ansiMode) override; // DECANM
        bool SetScreenMode(const bool reverseMode) override; // DECSCNM
        bool SetOriginMode(const bool relativeMode) noexcept override; // DECOM
        bool SetAutoWrapMode(const bool wrapAtEOL) override; // DECAWM
        bool SetTopBottomScrollingMargins(const size_t topMargin,
                                          const size_t bottomMargin) override; // DECSTBM
        bool WarningBell() override; // BEL
        bool CarriageReturn() override; // CR
        bool LineFeed(const DispatchTypes::LineFeedType lineFeedType) override; // IND, NEL, LF, FF, VT
        bool ReverseLineFeed() override; // RI
        bool SetWindowTitle(const std::wstring_view title) override; // OscWindowTitle
        bool UseAlternateScreenBuffer() override; // ASBSET
        bool UseMainScreenBuffer() override; // ASBRST
        bool HorizontalTabSet() override; // HTS
        bool ForwardTab(const size_t numTabs) override; // CHT, HT
        bool BackwardsTab(const size_t numTabs) override; // CBT
        bool TabClear(const size_t clearType) override; // TBC
        bool DesignateCodingSystem(const wchar_t codingSystem) override; // DOCS
        bool Designate94Charset(const size_t gsetNumber, const std::pair<wchar_t, wchar_t> charset) override; // SCS
        bool Designate96Charset(const size_t gsetNumber, const std::pair<wchar_t, wchar_t> charset) override; // SCS
        bool LockingShift(const size_t gsetNumber) override; // LS0, LS1, LS2, LS3
        bool LockingShiftRight(const size_t gsetNumber) override; // LS1R, LS2R, LS3R
        bool SingleShift(const size_t gsetNumber) override; // SS2, SS3
        bool SoftReset() override; // DECSTR
        bool HardReset() override; // RIS
        bool ScreenAlignmentPattern() override; // DECALN
        bool EnableDECCOLMSupport(const bool enabled) noexcept override; // ?40
        bool EnableVT200MouseMode(const bool enabled) override; // ?1000
        bool EnableUTF8ExtendedMouseMode(const bool enabled) override; // ?1005
        bool EnableSGRExtendedMouseMode(const bool enabled) override; // ?1006
        bool EnableButtonEventMouseMode(const bool enabled) override; // ?1002
        bool EnableAnyEventMouseMode(const bool enabled) override; // ?1003
        bool EnableAlternateScroll(const bool enabled) override; // ?1007
        bool SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) override; // DECSCUSR
        bool SetCursorColor(const COLORREF cursorColor) override;

        bool SetColorTableEntry(const size_t tableIndex,
                                const DWORD color) override; // OscColorTable
        bool SetDefaultForeground(const DWORD color) override; // OSCDefaultForeground
        bool SetDefaultBackground(const DWORD color) override; // OSCDefaultBackground

        bool WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                const std::basic_string_view<size_t> parameters) override; // DTTERM_WindowManipulation

    private:
        enum class ScrollDirection
        {
            Up,
            Down
        };
        struct CursorState
        {
            unsigned int Row = 1;
            unsigned int Column = 1;
            bool IsOriginModeRelative = false;
            TextAttribute Attributes = {};
            TerminalOutput TermOutput = {};
            unsigned int CodePage = 0;
        };
        struct Offset
        {
            int Value;
            bool IsAbsolute;
            // VT origin is at 1,1 so we need to subtract 1 from absolute positions.
            static constexpr Offset Absolute(const size_t value) { return { gsl::narrow_cast<int>(value) - 1, true }; };
            static constexpr Offset Forward(const size_t value) { return { gsl::narrow_cast<int>(value), false }; };
            static constexpr Offset Backward(const size_t value) { return { -gsl::narrow_cast<int>(value), false }; };
            static constexpr Offset Unchanged() { return Forward(0); };
        };

        bool _CursorMovePosition(const Offset rowOffset, const Offset colOffset, const bool clampInMargins) const;
        bool _EraseSingleLineHelper(const CONSOLE_SCREEN_BUFFER_INFOEX& csbiex,
                                    const DispatchTypes::EraseType eraseType,
                                    const size_t lineId) const;
        bool _EraseScrollback();
        bool _EraseAll();
        bool _InsertDeleteHelper(const size_t count, const bool isInsert) const;
        bool _ScrollMovement(const ScrollDirection dir, const size_t distance) const;

        bool _DoSetTopBottomScrollingMargins(const size_t topMargin,
                                             const size_t bottomMargin);
        bool _OperatingStatus() const;
        bool _CursorPositionReport() const;

        bool _WriteResponse(const std::wstring_view reply) const;
        bool _SetResetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> params, const bool enable);
        bool _PrivateModeParamsHelper(const DispatchTypes::PrivateModeParams param, const bool enable);
        bool _DoDECCOLMHelper(const size_t columns);

        bool _ClearSingleTabStop();
        bool _ClearAllTabStops() noexcept;
        void _ResetTabStops() noexcept;
        void _InitTabStopsForWidth(const size_t width);

        bool _ShouldPassThroughInputModeChange() const;

        std::vector<bool> _tabStopColumns;
        bool _initDefaultTabStops = true;

        std::unique_ptr<ConGetSet> _pConApi;
        std::unique_ptr<AdaptDefaults> _pDefaults;
        TerminalOutput _termOutput;
        std::optional<unsigned int> _initialCodePage;

        // We have two instances of the saved cursor state, because we need
        // one for the main buffer (at index 0), and another for the alt buffer
        // (at index 1). The _usingAltBuffer property keeps tracks of which
        // buffer is active, so can be used as an index into this array to
        // obtain the saved state that should be currently active.
        std::array<CursorState, 2> _savedCursorState;
        bool _usingAltBuffer;

        SMALL_RECT _scrollMargins;

        bool _isOriginModeRelative;

        bool _isDECCOLMAllowed;

        size_t _SetRgbColorsHelper(const std::basic_string_view<DispatchTypes::GraphicsOptions> options,
                                   TextAttribute& attr,
                                   const bool isForeground) noexcept;
    };
}
