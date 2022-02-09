// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../../terminal/adapter/termDispatch.hpp"
#include "ITerminalApi.hpp"

static constexpr size_t TaskbarMaxState{ 4 };
static constexpr size_t TaskbarMaxProgress{ 100 };

class TerminalDispatch : public Microsoft::Console::VirtualTerminal::TermDispatch
{
public:
    TerminalDispatch(::Microsoft::Terminal::Core::ITerminalApi& terminalApi) noexcept;

    void Print(const wchar_t wchPrintable) override;
    void PrintString(const std::wstring_view string) override;

    bool SetGraphicsRendition(const ::Microsoft::Console::VirtualTerminal::VTParameters options) override;

    bool PushGraphicsRendition(const ::Microsoft::Console::VirtualTerminal::VTParameters options) override;
    bool PopGraphicsRendition() override;

    bool CursorPosition(const size_t line,
                        const size_t column) override; // CUP

    bool EnableWin32InputMode(const bool win32InputMode) override; // win32-input-mode

    bool CursorVisibility(const bool isVisible) override; // DECTCEM
    bool EnableCursorBlinking(const bool enable) override; // ATT610

    bool CursorForward(const size_t distance) override;
    bool CursorBackward(const size_t distance) override;
    bool CursorUp(const size_t distance) override;

    bool LineFeed(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::LineFeedType lineFeedType) override;

    bool EraseCharacters(const size_t numChars) override;
    bool WarningBell() override;
    bool CarriageReturn() override;
    bool SetWindowTitle(std::wstring_view title) override;

    bool HorizontalTabSet() override; // HTS
    bool ForwardTab(const size_t numTabs) override; // CHT, HT
    bool BackwardsTab(const size_t numTabs) override; // CBT
    bool TabClear(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TabClearType clearType) override; // TBC

    bool SetColorTableEntry(const size_t tableIndex, const DWORD color) override;
    bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) override;
    bool SetCursorColor(const DWORD color) override;

    bool SetClipboard(std::wstring_view content) override;

    bool SetDefaultForeground(const DWORD color) override;
    bool SetDefaultBackground(const DWORD color) override;
    bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) override; // ED
    bool DeleteCharacter(const size_t count) override;
    bool InsertCharacter(const size_t count) override;
    bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) override;

    bool SetCursorKeysMode(const bool applicationMode) override; // DECCKM
    bool SetKeypadMode(const bool applicationMode) override; // DECKPAM, DECKPNM
    bool SetScreenMode(const bool reverseMode) override; // DECSCNM

    bool SoftReset() override; // DECSTR
    bool HardReset() override; // RIS

    bool EnableVT200MouseMode(const bool enabled) override; // ?1000
    bool EnableUTF8ExtendedMouseMode(const bool enabled) override; // ?1005
    bool EnableSGRExtendedMouseMode(const bool enabled) override; // ?1006
    bool EnableButtonEventMouseMode(const bool enabled) override; // ?1002
    bool EnableAnyEventMouseMode(const bool enabled) override; // ?1003
    bool EnableAlternateScroll(const bool enabled) override; // ?1007
    bool EnableXtermBracketedPasteMode(const bool enabled) override; // ?2004

    bool SetMode(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::ModeParams /*param*/) override; // DECSET
    bool ResetMode(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::ModeParams /*param*/) override; // DECRST

    bool AddHyperlink(const std::wstring_view uri, const std::wstring_view params) override;
    bool EndHyperlink() override;

    bool DoConEmuAction(const std::wstring_view string) override;

private:
    ::Microsoft::Terminal::Core::ITerminalApi& _terminalApi;

    std::vector<bool> _tabStopColumns;
    bool _initDefaultTabStops = true;

    size_t _SetRgbColorsHelper(const ::Microsoft::Console::VirtualTerminal::VTParameters options,
                               TextAttribute& attr,
                               const bool isForeground);

    bool _ModeParamsHelper(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::ModeParams param, const bool enable);

    void _ClearSingleTabStop();
    void _ClearAllTabStops();
    void _ResetTabStops();
    void _InitTabStopsForWidth(const size_t width);
};
