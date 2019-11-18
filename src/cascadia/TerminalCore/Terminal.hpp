// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <conattrs.hpp>

#include "../../buffer/out/textBuffer.hpp"
#include "../../renderer/inc/IRenderData.hpp"
#include "../../terminal/parser/StateMachine.hpp"
#include "../../terminal/input/terminalInput.hpp"

#include "../../types/inc/Viewport.hpp"
#include "../../types/IUiaData.h"
#include "../../cascadia/terminalcore/ITerminalApi.hpp"
#include "../../cascadia/terminalcore/ITerminalInput.hpp"

// You have to forward decl the ICoreSettings here, instead of including the header.
// If you include the header, there will be compilation errors with other
//      headers that include Terminal.hpp
namespace winrt::Microsoft::Terminal::Settings
{
    struct ICoreSettings;
}

namespace Microsoft::Terminal::Core
{
    class Terminal;
}

class Microsoft::Terminal::Core::Terminal final :
    public Microsoft::Terminal::Core::ITerminalApi,
    public Microsoft::Terminal::Core::ITerminalInput,
    public Microsoft::Console::Render::IRenderData,
    public Microsoft::Console::Types::IUiaData
{
public:
    Terminal();
    ~Terminal(){};
    Terminal(const Terminal&) = default;
    Terminal(Terminal&&) = default;
    Terminal& operator=(const Terminal&) = default;
    Terminal& operator=(Terminal&&) = default;

    void Create(COORD viewportSize,
                SHORT scrollbackLines,
                Microsoft::Console::Render::IRenderTarget& renderTarget);

    void CreateFromSettings(winrt::Microsoft::Terminal::Settings::ICoreSettings settings,
                            Microsoft::Console::Render::IRenderTarget& renderTarget);

    void UpdateSettings(winrt::Microsoft::Terminal::Settings::ICoreSettings settings);

    // Write goes through the parser
    void Write(std::wstring_view stringView);

    [[nodiscard]] std::shared_lock<std::shared_mutex> LockForReading();
    [[nodiscard]] std::unique_lock<std::shared_mutex> LockForWriting();

    short GetBufferHeight() const noexcept;

#pragma region ITerminalApi
    // These methods are defined in TerminalApi.cpp
    bool PrintString(std::wstring_view stringView) override;
    bool ExecuteChar(wchar_t wch) override;
    bool SetTextToDefaults(bool foreground, bool background) override;
    bool SetTextForegroundIndex(BYTE colorIndex) override;
    bool SetTextBackgroundIndex(BYTE colorIndex) override;
    bool SetTextRgbColor(COLORREF color, bool foreground) override;
    bool BoldText(bool boldOn) override;
    bool UnderlineText(bool underlineOn) override;
    bool ReverseText(bool reversed) override;
    bool SetCursorPosition(short x, short y) override;
    COORD GetCursorPosition() override;
    bool DeleteCharacter(const unsigned int uiCount) override;
    bool InsertCharacter(const unsigned int uiCount) override;
    bool EraseCharacters(const unsigned int numChars) override;
    bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) override;
    bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) override;
    bool SetWindowTitle(std::wstring_view title) override;
    bool SetColorTableEntry(const size_t tableIndex, const COLORREF dwColor) override;
    bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) override;
    bool SetDefaultForeground(const COLORREF dwColor) override;
    bool SetDefaultBackground(const COLORREF dwColor) override;
#pragma endregion

#pragma region ITerminalInput
    // These methods are defined in Terminal.cpp
    bool SendKeyEvent(const WORD vkey, const WORD scanCode, const Microsoft::Terminal::Core::ControlKeyStates states) override;
    bool SendCharEvent(const wchar_t ch) override;

    [[nodiscard]] HRESULT UserResize(const COORD viewportSize) noexcept override;
    void UserScrollViewport(const int viewTop) override;
    int GetScrollOffset() override;

    void TrySnapOnInput() override;
#pragma endregion

#pragma region IBaseData(base to IRenderData and IUiaData)
    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    const TextBuffer& GetTextBuffer() noexcept override;
    const FontInfo& GetFontInfo() noexcept override;

    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;
#pragma endregion

#pragma region IRenderData
    // These methods are defined in TerminalRenderData.cpp
    const TextAttribute GetDefaultBrushColors() noexcept override;
    const COLORREF GetForegroundColor(const TextAttribute& attr) const noexcept override;
    const COLORREF GetBackgroundColor(const TextAttribute& attr) const noexcept override;
    COORD GetCursorPosition() const noexcept override;
    bool IsCursorVisible() const noexcept override;
    bool IsCursorOn() const noexcept override;
    ULONG GetCursorHeight() const noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    CursorType GetCursorStyle() const noexcept override;
    COLORREF GetCursorColor() const noexcept override;
    bool IsCursorDoubleWidth() const noexcept override;
    const std::vector<Microsoft::Console::Render::RenderOverlay> GetOverlays() const noexcept override;
    const bool IsGridLineDrawingAllowed() noexcept override;
#pragma endregion

#pragma region IUiaData
    std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept override;
    const bool IsSelectionActive() const noexcept;
    void ClearSelection() override;
    void SelectNewRegion(const COORD coordStart, const COORD coordEnd) override;
    const COORD GetSelectionAnchor() const override;
    const std::wstring GetConsoleTitle() const noexcept override;
    void ColorSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd, const TextAttribute) override;
#pragma endregion

    void SetWriteInputCallback(std::function<void(std::wstring&)> pfn) noexcept;
    void SetTitleChangedCallback(std::function<void(const std::wstring_view&)> pfn) noexcept;
    void SetScrollPositionChangedCallback(std::function<void(const int, const int, const int)> pfn) noexcept;
    void SetBackgroundCallback(std::function<void(const uint32_t)> pfn) noexcept;

    void SetCursorVisible(const bool isVisible) noexcept;
    bool IsCursorBlinkingAllowed() const noexcept;

#pragma region TextSelection
    // These methods are defined in TerminalSelection.cpp
    const bool IsCopyOnSelectActive() const noexcept;
    void DoubleClickSelection(const COORD position);
    void TripleClickSelection(const COORD position);
    void SetSelectionAnchor(const COORD position);
    void SetEndSelectionPosition(const COORD position);
    void SetBoxSelection(const bool isEnabled) noexcept;

    const TextBuffer::TextAndColor RetrieveSelectedTextFromBuffer(bool trimTrailingWhitespace) const;
#pragma endregion

private:
    std::function<void(std::wstring&)> _pfnWriteInput;
    std::function<void(const std::wstring_view&)> _pfnTitleChanged;
    std::function<void(const int, const int, const int)> _pfnScrollPositionChanged;
    std::function<void(const uint32_t)> _pfnBackgroundColorChanged;

    std::unique_ptr<::Microsoft::Console::VirtualTerminal::StateMachine> _stateMachine;
    std::unique_ptr<::Microsoft::Console::VirtualTerminal::TerminalInput> _terminalInput;

    std::wstring _title;
    std::wstring _startingTitle;

    std::array<COLORREF, XTERM_COLOR_TABLE_SIZE> _colorTable;
    COLORREF _defaultFg;
    COLORREF _defaultBg;

    bool _snapOnInput;
    bool _suppressApplicationTitle;

#pragma region Text Selection
    enum class SelectionExpansionMode
    {
        Cell,
        Word,
        Line
    };
    enum class DelimiterClass
    {
        ControlChar,
        DelimiterChar,
        RegularChar
    };
    COORD _selectionAnchor;
    COORD _endSelectionPosition;
    bool _boxSelection;
    bool _selectionActive;
    bool _allowSingleCharSelection;
    bool _copyOnSelect;
    SHORT _selectionVerticalOffset;
    std::wstring _wordDelimiters;
    SelectionExpansionMode _multiClickSelectionMode;
#pragma endregion

    std::shared_mutex _readWriteLock;

    // TODO: These members are not shared by an alt-buffer. They should be
    //      encapsulated, such that a Terminal can have both a main and alt buffer.
    std::unique_ptr<TextBuffer> _buffer;
    Microsoft::Console::Types::Viewport _mutableViewport;
    SHORT _scrollbackLines;

    // _scrollOffset is the number of lines above the viewport that are currently visible
    // If _scrollOffset is 0, then the visible region of the buffer is the viewport.
    int _scrollOffset;
    // TODO this might not be the value we want to store.
    // We might want to store the height in the scrollback that's currenty visible.
    // Think on this some more.
    // For example: While looking at the scrollback, we probably want the visible region to "stick"
    //   to the region they scrolled to. If that were the case, then every time we move _mutableViewport,
    //   we'd also need to update _offset.
    // However, if we just stored it as a _visibleTop, then that point would remain fixed -
    //      Though if _visibleTop == _mutableViewport.Top, then we'd need to make sure to update
    //      _visibleTop as well.
    // Additionally, maybe some people want to scroll into the history, then have that scroll out from
    //      underneath them, while others would prefer to anchor it in place.
    //      Either way, we sohould make this behavior controlled by a setting.

    static WORD _ScanCodeFromVirtualKey(const WORD vkey) noexcept;
    static wchar_t _CharacterFromKeyEvent(const WORD vkey, const WORD scanCode, const ControlKeyStates states) noexcept;

    int _ViewStartIndex() const noexcept;
    int _VisibleStartIndex() const noexcept;

    Microsoft::Console::Types::Viewport _GetMutableViewport() const noexcept;
    Microsoft::Console::Types::Viewport _GetVisibleViewport() const noexcept;

    void _InitializeColorTable();

    void _WriteBuffer(const std::wstring_view& stringView);

    void _NotifyScrollEvent();

#pragma region TextSelection
    // These methods are defined in TerminalSelection.cpp
    std::vector<SMALL_RECT> _GetSelectionRects() const noexcept;
    SHORT _ExpandWideGlyphSelectionLeft(const SHORT xPos, const SHORT yPos) const;
    SHORT _ExpandWideGlyphSelectionRight(const SHORT xPos, const SHORT yPos) const;
    COORD _ExpandDoubleClickSelectionLeft(const COORD position) const;
    COORD _ExpandDoubleClickSelectionRight(const COORD position) const;
    DelimiterClass _GetDelimiterClass(const std::wstring_view cellChar) const noexcept;
    COORD _ConvertToBufferCell(const COORD viewportPos) const;
    const bool _IsSingleCellSelection() const noexcept;
    std::tuple<COORD, COORD> _PreprocessSelectionCoords() const;
    SMALL_RECT _GetSelectionRow(const SHORT row, const COORD higherCoord, const COORD lowerCoord) const;
    void _ExpandSelectionRow(SMALL_RECT& selectionRow) const;
#pragma endregion
};
