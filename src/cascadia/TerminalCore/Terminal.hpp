// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <conattrs.hpp>

#include "../../inc/DefaultSettings.h"
#include "../../buffer/out/textBuffer.hpp"
#include "../../renderer/inc/IRenderData.hpp"
#include "../../terminal/adapter/ITerminalApi.hpp"
#include "../../terminal/parser/StateMachine.hpp"
#include "../../terminal/input/terminalInput.hpp"

#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/IUiaData.h"
#include "../../cascadia/terminalcore/ITerminalInput.hpp"

#include <til/ticket_lock.h>

static constexpr std::wstring_view linkPattern{ LR"(\b(https?|ftp|file)://[-A-Za-z0-9+&@#/%?=~_|$!:,.;]*[A-Za-z0-9+&@#/%=~_|$])" };
static constexpr size_t TaskbarMinProgress{ 10 };

// You have to forward decl the ICoreSettings here, instead of including the header.
// If you include the header, there will be compilation errors with other
//      headers that include Terminal.hpp
namespace winrt::Microsoft::Terminal::Core
{
    struct ICoreSettings;
    struct ICoreAppearance;
    struct Scheme;
}

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch;
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
    public Microsoft::Console::VirtualTerminal::ITerminalApi,
    public Microsoft::Terminal::Core::ITerminalInput,
    public Microsoft::Console::Render::IRenderData,
    public Microsoft::Console::Types::IUiaData
{
    using RenderSettings = Microsoft::Console::Render::RenderSettings;

public:
    Terminal();
    ~Terminal(){};
    Terminal(const Terminal&) = default;
    Terminal(Terminal&&) = default;
    Terminal& operator=(const Terminal&) = default;
    Terminal& operator=(Terminal&&) = default;

    void Create(COORD viewportSize,
                SHORT scrollbackLines,
                Microsoft::Console::Render::Renderer& renderer);

    void CreateFromSettings(winrt::Microsoft::Terminal::Core::ICoreSettings settings,
                            Microsoft::Console::Render::Renderer& renderer);

    void UpdateSettings(winrt::Microsoft::Terminal::Core::ICoreSettings settings);
    void UpdateAppearance(const winrt::Microsoft::Terminal::Core::ICoreAppearance& appearance);
    void SetFontInfo(const FontInfo& fontInfo);
    void SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle);
    void EraseScrollback();
    bool IsXtermBracketedPasteModeEnabled() const;
    std::wstring_view GetWorkingDirectory();

    // Write comes from the PTY and goes to our parser to be stored in the output buffer
    void Write(std::wstring_view stringView);

    // WritePastedText comes from our input and goes back to the PTY's input channel
    void WritePastedText(std::wstring_view stringView);

    [[nodiscard]] std::unique_lock<til::ticket_lock> LockForReading();
    [[nodiscard]] std::unique_lock<til::ticket_lock> LockForWriting();

    short GetBufferHeight() const noexcept;

    int ViewStartIndex() const noexcept;
    int ViewEndIndex() const noexcept;

    RenderSettings& GetRenderSettings() noexcept { return _renderSettings; };
    const RenderSettings& GetRenderSettings() const noexcept { return _renderSettings; };

#pragma region ITerminalApi
    // These methods are defined in TerminalApi.cpp
    void PrintString(const std::wstring_view string) override;
    void ReturnResponse(const std::wstring_view response) override;
    Microsoft::Console::VirtualTerminal::StateMachine& GetStateMachine() override;
    TextBuffer& GetTextBuffer() override;
    til::rect GetViewport() const override;
    void SetViewportPosition(const til::point position) override;
    void SetTextAttributes(const TextAttribute& attrs) override;
    void SetAutoWrapMode(const bool wrapAtEOL) override;
    void SetScrollingRegion(const til::inclusive_rect& scrollMargins) override;
    void WarningBell() override;
    bool GetLineFeedMode() const override;
    void LineFeed(const bool withReturn) override;
    void SetWindowTitle(const std::wstring_view title) override;
    CursorType GetUserDefaultCursorStyle() const override;
    bool ResizeWindow(const size_t width, const size_t height) override;
    void SetConsoleOutputCP(const unsigned int codepage) override;
    unsigned int GetConsoleOutputCP() const override;
    void EnableXtermBracketedPasteMode(const bool enabled) override;
    void CopyToClipboard(std::wstring_view content) override;
    void SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress) override;
    void SetWorkingDirectory(std::wstring_view uri) override;
    void ShowWindow(bool showOrHide) override;
    void UseAlternateScreenBuffer() override;
    void UseMainScreenBuffer() override;
    bool IsConsolePty() const override;
    bool IsVtInputEnabled() const override;
    void NotifyAccessibilityChange(const til::rect& changedRect) override;
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
    bool ShouldSendAlternateScroll(const unsigned int uiButton, const int32_t delta) const noexcept;

    void FocusChanged(const bool focused) noexcept override;

    std::wstring GetHyperlinkAtPosition(const COORD position);
    uint16_t GetHyperlinkIdAtPosition(const COORD position);
    std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> GetHyperlinkIntervalFromPosition(const COORD position);
#pragma endregion

#pragma region IBaseData(base to IRenderData and IUiaData)
    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    COORD GetTextBufferEndPosition() const noexcept override;
    const TextBuffer& GetTextBuffer() const noexcept override;
    const FontInfo& GetFontInfo() const noexcept override;

    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;
#pragma endregion

#pragma region IRenderData
    // These methods are defined in TerminalRenderData.cpp
    COORD GetCursorPosition() const noexcept override;
    bool IsCursorVisible() const noexcept override;
    bool IsCursorOn() const noexcept override;
    ULONG GetCursorHeight() const noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    CursorType GetCursorStyle() const noexcept override;
    bool IsCursorDoubleWidth() const override;
    const std::vector<Microsoft::Console::Render::RenderOverlay> GetOverlays() const noexcept override;
    const bool IsGridLineDrawingAllowed() noexcept override;
    const std::wstring GetHyperlinkUri(uint16_t id) const noexcept override;
    const std::wstring GetHyperlinkCustomId(uint16_t id) const noexcept override;
    const std::vector<size_t> GetPatternId(const COORD location) const noexcept override;
#pragma endregion

#pragma region IUiaData
    std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept override;
    std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept override;
    const bool IsSelectionActive() const noexcept override;
    const bool IsBlockSelection() const noexcept override;
    void ClearSelection() override;
    void SelectNewRegion(const COORD coordStart, const COORD coordEnd) override;
    const COORD GetSelectionAnchor() const noexcept override;
    const COORD GetSelectionEnd() const noexcept override;
    const std::wstring_view GetConsoleTitle() const noexcept override;
    void ColorSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd, const TextAttribute) override;
    const bool IsUiaDataInitialized() const noexcept override;
#pragma endregion

    void SetWriteInputCallback(std::function<void(std::wstring_view)> pfn) noexcept;
    void SetWarningBellCallback(std::function<void()> pfn) noexcept;
    void SetTitleChangedCallback(std::function<void(std::wstring_view)> pfn) noexcept;
    void SetTabColorChangedCallback(std::function<void(const std::optional<til::color>)> pfn) noexcept;
    void SetCopyToClipboardCallback(std::function<void(std::wstring_view)> pfn) noexcept;
    void SetScrollPositionChangedCallback(std::function<void(const int, const int, const int)> pfn) noexcept;
    void SetCursorPositionChangedCallback(std::function<void()> pfn) noexcept;
    void TaskbarProgressChangedCallback(std::function<void()> pfn) noexcept;
    void SetShowWindowCallback(std::function<void(bool)> pfn) noexcept;

    void SetCursorOn(const bool isOn);
    bool IsCursorBlinkingAllowed() const noexcept;

    void UpdatePatternsUnderLock() noexcept;
    void ClearPatternTree() noexcept;

    const std::optional<til::color> GetTabColor() const noexcept;

    winrt::Microsoft::Terminal::Core::Scheme GetColorScheme() const noexcept;
    void ApplyScheme(const winrt::Microsoft::Terminal::Core::Scheme& scheme);

    const size_t GetTaskbarState() const noexcept;
    const size_t GetTaskbarProgress() const noexcept;

#pragma region TextSelection
    // These methods are defined in TerminalSelection.cpp
    enum class SelectionDirection
    {
        Left,
        Right,
        Up,
        Down
    };

    enum class SelectionExpansion
    {
        Char,
        Word,
        Line, // Mouse selection only!
        Viewport,
        Buffer
    };
    void MultiClickSelection(const COORD viewportPos, SelectionExpansion expansionMode);
    void SetSelectionAnchor(const COORD position);
    void SetSelectionEnd(const COORD position, std::optional<SelectionExpansion> newExpansionMode = std::nullopt);
    void SetBlockSelection(const bool isEnabled) noexcept;
    void UpdateSelection(SelectionDirection direction, SelectionExpansion mode);

    using UpdateSelectionParams = std::optional<std::pair<SelectionDirection, SelectionExpansion>>;
    static UpdateSelectionParams ConvertKeyEventToUpdateSelectionParams(const ControlKeyStates mods, const WORD vkey);

    const TextBuffer::TextAndColor RetrieveSelectedTextFromBuffer(bool trimTrailingWhitespace);
#pragma endregion

private:
    std::function<void(std::wstring_view)> _pfnWriteInput;
    std::function<void()> _pfnWarningBell;
    std::function<void(std::wstring_view)> _pfnTitleChanged;
    std::function<void(std::wstring_view)> _pfnCopyToClipboard;

    // I've specifically put this instance here as it requires
    //   alignas(std::hardware_destructive_interference_size)
    // for best performance.
    //
    // But we can abuse the fact that the surrounding members rarely change and are huge
    // (std::function is like 64 bytes) to create some natural padding without wasting space.
    til::ticket_lock _readWriteLock;
#ifndef NDEBUG
    DWORD _lastLocker;
#endif

    std::function<void(const int, const int, const int)> _pfnScrollPositionChanged;
    std::function<void()> _pfnCursorPositionChanged;
    std::function<void(const std::optional<til::color>)> _pfnTabColorChanged;
    std::function<void()> _pfnTaskbarProgressChanged;
    std::function<void(bool)> _pfnShowWindowChanged;

    RenderSettings _renderSettings;
    std::unique_ptr<::Microsoft::Console::VirtualTerminal::StateMachine> _stateMachine;
    std::unique_ptr<::Microsoft::Console::VirtualTerminal::TerminalInput> _terminalInput;

    std::optional<std::wstring> _title;
    std::wstring _startingTitle;
    std::optional<til::color> _tabColor;
    std::optional<til::color> _startingTabColor;

    CursorType _defaultCursorShape;

    bool _snapOnInput;
    bool _altGrAliasing;
    bool _suppressApplicationTitle;
    bool _bracketedPasteMode;
    bool _trimBlockSelection;

    size_t _taskbarState;
    size_t _taskbarProgress;

    size_t _hyperlinkPatternId;

    std::wstring _workingDirectory;

    // This default fake font value is only used to check if the font is a raster font.
    // Otherwise, the font is changed to a real value with the renderer via TriggerFontChange.
    FontInfo _fontInfo{ DEFAULT_FONT_FACE, TMPF_TRUETYPE, 10, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false };
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
    SelectionExpansion _multiClickSelectionMode;
#pragma endregion

    std::unique_ptr<TextBuffer> _mainBuffer;
    std::unique_ptr<TextBuffer> _altBuffer;
    Microsoft::Console::Types::Viewport _mutableViewport;
    SHORT _scrollbackLines;
    bool _detectURLs{ false };

    til::size _altBufferSize;
    std::optional<til::size> _deferredResize{ std::nullopt };

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

    void _WriteBuffer(const std::wstring_view& stringView);

    void _AdjustCursorPosition(const COORD proposedPosition);

    void _NotifyScrollEvent() noexcept;

    void _NotifyTerminalCursorPositionChanged() noexcept;

    bool _inAltBuffer() const noexcept;
    TextBuffer& _activeBuffer() const noexcept;
    void _updateUrlDetection();

#pragma region TextSelection
    // These methods are defined in TerminalSelection.cpp
    std::vector<SMALL_RECT> _GetSelectionRects() const noexcept;
    std::pair<COORD, COORD> _PivotSelection(const COORD targetPos, bool& targetStart) const;
    std::pair<COORD, COORD> _ExpandSelectionAnchors(std::pair<COORD, COORD> anchors) const;
    COORD _ConvertToBufferCell(const COORD viewportPos) const;
    void _MoveByChar(SelectionDirection direction, COORD& pos);
    void _MoveByWord(SelectionDirection direction, COORD& pos);
    void _MoveByViewport(SelectionDirection direction, COORD& pos);
    void _MoveByBuffer(SelectionDirection direction, COORD& pos);
#pragma endregion

#ifdef UNIT_TESTING
    friend class TerminalCoreUnitTests::TerminalBufferTests;
    friend class TerminalCoreUnitTests::TerminalApiTest;
    friend class TerminalCoreUnitTests::ConptyRoundtripTests;
    friend class TerminalCoreUnitTests::ScrollTest;
#endif
};
