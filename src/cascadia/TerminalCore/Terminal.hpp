// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <conattrs.hpp>

#include "../../buffer/out/textBuffer.hpp"
#include "../../types/inc/sgrStack.hpp"
#include "../../renderer/inc/BlinkingState.hpp"
#include "../../terminal/parser/StateMachine.hpp"
#include "../../terminal/input/terminalInput.hpp"

#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/IUiaData.h"
#include "../../cascadia/terminalcore/ITerminalApi.hpp"
#include "../../cascadia/terminalcore/ITerminalInput.hpp"

static constexpr std::wstring_view linkPattern{ LR"(\b(https?|ftp|file)://[-A-Za-z0-9+&@#/%?=~_|$!:,.;]*[A-Za-z0-9+&@#/%=~_|$])" };
static constexpr size_t TaskbarMinProgress{ 10 };

// You have to forward decl the ICoreSettings here, instead of including the header.
// If you include the header, there will be compilation errors with other
//      headers that include Terminal.hpp
namespace winrt::Microsoft::Terminal::Core
{
    struct ICoreSettings;
    struct ICoreAppearance;
}

namespace Microsoft::Terminal::Core
{
    class Terminal;
}

// fwdecl unittest classes
#ifdef UNIT_TESTING
namespace TerminalCoreUnitTests
{
    class TerminalBufferTests;
    class TerminalApiTest;
    class ConptyRoundtripTests;
    class ScrollTest;
};
#endif

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

    void CreateFromSettings(winrt::Microsoft::Terminal::Core::ICoreSettings settings,
                            Microsoft::Console::Render::IRenderTarget& renderTarget);

    void UpdateSettings(winrt::Microsoft::Terminal::Core::ICoreSettings settings);
    void UpdateAppearance(const winrt::Microsoft::Terminal::Core::ICoreAppearance& appearance);

    // Write goes through the parser
    void Write(std::wstring_view stringView);

    // WritePastedText goes directly to the connection
    void WritePastedText(std::wstring_view stringView);

    [[nodiscard]] std::shared_lock<std::shared_mutex> LockForReading();
    [[nodiscard]] std::unique_lock<std::shared_mutex> LockForWriting();

    short GetBufferHeight() const noexcept;

    int ViewStartIndex() const noexcept;
    int ViewEndIndex() const noexcept;

#pragma region ITerminalApi
    // These methods are defined in TerminalApi.cpp
    bool PrintString(std::wstring_view stringView) noexcept override;
    bool ExecuteChar(wchar_t wch) noexcept override;
    TextAttribute GetTextAttributes() const noexcept override;
    void SetTextAttributes(const TextAttribute& attrs) noexcept override;
    Microsoft::Console::Types::Viewport GetBufferSize() noexcept override;
    bool SetCursorPosition(short x, short y) noexcept override;
    COORD GetCursorPosition() noexcept override;
    bool SetCursorVisibility(const bool visible) noexcept override;
    bool EnableCursorBlinking(const bool enable) noexcept override;
    bool CursorLineFeed(const bool withReturn) noexcept override;
    bool DeleteCharacter(const size_t count) noexcept override;
    bool InsertCharacter(const size_t count) noexcept override;
    bool EraseCharacters(const size_t numChars) noexcept override;
    bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept override;
    bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept override;
    bool WarningBell() noexcept override;
    bool SetWindowTitle(std::wstring_view title) noexcept override;
    bool SetColorTableEntry(const size_t tableIndex, const COLORREF color) noexcept override;
    bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) noexcept override;
    bool SetCursorColor(const COLORREF color) noexcept override;
    bool SetDefaultForeground(const COLORREF color) noexcept override;
    bool SetDefaultBackground(const COLORREF color) noexcept override;

    bool EnableWin32InputMode(const bool win32InputMode) noexcept override;
    bool SetCursorKeysMode(const bool applicationMode) noexcept override;
    bool SetKeypadMode(const bool applicationMode) noexcept override;
    bool SetScreenMode(const bool reverseMode) noexcept override;
    bool EnableVT200MouseMode(const bool enabled) noexcept override;
    bool EnableUTF8ExtendedMouseMode(const bool enabled) noexcept override;
    bool EnableSGRExtendedMouseMode(const bool enabled) noexcept override;
    bool EnableButtonEventMouseMode(const bool enabled) noexcept override;
    bool EnableAnyEventMouseMode(const bool enabled) noexcept override;
    bool EnableAlternateScrollMode(const bool enabled) noexcept override;
    bool EnableXtermBracketedPasteMode(const bool enabled) noexcept override;
    bool IsXtermBracketedPasteModeEnabled() const noexcept override;

    bool IsVtInputEnabled() const noexcept override;

    bool CopyToClipboard(std::wstring_view content) noexcept override;

    bool AddHyperlink(std::wstring_view uri, std::wstring_view params) noexcept override;
    bool EndHyperlink() noexcept override;

    bool SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress) noexcept override;
    bool SetWorkingDirectory(std::wstring_view uri) noexcept override;
    std::wstring_view GetWorkingDirectory() noexcept override;

    bool PushGraphicsRendition(const ::Microsoft::Console::VirtualTerminal::VTParameters options) noexcept override;
    bool PopGraphicsRendition() noexcept override;

#pragma endregion

#pragma region ITerminalInput
    // These methods are defined in Terminal.cpp
    bool SendKeyEvent(const WORD vkey, const WORD scanCode, const Microsoft::Terminal::Core::ControlKeyStates states, const bool keyDown) override;
    bool SendMouseEvent(const COORD viewportPos, const unsigned int uiButton, const ControlKeyStates states, const short wheelDelta, const Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState state) override;
    bool SendCharEvent(const wchar_t ch, const WORD scanCode, const ControlKeyStates states) override;

    [[nodiscard]] HRESULT UserResize(const COORD viewportSize) noexcept override;
    void UserScrollViewport(const int viewTop) override;
    int GetScrollOffset() noexcept override;

    void TrySnapOnInput() override;
    bool IsTrackingMouseInput() const noexcept;

    std::wstring GetHyperlinkAtPosition(const COORD position);
    uint16_t GetHyperlinkIdAtPosition(const COORD position);
    std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> GetHyperlinkIntervalFromPosition(const COORD position);
#pragma endregion

#pragma region IBaseData(base to IRenderData and IUiaData)
    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    COORD GetTextBufferEndPosition() const noexcept override;
    const TextBuffer& GetTextBuffer() noexcept override;
    const FontInfo& GetFontInfo() noexcept override;

    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;
#pragma endregion

#pragma region IRenderData
    // These methods are defined in TerminalRenderData.cpp
    const TextAttribute GetDefaultBrushColors() noexcept override;
    std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept override;
    COORD GetCursorPosition() const noexcept override;
    bool IsCursorVisible() const noexcept override;
    bool IsCursorOn() const noexcept override;
    ULONG GetCursorHeight() const noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    CursorType GetCursorStyle() const noexcept override;
    COLORREF GetCursorColor() const noexcept override;
    bool IsCursorDoubleWidth() const override;
    bool IsScreenReversed() const noexcept override;
    const std::vector<Microsoft::Console::Render::RenderOverlay> GetOverlays() const noexcept override;
    const bool IsGridLineDrawingAllowed() noexcept override;
    const std::wstring GetHyperlinkUri(uint16_t id) const noexcept override;
    const std::wstring GetHyperlinkCustomId(uint16_t id) const noexcept override;
    const std::vector<size_t> GetPatternId(const COORD location) const noexcept override;
#pragma endregion

#pragma region IUiaData
    std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept override;
    const bool IsSelectionActive() const noexcept override;
    const bool IsBlockSelection() const noexcept override;
    void ClearSelection() override;
    void SelectNewRegion(const COORD coordStart, const COORD coordEnd) override;
    const COORD GetSelectionAnchor() const noexcept override;
    const COORD GetSelectionEnd() const noexcept override;
    const std::wstring_view GetConsoleTitle() const noexcept override;
    void ColorSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd, const TextAttribute) override;
#pragma endregion

    void SetWriteInputCallback(std::function<void(std::wstring&)> pfn) noexcept;
    void SetWarningBellCallback(std::function<void()> pfn) noexcept;
    void SetTitleChangedCallback(std::function<void(std::wstring_view)> pfn) noexcept;
    void SetTabColorChangedCallback(std::function<void(const std::optional<til::color>)> pfn) noexcept;
    void SetCopyToClipboardCallback(std::function<void(std::wstring_view)> pfn) noexcept;
    void SetScrollPositionChangedCallback(std::function<void(const int, const int, const int)> pfn) noexcept;
    void SetCursorPositionChangedCallback(std::function<void()> pfn) noexcept;
    void SetBackgroundCallback(std::function<void(const til::color)> pfn) noexcept;
    void TaskbarProgressChangedCallback(std::function<void()> pfn) noexcept;

    void SetCursorOn(const bool isOn);
    bool IsCursorBlinkingAllowed() const noexcept;

    void UpdatePatternsUnderLock() noexcept;
    void ClearPatternTree() noexcept;

    const std::optional<til::color> GetTabColor() const noexcept;
    til::color GetDefaultBackground() const noexcept;

    Microsoft::Console::Render::BlinkingState& GetBlinkingState() const noexcept;

    const size_t GetTaskbarState() const noexcept;
    const size_t GetTaskbarProgress() const noexcept;

#pragma region TextSelection
    // These methods are defined in TerminalSelection.cpp
    enum class SelectionExpansionMode
    {
        Cell,
        Word,
        Line
    };
    void MultiClickSelection(const COORD viewportPos, SelectionExpansionMode expansionMode);
    void SetSelectionAnchor(const COORD position);
    void SetSelectionEnd(const COORD position, std::optional<SelectionExpansionMode> newExpansionMode = std::nullopt);
    void SetBlockSelection(const bool isEnabled) noexcept;

    const TextBuffer::TextAndColor RetrieveSelectedTextFromBuffer(bool trimTrailingWhitespace);
#pragma endregion

private:
    std::function<void(std::wstring&)> _pfnWriteInput;
    std::function<void()> _pfnWarningBell;
    std::function<void(std::wstring_view)> _pfnTitleChanged;
    std::function<void(std::wstring_view)> _pfnCopyToClipboard;
    std::function<void(const int, const int, const int)> _pfnScrollPositionChanged;
    std::function<void(const til::color)> _pfnBackgroundColorChanged;
    std::function<void()> _pfnCursorPositionChanged;
    std::function<void(const std::optional<til::color>)> _pfnTabColorChanged;
    std::function<void()> _pfnTaskbarProgressChanged;

    std::unique_ptr<::Microsoft::Console::VirtualTerminal::StateMachine> _stateMachine;
    std::unique_ptr<::Microsoft::Console::VirtualTerminal::TerminalInput> _terminalInput;

    std::optional<std::wstring> _title;
    std::wstring _startingTitle;
    std::optional<til::color> _tabColor;
    std::optional<til::color> _startingTabColor;

    // This is still stored as a COLORREF because it interacts with some code in ConTypes
    std::array<COLORREF, XTERM_COLOR_TABLE_SIZE> _colorTable;
    til::color _defaultFg;
    til::color _defaultBg;
    CursorType _defaultCursorShape;
    bool _screenReversed;
    mutable Microsoft::Console::Render::BlinkingState _blinkingState;

    bool _snapOnInput;
    bool _altGrAliasing;
    bool _suppressApplicationTitle;
    bool _bracketedPasteMode;
    bool _trimBlockSelection;

    size_t _taskbarState;
    size_t _taskbarProgress;

    size_t _hyperlinkPatternId;

    std::wstring _workingDirectory;
#pragma region Text Selection
    // a selection is represented as a range between two COORDs (start and end)
    // the pivot is the COORD that remains selected when you extend a selection in any direction
    //   this is particularly useful when a word selection is extended over its starting point
    //   see TerminalSelection.cpp for more information
    struct SelectionAnchors
    {
        COORD start;
        COORD end;
        COORD pivot;
    };
    std::optional<SelectionAnchors> _selection;
    bool _blockSelection;
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
    // We might want to store the height in the scrollback that's currently visible.
    // Think on this some more.
    // For example: While looking at the scrollback, we probably want the visible region to "stick"
    //   to the region they scrolled to. If that were the case, then every time we move _mutableViewport,
    //   we'd also need to update _offset.
    // However, if we just stored it as a _visibleTop, then that point would remain fixed -
    //      Though if _visibleTop == _mutableViewport.Top, then we'd need to make sure to update
    //      _visibleTop as well.
    // Additionally, maybe some people want to scroll into the history, then have that scroll out from
    //      underneath them, while others would prefer to anchor it in place.
    //      Either way, we should make this behavior controlled by a setting.

    interval_tree::IntervalTree<til::point, size_t> _patternIntervalTree;
    void _InvalidatePatternTree(interval_tree::IntervalTree<til::point, size_t>& tree);
    void _InvalidateFromCoords(const COORD start, const COORD end);

    // Since virtual keys are non-zero, you assume that this field is empty/invalid if it is.
    struct KeyEventCodes
    {
        WORD VirtualKey;
        WORD ScanCode;
    };
    std::optional<KeyEventCodes> _lastKeyEventCodes;

    static WORD _ScanCodeFromVirtualKey(const WORD vkey) noexcept;
    static WORD _VirtualKeyFromScanCode(const WORD scanCode) noexcept;
    static WORD _VirtualKeyFromCharacter(const wchar_t ch) noexcept;
    static wchar_t _CharacterFromKeyEvent(const WORD vkey, const WORD scanCode, const ControlKeyStates states) noexcept;

    void _StoreKeyEvent(const WORD vkey, const WORD scanCode);
    WORD _TakeVirtualKeyFromLastKeyEvent(const WORD scanCode) noexcept;

    int _VisibleStartIndex() const noexcept;
    int _VisibleEndIndex() const noexcept;

    Microsoft::Console::Types::Viewport _GetMutableViewport() const noexcept;
    Microsoft::Console::Types::Viewport _GetVisibleViewport() const noexcept;

    void _InitializeColorTable();

    void _WriteBuffer(const std::wstring_view& stringView);

    void _AdjustCursorPosition(const COORD proposedPosition);

    void _NotifyScrollEvent() noexcept;

    void _NotifyTerminalCursorPositionChanged() noexcept;

#pragma region TextSelection
    // These methods are defined in TerminalSelection.cpp
    std::vector<SMALL_RECT> _GetSelectionRects() const noexcept;
    std::pair<COORD, COORD> _PivotSelection(const COORD targetPos, bool& targetStart) const;
    std::pair<COORD, COORD> _ExpandSelectionAnchors(std::pair<COORD, COORD> anchors) const;
    COORD _ConvertToBufferCell(const COORD viewportPos) const;
#pragma endregion

    Microsoft::Console::VirtualTerminal::SgrStack _sgrStack;

#ifdef UNIT_TESTING
    friend class TerminalCoreUnitTests::TerminalBufferTests;
    friend class TerminalCoreUnitTests::TerminalApiTest;
    friend class TerminalCoreUnitTests::ConptyRoundtripTests;
    friend class TerminalCoreUnitTests::ScrollTest;
#endif
};
