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
#include "../../cascadia/terminalcore/ITerminalInput.hpp"

#include <til/generational.h>
#include <til/ticket_lock.h>
#include <til/winrt.h>

inline constexpr size_t TaskbarMinProgress{ 10 };

// You have to forward decl the ICoreSettings here, instead of including the header.
// If you include the header, there will be compilation errors with other
//      headers that include Terminal.hpp
namespace winrt::Microsoft::Terminal::Core
{
    struct ICoreSettings;
    struct ICoreAppearance;
    struct Scheme;
    enum class MatchMode;
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
    class ScrollTest;
};
#endif

class Microsoft::Terminal::Core::Terminal final :
    public Microsoft::Console::VirtualTerminal::ITerminalApi,
    public Microsoft::Terminal::Core::ITerminalInput,
    public Microsoft::Console::Render::IRenderData
{
    using RenderSettings = Microsoft::Console::Render::RenderSettings;

public:
    struct TestDummyMarker
    {
    };

    static constexpr bool IsInputKey(WORD vkey)
    {
        return vkey != VK_CONTROL &&
               vkey != VK_LCONTROL &&
               vkey != VK_RCONTROL &&
               vkey != VK_MENU &&
               vkey != VK_LMENU &&
               vkey != VK_RMENU &&
               vkey != VK_SHIFT &&
               vkey != VK_LSHIFT &&
               vkey != VK_RSHIFT &&
               vkey != VK_LWIN &&
               vkey != VK_RWIN &&
               vkey != VK_SNAPSHOT;
    }

    Terminal();
    Terminal(TestDummyMarker);

    void Create(til::size viewportSize,
                til::CoordType scrollbackLines,
                Microsoft::Console::Render::Renderer& renderer);

    void CreateFromSettings(winrt::Microsoft::Terminal::Core::ICoreSettings settings,
                            Microsoft::Console::Render::Renderer& renderer);

    void UpdateSettings(winrt::Microsoft::Terminal::Core::ICoreSettings settings);
    void UpdateAppearance(const winrt::Microsoft::Terminal::Core::ICoreAppearance& appearance);
    void SetFontInfo(const FontInfo& fontInfo);
    void SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle);
    void SetVtChecksumReportSupport(const bool enabled);
    bool IsXtermBracketedPasteModeEnabled() const noexcept;
    std::wstring_view GetWorkingDirectory() noexcept;

    til::point GetViewportRelativeCursorPosition() const noexcept;

    // Write comes from the PTY and goes to our parser to be stored in the output buffer
    void Write(std::wstring_view stringView);

    void _assertLocked() const noexcept;
    void _assertUnlocked() const noexcept;
    [[nodiscard]] std::unique_lock<til::recursive_ticket_lock> LockForReading() const noexcept;
    [[nodiscard]] std::unique_lock<til::recursive_ticket_lock> LockForWriting() noexcept;
    til::recursive_ticket_lock_suspension SuspendLock() noexcept;

    til::CoordType GetBufferHeight() const noexcept;

    int ViewStartIndex() const noexcept;
    int ViewEndIndex() const noexcept;

    RenderSettings& GetRenderSettings() noexcept;
    const RenderSettings& GetRenderSettings() const noexcept;

    std::vector<ScrollMark> GetMarkRows() const;
    std::vector<MarkExtents> GetMarkExtents() const;
    void AddMarkFromUI(ScrollbarData mark, til::CoordType y);

    til::property<bool> AlwaysNotifyOnBufferRotation;

    std::wstring CurrentCommand() const;

    void SerializeMainBuffer(const wchar_t* destination) const;

#pragma region ITerminalApi
    // These methods are defined in TerminalApi.cpp
    void ReturnResponse(const std::wstring_view response) override;
    Microsoft::Console::VirtualTerminal::StateMachine& GetStateMachine() noexcept override;
    BufferState GetBufferAndViewport() noexcept override;
    void SetViewportPosition(const til::point position) noexcept override;
    void SetSystemMode(const Mode mode, const bool enabled) noexcept override;
    bool GetSystemMode(const Mode mode) const noexcept override;
    void ReturnAnswerback() override;
    void WarningBell() override;
    void SetWindowTitle(const std::wstring_view title) override;
    CursorType GetUserDefaultCursorStyle() const noexcept override;
    bool ResizeWindow(const til::CoordType width, const til::CoordType height) override;
    void SetCodePage(const unsigned int codepage) noexcept override;
    void ResetCodePage() noexcept override;
    unsigned int GetOutputCodePage() const noexcept override;
    unsigned int GetInputCodePage() const noexcept override;
    void CopyToClipboard(wil::zwstring_view content) override;
    void SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress) override;
    void SetWorkingDirectory(std::wstring_view uri) override;
    void PlayMidiNote(const int noteNumber, const int velocity, const std::chrono::microseconds duration) override;
    void ShowWindow(bool showOrHide) override;
    void UseAlternateScreenBuffer(const TextAttribute& attrs) override;
    void UseMainScreenBuffer() override;

    bool IsVtInputEnabled() const noexcept override;
    void NotifyAccessibilityChange(const til::rect& changedRect) noexcept override;
    void NotifyBufferRotation(const int delta) override;

    void InvokeCompletions(std::wstring_view menuJson, unsigned int replaceLength) override;

    void SearchMissingCommand(const std::wstring_view command) override;

#pragma endregion

    void ClearMark();
    void ClearAllMarks();
    til::color GetColorForMark(const ScrollbarData& markData) const;

#pragma region ITerminalInput
    // These methods are defined in Terminal.cpp
    [[nodiscard]] ::Microsoft::Console::VirtualTerminal::TerminalInput::OutputType SendKeyEvent(const WORD vkey, const WORD scanCode, const Microsoft::Terminal::Core::ControlKeyStates states, const bool keyDown) override;
    [[nodiscard]] ::Microsoft::Console::VirtualTerminal::TerminalInput::OutputType SendMouseEvent(const til::point viewportPos, const unsigned int uiButton, const ControlKeyStates states, const short wheelDelta, const Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState state) override;
    [[nodiscard]] ::Microsoft::Console::VirtualTerminal::TerminalInput::OutputType SendCharEvent(const wchar_t ch, const WORD scanCode, const ControlKeyStates states) override;
    [[nodiscard]] ::Microsoft::Console::VirtualTerminal::TerminalInput::OutputType FocusChanged(const bool focused) override;

    [[nodiscard]] HRESULT UserResize(const til::size viewportSize) noexcept override;
    void UserScrollViewport(const int viewTop) override;
    int GetScrollOffset() noexcept override;

    void TrySnapOnInput() override;
    bool IsTrackingMouseInput() const noexcept;
    bool ShouldSendAlternateScroll(const unsigned int uiButton, const int32_t delta) const noexcept;

    std::wstring GetHyperlinkAtViewportPosition(const til::point viewportPos);
    std::wstring GetHyperlinkAtBufferPosition(const til::point bufferPos);
    uint16_t GetHyperlinkIdAtViewportPosition(const til::point viewportPos);
    std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> GetHyperlinkIntervalFromViewportPosition(const til::point viewportPos);
#pragma endregion

#pragma region IRenderData
    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    til::point GetTextBufferEndPosition() const noexcept override;
    TextBuffer& GetTextBuffer() const noexcept override;
    const FontInfo& GetFontInfo() const noexcept override;

    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;

    // These methods are defined in TerminalRenderData.cpp
    til::point GetCursorPosition() const noexcept override;
    bool IsCursorVisible() const noexcept override;
    bool IsCursorOn() const noexcept override;
    ULONG GetCursorHeight() const noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    CursorType GetCursorStyle() const noexcept override;
    bool IsCursorDoubleWidth() const override;
    const bool IsGridLineDrawingAllowed() noexcept override;
    const std::wstring GetHyperlinkUri(uint16_t id) const override;
    const std::wstring GetHyperlinkCustomId(uint16_t id) const override;
    const std::vector<size_t> GetPatternId(const til::point location) const override;

    std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept override;
    std::span<const til::point_span> GetSelectionSpans() const noexcept override;
    std::span<const til::point_span> GetSearchHighlights() const noexcept override;
    const til::point_span* GetSearchHighlightFocused() const noexcept override;
    const bool IsSelectionActive() const noexcept override;
    const bool IsBlockSelection() const noexcept override;
    void ClearSelection() override;
    void SelectNewRegion(const til::point coordStart, const til::point coordEnd) override;
    const til::point GetSelectionAnchor() const noexcept override;
    const til::point GetSelectionEnd() const noexcept override;
    const std::wstring_view GetConsoleTitle() const noexcept override;
    const bool IsUiaDataInitialized() const noexcept override;
#pragma endregion

    void SetWriteInputCallback(std::function<void(std::wstring_view)> pfn) noexcept;
    void SetWarningBellCallback(std::function<void()> pfn) noexcept;
    void SetTitleChangedCallback(std::function<void(std::wstring_view)> pfn) noexcept;
    void SetCopyToClipboardCallback(std::function<void(wil::zwstring_view)> pfn) noexcept;
    void SetScrollPositionChangedCallback(std::function<void(const int, const int, const int)> pfn) noexcept;
    void TaskbarProgressChangedCallback(std::function<void()> pfn) noexcept;
    void SetShowWindowCallback(std::function<void(bool)> pfn) noexcept;
    void SetPlayMidiNoteCallback(std::function<void(const int, const int, const std::chrono::microseconds)> pfn) noexcept;
    void CompletionsChangedCallback(std::function<void(std::wstring_view, unsigned int)> pfn) noexcept;
    void SetSearchMissingCommandCallback(std::function<void(std::wstring_view, const til::CoordType)> pfn) noexcept;
    void SetClearQuickFixCallback(std::function<void()> pfn) noexcept;
    void SetWindowSizeChangedCallback(std::function<void(int32_t, int32_t)> pfn) noexcept;
    void SetSearchHighlights(const std::vector<til::point_span>& highlights) noexcept;
    void SetSearchHighlightFocused(size_t focusedIdx) noexcept;
    void ScrollToSearchHighlight(til::CoordType searchScrollOffset);

    void BlinkCursor() noexcept;
    void SetCursorOn(const bool isOn) noexcept;

    void UpdatePatternsUnderLock();

    const std::optional<til::color> GetTabColor() const;

    winrt::Microsoft::Terminal::Core::Scheme GetColorScheme() const;
    void ApplyScheme(const winrt::Microsoft::Terminal::Core::Scheme& scheme);

    const size_t GetTaskbarState() const noexcept;
    const size_t GetTaskbarProgress() const noexcept;

    void ColorSelection(const TextAttribute& attr, winrt::Microsoft::Terminal::Core::MatchMode matchMode);
    void PreviewText(std::wstring_view input);

#pragma region TextSelection
    // These methods are defined in TerminalSelection.cpp
    enum class SelectionInteractionMode
    {
        None,
        Mouse,
        Keyboard,
        Mark
    };

    enum class SelectionDirection
    {
        Left,
        Right,
        Up,
        Down
    };

    enum class SearchDirection
    {
        Forward,
        Backward
    };

    enum class SelectionExpansion
    {
        Char,
        Word,
        Line, // Mouse selection only!
        Viewport,
        Buffer
    };

    enum class SelectionEndpoint
    {
        None = 0,
        Start = 0x1,
        End = 0x2
    };

    struct TextCopyData
    {
        std::wstring plainText;
        std::string html;
        std::string rtf;
    };

    void MultiClickSelection(const til::point viewportPos, SelectionExpansion expansionMode);
    void SetSelectionAnchor(const til::point position);
    void SetSelectionEnd(const til::point position, std::optional<SelectionExpansion> newExpansionMode = std::nullopt);
    void SetBlockSelection(const bool isEnabled) noexcept;
    void UpdateSelection(SelectionDirection direction, SelectionExpansion mode, ControlKeyStates mods);
    void SelectAll();
    SelectionInteractionMode SelectionMode() const noexcept;
    void SwitchSelectionEndpoint() noexcept;
    void ExpandSelectionToWord();
    void ToggleMarkMode();
    void SelectHyperlink(const SearchDirection dir);

    using UpdateSelectionParams = std::optional<std::pair<SelectionDirection, SelectionExpansion>>;
    UpdateSelectionParams ConvertKeyEventToUpdateSelectionParams(const ControlKeyStates mods, const WORD vkey) const noexcept;
    til::point SelectionStartForRendering() const;
    til::point SelectionEndForRendering() const;
    const SelectionEndpoint SelectionEndpointTarget() const noexcept;

    TextCopyData RetrieveSelectedTextFromBuffer(const bool singleLine, const bool withControlSequences = false, const bool html = false, const bool rtf = false) const;
#pragma endregion

#ifndef NDEBUG
    bool _suppressLockChecks = false;
#endif

private:
    std::function<void(std::wstring_view)> _pfnWriteInput;
    std::function<void()> _pfnWarningBell;
    std::function<void(std::wstring_view)> _pfnTitleChanged;
    std::function<void(wil::zwstring_view)> _pfnCopyToClipboard;

    // I've specifically put this instance here as it requires
    //   alignas(std::hardware_destructive_interference_size)
    // for best performance.
    //
    // But we can abuse the fact that the surrounding members rarely change and are huge
    // (std::function is like 64 bytes) to create some natural padding without wasting space.
    til::recursive_ticket_lock _readWriteLock;

    std::function<void(const int, const int, const int)> _pfnScrollPositionChanged;
    std::function<void()> _pfnTaskbarProgressChanged;
    std::function<void(bool)> _pfnShowWindowChanged;
    std::function<void(const int, const int, const std::chrono::microseconds)> _pfnPlayMidiNote;
    std::function<void(std::wstring_view, unsigned int)> _pfnCompletionsChanged;
    std::function<void(std::wstring_view, const til::CoordType)> _pfnSearchMissingCommand;
    std::function<void()> _pfnClearQuickFix;
    std::function<void(int32_t, int32_t)> _pfnWindowSizeChanged;

    RenderSettings _renderSettings;
    std::unique_ptr<::Microsoft::Console::VirtualTerminal::StateMachine> _stateMachine;
    ::Microsoft::Console::VirtualTerminal::TerminalInput _terminalInput;

    std::optional<std::wstring> _title;
    std::wstring _startingTitle;
    std::optional<til::color> _startingTabColor;

    std::vector<til::point_span> _searchHighlights;
    size_t _searchHighlightFocused = 0;

    mutable std::vector<til::point_span> _lastSelectionSpans;
    mutable til::generation_t _lastSelectionGeneration{};

    CursorType _defaultCursorShape = CursorType::Legacy;

    til::enumset<Mode> _systemMode{ Mode::AutoWrap };

    bool _snapOnInput = true;
    bool _altGrAliasing = true;
    bool _suppressApplicationTitle = false;
    bool _trimBlockSelection = false;
    bool _autoMarkPrompts = false;
    bool _rainbowSuggestions = false;

    size_t _taskbarState = 0;
    size_t _taskbarProgress = 0;

    size_t _hyperlinkPatternId = 0;

    std::wstring _answerbackMessage;
    std::wstring _workingDirectory;

    // This default fake font value is only used to check if the font is a raster font.
    // Otherwise, the font is changed to a real value with the renderer via TriggerFontChange.
    FontInfo _fontInfo{ DEFAULT_FONT_FACE, TMPF_TRUETYPE, 10, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false };
#pragma region Text Selection
    // a selection is represented as a range between two COORDs (start and end)
    // the pivot is the til::point that remains selected when you extend a selection in any direction
    //   this is particularly useful when a word selection is extended over its starting point
    //   see TerminalSelection.cpp for more information
    struct SelectionInfo
    {
        til::point start;
        til::point end;
        til::point pivot;
        bool blockSelection = false;
        bool active = false;
    };
    til::generational<SelectionInfo> _selection{};
    std::wstring _wordDelimiters;
    SelectionExpansion _multiClickSelectionMode = SelectionExpansion::Char;
    SelectionInteractionMode _selectionMode = SelectionInteractionMode::None;
    bool _selectionIsTargetingUrl = false;
    SelectionEndpoint _selectionEndpoint = SelectionEndpoint::None;
    bool _anchorInactiveSelectionEndpoint = false;
#pragma endregion

    std::unique_ptr<TextBuffer> _mainBuffer;
    std::unique_ptr<TextBuffer> _altBuffer;
    Microsoft::Console::Types::Viewport _mutableViewport;
    til::CoordType _scrollbackLines = 0;
    bool _detectURLs = false;
    bool _clipboardOperationsAllowed = true;

    til::size _altBufferSize;
    std::optional<til::size> _deferredResize;

    // _scrollOffset is the number of lines above the viewport that are currently visible
    // If _scrollOffset is 0, then the visible region of the buffer is the viewport.
    til::CoordType _scrollOffset = 0;
    // TODO this might not be the value we want to store.
    // We might want to store the height in the scrollback that's currently visible.
    // Think on this some more.
    // For example: While looking at the scrollback, we probably want the visible region to "stick"
    //   to the region they scrolled to. If that were the case, then every time we move _mutableViewport,
    //   we'd also need to update _offset.
    // However, if we just stored it as a _visibleTop, then that point would remain fixed -
    //      Though if _visibleTop == _mutableViewport.top, then we'd need to make sure to update
    //      _visibleTop as well.
    // Additionally, maybe some people want to scroll into the history, then have that scroll out from
    //      underneath them, while others would prefer to anchor it in place.
    //      Either way, we should make this behavior controlled by a setting.

    interval_tree::IntervalTree<til::point, size_t> _patternIntervalTree;
    void _clearPatternTree();
    void _InvalidatePatternTree();
    void _InvalidateFromCoords(const til::point start, const til::point end);

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

    void _StoreKeyEvent(const WORD vkey, const WORD scanCode) noexcept;
    WORD _TakeVirtualKeyFromLastKeyEvent(const WORD scanCode) noexcept;

    Console::VirtualTerminal::TerminalInput& _getTerminalInput() noexcept;
    const Console::VirtualTerminal::TerminalInput& _getTerminalInput() const noexcept;

    int _VisibleStartIndex() const noexcept;
    int _VisibleEndIndex() const noexcept;

    Microsoft::Console::Types::Viewport _GetMutableViewport() const noexcept;
    Microsoft::Console::Types::Viewport _GetVisibleViewport() const noexcept;

    void _PreserveUserScrollOffset(const int viewportDelta) noexcept;
    til::CoordType _ScrollToPoints(const til::point coordStart, const til::point coordEnd);

    void _NotifyScrollEvent();
    bool _inAltBuffer() const noexcept;
    TextBuffer& _activeBuffer() const noexcept;
    void _updateUrlDetection();
    interval_tree::IntervalTree<til::point, size_t> _getPatterns(til::CoordType beg, til::CoordType end) const;

#pragma region TextSelection
    // These methods are defined in TerminalSelection.cpp
    std::vector<til::point_span> _GetSelectionSpans() const noexcept;
    std::pair<til::point, til::point> _PivotSelection(const til::point targetPos, bool& targetStart) const noexcept;
    std::pair<til::point, til::point> _ExpandSelectionAnchors(std::pair<til::point, til::point> anchors) const;
    til::point _ConvertToBufferCell(const til::point viewportPos, bool allowRightExclusive) const;
    void _ScrollToPoint(const til::point pos);
    void _MoveByChar(SelectionDirection direction, til::point& pos);
    void _MoveByWord(SelectionDirection direction, til::point& pos);
    void _MoveByViewport(SelectionDirection direction, til::point& pos) noexcept;
    void _MoveByBuffer(SelectionDirection direction, til::point& pos) noexcept;
    void _SetSelectionEnd(SelectionInfo* selection, const til::point position, std::optional<SelectionExpansion> newExpansionMode = std::nullopt);
#pragma endregion

#ifdef UNIT_TESTING
    friend class TerminalCoreUnitTests::TerminalBufferTests;
    friend class TerminalCoreUnitTests::TerminalApiTest;
    friend class TerminalCoreUnitTests::ScrollTest;
#endif
};
