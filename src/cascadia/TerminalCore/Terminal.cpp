// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "../../terminal/adapter/adaptDispatch.hpp"
#include "../../terminal/parser/OutputStateMachineEngine.hpp"
#include "../../inc/unicode.hpp"
#include "../../types/inc/utils.hpp"
#include "../../types/inc/colorTable.hpp"
#include "../../buffer/out/search.h"

#include <winrt/Microsoft.Terminal.Core.h>

using namespace winrt::Microsoft::Terminal::Core;
using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

using PointTree = interval_tree::IntervalTree<til::point, size_t>;

#pragma warning(suppress : 26455) // default constructor is throwing, too much effort to rearrange at this time.
Terminal::Terminal()
{
    _renderSettings.SetColorAlias(ColorAlias::DefaultForeground, TextColor::DEFAULT_FOREGROUND, RGB(255, 255, 255));
    _renderSettings.SetColorAlias(ColorAlias::DefaultBackground, TextColor::DEFAULT_BACKGROUND, RGB(0, 0, 0));
}

void Terminal::Create(til::size viewportSize, til::CoordType scrollbackLines, Renderer& renderer)
{
    _mutableViewport = Viewport::FromDimensions({ 0, 0 }, viewportSize);
    _scrollbackLines = scrollbackLines;
    const til::size bufferSize{ viewportSize.width,
                                Utils::ClampToShortMax(viewportSize.height + scrollbackLines, 1) };
    const TextAttribute attr{};
    const UINT cursorSize = 12;
    _mainBuffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, true, renderer);

    auto dispatch = std::make_unique<AdaptDispatch>(*this, renderer, _renderSettings, _terminalInput);
    auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
    _stateMachine = std::make_unique<StateMachine>(std::move(engine));

    // Until we have a true pass-through mode (GH#1173), the decision as to
    // whether C1 controls are interpreted or not is made at the conhost level.
    // If they are being filtered out, then we will simply never receive them.
    // But if they are being accepted by conhost, there's a chance they may get
    // passed through in some situations, so it's important that our state
    // machine is always prepared to accept them.
    _stateMachine->SetParserMode(StateMachine::Mode::AlwaysAcceptC1, true);
}

// Method Description:
// - Initializes the Terminal from the given set of settings.
// Arguments:
// - settings: the set of CoreSettings we need to use to initialize the terminal
// - renderer: the Renderer that the terminal can use for paint invalidation.
void Terminal::CreateFromSettings(ICoreSettings settings,
                                  Renderer& renderer)
{
    const til::size viewportSize{ Utils::ClampToShortMax(settings.InitialCols(), 1),
                                  Utils::ClampToShortMax(settings.InitialRows(), 1) };

    // TODO:MSFT:20642297 - Support infinite scrollback here, if HistorySize is -1
    Create(viewportSize, Utils::ClampToShortMax(settings.HistorySize(), 0), renderer);

    UpdateSettings(settings);
}

// Method Description:
// - Update our internal properties to match the new values in the provided
//   CoreSettings object.
// Arguments:
// - settings: an ICoreSettings with new settings values for us to use.
void Terminal::UpdateSettings(ICoreSettings settings)
{
    UpdateAppearance(settings);

    _snapOnInput = settings.SnapOnInput();
    _altGrAliasing = settings.AltGrAliasing();
    _wordDelimiters = settings.WordDelimiters();
    _suppressApplicationTitle = settings.SuppressApplicationTitle();
    _startingTitle = settings.StartingTitle();
    _trimBlockSelection = settings.TrimBlockSelection();
    _autoMarkPrompts = settings.AutoMarkPrompts();

    _terminalInput.ForceDisableWin32InputMode(settings.ForceVTInput());

    if (settings.TabColor() == nullptr)
    {
        _renderSettings.SetColorTableEntry(TextColor::FRAME_BACKGROUND, INVALID_COLOR);
    }
    else
    {
        _renderSettings.SetColorTableEntry(TextColor::FRAME_BACKGROUND, til::color{ settings.TabColor().Value() });
    }

    if (!_startingTabColor && settings.StartingTabColor())
    {
        _startingTabColor = settings.StartingTabColor().Value();
    }

    // TODO:MSFT:21327402 - if HistorySize has changed, resize the buffer so we
    // have a smaller scrollback. We should do this carefully - if the new buffer
    // size is smaller than where the mutable viewport currently is, we'll want
    // to make sure to rotate the buffer contents upwards, so the mutable viewport
    // remains at the bottom of the buffer.

    // Regenerate the pattern tree for the new buffer size
    if (_mainBuffer)
    {
        // Clear the patterns first
        _mainBuffer->ClearPatternRecognizers();
        _detectURLs = settings.DetectURLs();
        _updateUrlDetection();
    }
}

// Method Description:
// - Update our internal properties to match the new values in the provided
//   CoreAppearance object.
// Arguments:
// - appearance: an ICoreAppearance with new settings values for us to use.
void Terminal::UpdateAppearance(const ICoreAppearance& appearance)
{
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBold, appearance.IntenseIsBold());
    _renderSettings.SetRenderMode(RenderSettings::Mode::IntenseIsBright, appearance.IntenseIsBright());

    switch (appearance.AdjustIndistinguishableColors())
    {
    case AdjustTextMode::Always:
        _renderSettings.SetRenderMode(RenderSettings::Mode::IndexedDistinguishableColors, false);
        _renderSettings.SetRenderMode(RenderSettings::Mode::AlwaysDistinguishableColors, true);
        break;
    case AdjustTextMode::Indexed:
        _renderSettings.SetRenderMode(RenderSettings::Mode::IndexedDistinguishableColors, true);
        _renderSettings.SetRenderMode(RenderSettings::Mode::AlwaysDistinguishableColors, false);
        break;
    case AdjustTextMode::Never:
        _renderSettings.SetRenderMode(RenderSettings::Mode::IndexedDistinguishableColors, false);
        _renderSettings.SetRenderMode(RenderSettings::Mode::AlwaysDistinguishableColors, false);
        break;
    }

    const til::color newBackgroundColor{ appearance.DefaultBackground() };
    _renderSettings.SetColorAlias(ColorAlias::DefaultBackground, TextColor::DEFAULT_BACKGROUND, newBackgroundColor);
    const til::color newForegroundColor{ appearance.DefaultForeground() };
    _renderSettings.SetColorAlias(ColorAlias::DefaultForeground, TextColor::DEFAULT_FOREGROUND, newForegroundColor);
    const til::color newCursorColor{ appearance.CursorColor() };
    _renderSettings.SetColorTableEntry(TextColor::CURSOR_COLOR, newCursorColor);

    for (auto i = 0; i < 16; i++)
    {
        _renderSettings.SetColorTableEntry(i, til::color{ appearance.GetColorTableEntry(i) });
    }

    auto cursorShape = CursorType::VerticalBar;
    switch (appearance.CursorShape())
    {
    case CursorStyle::Underscore:
        cursorShape = CursorType::Underscore;
        break;
    case CursorStyle::FilledBox:
        cursorShape = CursorType::FullBox;
        break;
    case CursorStyle::EmptyBox:
        cursorShape = CursorType::EmptyBox;
        break;
    case CursorStyle::Vintage:
        cursorShape = CursorType::Legacy;
        break;
    case CursorStyle::DoubleUnderscore:
        cursorShape = CursorType::DoubleUnderscore;
        break;
    default:
    case CursorStyle::Bar:
        cursorShape = CursorType::VerticalBar;
        break;
    }

    // We're checking if the main buffer exists here, but then setting the
    // appearance of the active one. If the main buffer exists, then at least
    // one buffer exists and _activeBuffer() will work
    if (_mainBuffer)
    {
        _activeBuffer().GetCursor().SetStyle(appearance.CursorHeight(), cursorShape);
    }

    _defaultCursorShape = cursorShape;

    // Tell the control that the scrollbar has somehow changed. Used as a
    // workaround to force the control to redraw any scrollbar marks whose color
    // may have changed.
    _NotifyScrollEvent();
}

void Terminal::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle)
{
    auto& engine = reinterpret_cast<OutputStateMachineEngine&>(_stateMachine->Engine());
    engine.Dispatch().SetCursorStyle(cursorStyle);
}

void Terminal::EraseScrollback()
{
    auto& engine = reinterpret_cast<OutputStateMachineEngine&>(_stateMachine->Engine());
    engine.Dispatch().EraseInDisplay(DispatchTypes::EraseType::Scrollback);
}

bool Terminal::IsXtermBracketedPasteModeEnabled() const noexcept
{
    return _systemMode.test(Mode::BracketedPaste);
}

std::wstring_view Terminal::GetWorkingDirectory() noexcept
{
    return _workingDirectory;
}

// Method Description:
// - Resize the terminal as the result of some user interaction.
// Arguments:
// - viewportSize: the new size of the viewport, in chars
// Return Value:
// - S_OK if we successfully resized the terminal, S_FALSE if there was
//      nothing to do (the viewportSize is the same as our current size), or an
//      appropriate HRESULT for failing to resize.
[[nodiscard]] HRESULT Terminal::UserResize(const til::size viewportSize) noexcept
{
    const auto oldDimensions = _GetMutableViewport().Dimensions();
    if (viewportSize == oldDimensions)
    {
        return S_FALSE;
    }

    // Shortcut: if we're in the alt buffer, just resize the
    // alt buffer and put off resizing the main buffer till we switch back. Fortunately, this is easy. We don't need to
    // worry about the viewport and scrollback at all! The alt buffer never has
    // any scrollback, so we just need to resize it and presto, we're done.
    if (_inAltBuffer())
    {
        // stash this resize for the future.
        _deferredResize = viewportSize;

        _altBuffer->GetCursor().StartDeferDrawing();
        // we're capturing `this` here because when we exit, we want to EndDefer on the (newly created) active buffer.
        auto endDefer = wil::scope_exit([this]() noexcept { _altBuffer->GetCursor().EndDeferDrawing(); });

        // GH#3494: We don't need to reflow the alt buffer. Apps that use the
        // alt buffer will redraw themselves. This prevents graphical artifacts.
        //
        // This is consistent with VTE
        RETURN_IF_FAILED(_altBuffer->ResizeTraditional(viewportSize));

        // Since the _mutableViewport is no longer the size of the actual
        // viewport, then update our _altBufferSize tracker we're using to help
        // us out here.
        _altBufferSize = viewportSize;
        return S_OK;
    }

    const auto dx = viewportSize.width - oldDimensions.width;
    const auto newBufferHeight = std::clamp(viewportSize.height + _scrollbackLines, 0, SHRT_MAX);

    til::size bufferSize{ viewportSize.width, newBufferHeight };

    // This will be used to determine where the viewport should be in the new buffer.
    const auto oldViewportTop = _mutableViewport.Top();
    auto newViewportTop = oldViewportTop;
    auto newVisibleTop = _VisibleStartIndex();

    // If the original buffer had _no_ scroll offset, then we should be at the
    // bottom in the new buffer as well. Track that case now.
    const auto originalOffsetWasZero = _scrollOffset == 0;

    // skip any drawing updates that might occur until we swap _buffer with the new buffer or if we exit early.
    _mainBuffer->GetCursor().StartDeferDrawing();
    // we're capturing `this` here because when we exit, we want to EndDefer on the (newly created) active buffer.
    auto endDefer = wil::scope_exit([this]() noexcept { _mainBuffer->GetCursor().EndDeferDrawing(); });

    // First allocate a new text buffer to take the place of the current one.
    std::unique_ptr<TextBuffer> newTextBuffer;
    try
    {
        // GH#3848 - Stash away the current attributes the old text buffer is
        // using. We'll initialize the new buffer with the default attributes,
        // but after the resize, we'll want to make sure that the new buffer's
        // current attributes (the ones used for printing new text) match the
        // old buffer's.
        const auto oldBufferAttributes = _mainBuffer->GetCurrentAttributes();
        newTextBuffer = std::make_unique<TextBuffer>(bufferSize,
                                                     TextAttribute{},
                                                     0, // temporarily set size to 0 so it won't render.
                                                     _mainBuffer->IsActiveBuffer(),
                                                     _mainBuffer->GetRenderer());

        // start defer drawing on the new buffer
        newTextBuffer->GetCursor().StartDeferDrawing();

        // Build a PositionInformation to track the position of both the top of
        // the mutable viewport and the top of the visible viewport in the new
        // buffer.
        // * the new value of mutableViewportTop will be used to figure out
        //   where we should place the mutable viewport in the new buffer. This
        //   requires a bit of trickiness to remain consistent with conpty's
        //   buffer (as seen below).
        // * the new value of visibleViewportTop will be used to calculate the
        //   new scrollOffset in the new buffer, so that the visible lines on
        //   the screen remain roughly the same.
        TextBuffer::PositionInformation oldRows{ 0 };
        oldRows.mutableViewportTop = oldViewportTop;
        oldRows.visibleViewportTop = newVisibleTop;

        const std::optional oldViewStart{ oldViewportTop };
        RETURN_IF_FAILED(TextBuffer::Reflow(*_mainBuffer.get(),
                                            *newTextBuffer.get(),
                                            _mutableViewport,
                                            { oldRows }));

        newViewportTop = oldRows.mutableViewportTop;
        newVisibleTop = oldRows.visibleViewportTop;

        // Restore the active text attributes
        newTextBuffer->SetCurrentAttributes(oldBufferAttributes);
    }
    CATCH_RETURN();

    // Conpty resizes a little oddly - if the height decreased, and there were
    // blank lines at the bottom, those lines will get trimmed. If there's not
    // blank lines, then the top will get "shifted down", moving the top line
    // into scrollback. See GH#3490 for more details.
    //
    // If the final position in the buffer is on the bottom row of the new
    // viewport, then we're going to need to move the top down. Otherwise, move
    // the bottom up.
    //
    // There are also important things to consider with line wrapping.
    // * If a line in scrollback wrapped that didn't previously, we'll need to
    //   make sure to have the new viewport down another line. This will cause
    //   our top to move down.
    // * If a line _in the viewport_ wrapped that didn't previously, then the
    //   conpty buffer will also have that wrapped line, and will move the
    //   cursor & text down a line in response. This causes our bottom to move
    //   down.
    //
    // We're going to use a combo of both these things to calculate where the
    // new viewport should be. To keep in sync with conpty, we'll need to make
    // sure that any lines that entered the scrollback _stay in scrollback_. We
    // do that by taking the max of
    // * Where the old top line in the viewport exists in the new buffer (as
    //   calculated by TextBuffer::Reflow)
    // * Where the bottom of the text in the new buffer is (and using that to
    //   calculate another proposed top location).

    const auto newCursorPos = newTextBuffer->GetCursor().GetPosition();
#pragma warning(push)
#pragma warning(disable : 26496) // cpp core checks wants this const, but it's assigned immediately below...
    auto newLastChar = newCursorPos;
    try
    {
        newLastChar = newTextBuffer->GetLastNonSpaceCharacter();
    }
    CATCH_LOG();
#pragma warning(pop)

    const auto maxRow = std::max(newLastChar.y, newCursorPos.y);

    const auto proposedTopFromLastLine = maxRow - viewportSize.height + 1;
    const auto proposedTopFromScrollback = newViewportTop;

    auto proposedTop = std::max(proposedTopFromLastLine,
                                proposedTopFromScrollback);

    // If we're using the new location of the old top line to place the
    // viewport, we might need to make an adjustment to it.
    //
    // We're using the last cell of the line to calculate where the top line is
    // in the new buffer. If that line wrapped, then all the lines below it
    // shifted down in the buffer. If there's space for all those lines in the
    // conpty buffer, then the originally unwrapped top line will _still_ be in
    // the buffer. In that case, don't stick to the _end_ of the old top line,
    // instead stick to the _start_, which is one line up.
    //
    // We can know if there's space in the conpty buffer by checking if the
    // maxRow (the highest row we've written text to) is above the viewport from
    // this proposed top position.
    if (proposedTop == proposedTopFromScrollback)
    {
        const auto proposedViewFromTop = Viewport::FromDimensions({ 0, proposedTopFromScrollback }, viewportSize);
        if (maxRow < proposedViewFromTop.BottomInclusive())
        {
            if (dx < 0 && proposedTop > 0)
            {
                try
                {
                    const auto& row = newTextBuffer->GetRowByOffset(::base::ClampSub(proposedTop, 1));
                    if (row.WasWrapForced())
                    {
                        proposedTop--;
                    }
                }
                CATCH_LOG();
            }
        }
    }

    // If the new bottom would be higher than the last row of text, then we
    // definitely want to use the last row of text to determine where the
    // viewport should be.
    const auto proposedViewFromTop = Viewport::FromDimensions({ 0, proposedTopFromScrollback }, viewportSize);
    if (maxRow > proposedViewFromTop.BottomInclusive())
    {
        proposedTop = proposedTopFromLastLine;
    }

    // Make sure the proposed viewport is within the bounds of the buffer.
    // First make sure the top is >=0
    proposedTop = std::max(0, proposedTop);

    // If the new bottom would be below the bottom of the buffer, then slide the
    // top up so that we'll still fit within the buffer.
    const auto newView = Viewport::FromDimensions({ 0, proposedTop }, viewportSize);
    const auto proposedBottom = newView.BottomExclusive();
    if (proposedBottom > bufferSize.height)
    {
        proposedTop = ::base::ClampSub(proposedTop, ::base::ClampSub(proposedBottom, bufferSize.height));
    }

    _mutableViewport = Viewport::FromDimensions({ 0, proposedTop }, viewportSize);

    _mainBuffer.swap(newTextBuffer);

    // GH#3494: Maintain scrollbar position during resize
    // Make sure that we don't scroll past the mutableViewport at the bottom of the buffer
    newVisibleTop = std::min(newVisibleTop, _mutableViewport.Top());
    // Make sure we don't scroll past the top of the scrollback
    newVisibleTop = std::max(newVisibleTop, 0);

    // If the old scrolloffset was 0, then we weren't scrolled back at all
    // before, and shouldn't be now either.
    _scrollOffset = originalOffsetWasZero ? 0 : static_cast<int>(::base::ClampSub(_mutableViewport.Top(), newVisibleTop));

    // GH#5029 - make sure to InvalidateAll here, so that we'll paint the entire visible viewport.
    try
    {
        _activeBuffer().TriggerRedrawAll();
    }
    CATCH_LOG();
    _NotifyScrollEvent();

    return S_OK;
}

void Terminal::Write(std::wstring_view stringView)
{
    auto lock = LockForWriting();

    const auto& cursor = _activeBuffer().GetCursor();
    const til::point cursorPosBefore{ cursor.GetPosition() };

    _stateMachine->ProcessString(stringView);

    const til::point cursorPosAfter{ cursor.GetPosition() };

    // Firing the CursorPositionChanged event is very expensive so we try not to
    // do that when the cursor does not need to be redrawn.
    if (cursorPosBefore != cursorPosAfter)
    {
        _NotifyTerminalCursorPositionChanged();
    }
}

void Terminal::WritePastedText(std::wstring_view stringView)
{
    const auto option = ::Microsoft::Console::Utils::FilterOption::CarriageReturnNewline |
                        ::Microsoft::Console::Utils::FilterOption::ControlCodes;

    auto filtered = ::Microsoft::Console::Utils::FilterStringForPaste(stringView, option);
    if (IsXtermBracketedPasteModeEnabled())
    {
        filtered.insert(0, L"\x1b[200~");
        filtered.append(L"\x1b[201~");
    }

    if (_pfnWriteInput)
    {
        _pfnWriteInput(filtered);
    }
}

// Method Description:
// - Attempts to snap to the bottom of the buffer, if SnapOnInput is true. Does
//   nothing if SnapOnInput is set to false, or we're already at the bottom of
//   the buffer.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Terminal::TrySnapOnInput()
{
    if (_snapOnInput && _scrollOffset != 0)
    {
        auto lock = LockForWriting();
        _scrollOffset = 0;
        _NotifyScrollEvent();
    }
}

// Routine Description:
// - Relays if we are tracking mouse input
// Parameters:
// - <none>
// Return value:
// - true, if we are tracking mouse input. False, otherwise
bool Terminal::IsTrackingMouseInput() const noexcept
{
    return _terminalInput.IsTrackingMouseInput();
}

// Routine Description:
// - Relays if we are in alternate scroll mode, a special type of mouse input
//   mode where scrolling sends the arrow keypresses, but the app doesn't
//   otherwise want mouse input.
// Parameters:
// - <none>
// Return value:
// - true, if we are tracking mouse input. False, otherwise
bool Terminal::ShouldSendAlternateScroll(const unsigned int uiButton,
                                         const int32_t delta) const noexcept
{
    return _terminalInput.ShouldSendAlternateScroll(uiButton, ::base::saturated_cast<short>(delta));
}

// Method Description:
// - Given a coord, get the URI at that location
// Arguments:
// - The position relative to the viewport
std::wstring Terminal::GetHyperlinkAtViewportPosition(const til::point viewportPos)
{
    return GetHyperlinkAtBufferPosition(_ConvertToBufferCell(viewportPos));
}

std::wstring Terminal::GetHyperlinkAtBufferPosition(const til::point bufferPos)
{
    // Case 1: buffer position has a hyperlink stored in the buffer
    const auto attr = _activeBuffer().GetCellDataAt(bufferPos)->TextAttr();
    if (attr.IsHyperlink())
    {
        return _activeBuffer().GetHyperlinkUriFromId(attr.GetHyperlinkId());
    }

    // Case 2: buffer position may point to an auto-detected hyperlink
    // Case 2 - Step 1: get the auto-detected hyperlink
    std::optional<interval_tree::Interval<til::point, size_t>> result;
    const auto visibleViewport = _GetVisibleViewport();
    if (visibleViewport.IsInBounds(bufferPos))
    {
        // Hyperlink is in the current view, so let's just get it
        auto viewportPos = bufferPos;
        visibleViewport.ConvertToOrigin(&viewportPos);
        result = GetHyperlinkIntervalFromViewportPosition(viewportPos);
        if (result.has_value())
        {
            result->start = _ConvertToBufferCell(result->start);
            result->stop = _ConvertToBufferCell(result->stop);
        }
    }
    else
    {
        // Hyperlink is outside of the current view.
        // We need to find if there's a pattern at that location.
        const auto patterns = _activeBuffer().GetPatterns(bufferPos.y, bufferPos.y);

        // NOTE: patterns is stored with top y-position being 0,
        //       so we need to cleverly set the y-pos to 0.
        const til::point viewportPos{ bufferPos.x, 0 };
        const auto results = patterns.findOverlapping(viewportPos, viewportPos);
        if (!results.empty())
        {
            result = results.front();
            result->start.y += bufferPos.y;
            result->stop.y += bufferPos.y;
        }
    }

    // Case 2 - Step 2: get the auto-detected hyperlink
    if (result.has_value() && result->value == _hyperlinkPatternId)
    {
        std::wstring uri;
        const auto startIter = _activeBuffer().GetCellDataAt(result->start);
        const auto endIter = _activeBuffer().GetCellDataAt(result->stop);
        for (auto iter = startIter; iter != endIter; ++iter)
        {
            uri += iter->Chars();
        }
        return uri;
    }
    return {};
}

// Method Description:
// - Gets the hyperlink ID of the text at the given terminal position
// Arguments:
// - The position of the text relative to the viewport
// Return value:
// - The hyperlink ID
uint16_t Terminal::GetHyperlinkIdAtViewportPosition(const til::point viewportPos)
{
    return _activeBuffer().GetCellDataAt(_ConvertToBufferCell(viewportPos))->TextAttr().GetHyperlinkId();
}

// Method description:
// - Given a position in a URI pattern, gets the start and end coordinates of the URI
// Arguments:
// - The position relative to the viewport
// Return value:
// - The interval representing the start and end coordinates
std::optional<PointTree::interval> Terminal::GetHyperlinkIntervalFromViewportPosition(const til::point viewportPos)
{
    const auto results = _patternIntervalTree.findOverlapping({ viewportPos.x + 1, viewportPos.y }, viewportPos);
    if (results.size() > 0)
    {
        for (const auto& result : results)
        {
            if (result.value == _hyperlinkPatternId)
            {
                return result;
            }
        }
    }
    return std::nullopt;
}

// Method Description:
// - Send this particular (non-character) key event to the terminal.
// - The terminal will translate the key and the modifiers pressed into the
//   appropriate VT sequence for that key chord. If we do translate the key,
//   we'll return true. In that case, the event should NOT be processed any further.
// - Character events (e.g. WM_CHAR) are generally the best way to properly receive
//   keyboard input on Windows though, as the OS is suited best at handling the
//   translation of the current keyboard layout, dead keys, etc.
//   As a result of this false is returned for all key down events that contain characters.
//   SendCharEvent may then be called with the data obtained from a character event.
// - As a special case we'll always handle VK_TAB key events.
//   This must be done due to TermControl::_KeyDownHandler (one of the callers)
//   always marking tab key events as handled, causing no character event to be raised.
// Arguments:
// - vkey: The vkey of the last pressed key.
// - scanCode: The scan code of the last pressed key.
// - states: The Microsoft::Terminal::Core::ControlKeyStates representing the modifier key states.
// - keyDown: If true, the key was pressed, otherwise the key was released.
// Return Value:
// - true if we translated the key event, and it should not be processed any further.
// - false if we did not translate the key, and it should be processed into a character.
bool Terminal::SendKeyEvent(const WORD vkey,
                            const WORD scanCode,
                            const ControlKeyStates states,
                            const bool keyDown)
{
    // GH#6423 - don't snap on this key if the key that was pressed was a
    // modifier key. We'll wait for a real keystroke to snap to the bottom.
    // GH#6481 - Additionally, make sure the key was actually pressed. This
    // check will make sure we behave the same as before GH#6309
    if (IsInputKey(vkey) && keyDown)
    {
        TrySnapOnInput();
    }

    _StoreKeyEvent(vkey, scanCode);

    // Certain applications like AutoHotKey and its keyboard remapping feature,
    // send us key events using SendInput() whose values are outside of the valid range.
    // GH#7064
    if (vkey == 0 || vkey >= 0xff)
    {
        return false;
    }

    // While not explicitly permitted, a wide range of software, including Windows' own touch keyboard,
    // sets the wScan member of the KEYBDINPUT structure to 0, resulting in scanCode being 0 as well.
    // --> Alternatively get the scanCode from the vkey if possible.
    // GH#7495
    const auto sc = scanCode ? scanCode : _ScanCodeFromVirtualKey(vkey);
    if (sc == 0)
    {
        return false;
    }

    const auto isAltOnlyPressed = states.IsAltPressed() && !states.IsCtrlPressed();

    // By default Windows treats Ctrl+Alt as an alias for AltGr.
    // When the altGrAliasing setting is set to false, this behaviour should be disabled.
    //
    // Whenever possible _CharacterFromKeyEvent() will return a valid character.
    // For instance both Ctrl+Alt+Q as well as AltGr+Q return @ on a German keyboard.
    //
    // We can achieve the altGrAliasing functionality by skipping the call to _CharacterFromKeyEvent,
    // as TerminalInput::HandleKey will then fall back to using the vkey which
    // is the underlying ASCII character (e.g. A-Z) on the keyboard in our case.
    // See GH#5525/GH#6211 for more details
    const auto isSuppressedAltGrAlias = !_altGrAliasing && states.IsAltPressed() && states.IsCtrlPressed() && !states.IsAltGrPressed();
    const auto ch = isSuppressedAltGrAlias ? UNICODE_NULL : _CharacterFromKeyEvent(vkey, sc, states);

    // Delegate it to the character event handler if this is a key down event that
    // can be mapped to one (see method description above). For Alt+key combinations
    // we'll not receive another character event for some reason though.
    // -> Don't delegate the event if this is a Alt+key combination.
    //
    // As a special case we'll furthermore always handle VK_TAB
    // key events here instead of in Terminal::SendCharEvent.
    // See the method description for more information.
    if (keyDown && !isAltOnlyPressed && vkey != VK_TAB && ch != UNICODE_NULL)
    {
        return false;
    }

    const auto keyEv = SynthesizeKeyEvent(keyDown, 1, vkey, sc, ch, states.Value());
    return _handleTerminalInputResult(_terminalInput.HandleKey(keyEv));
}

// Method Description:
// - Send this particular mouse event to the terminal. The terminal will translate
//   the button and the modifiers pressed into the appropriate VT sequence for that
//   mouse event. If we do translate the key, we'll return true. In that case, the
//   event should NOT be processed any further. If we return false, the event
//   was NOT translated, and we should instead use the event normally
// Arguments:
// - viewportPos: the position of the mouse event relative to the viewport origin.
// - uiButton: the WM mouse button event code
// - states: The Microsoft::Terminal::Core::ControlKeyStates representing the modifier key states.
// - wheelDelta: the amount that the scroll wheel changed (should be 0 unless button is a WM_MOUSE*WHEEL)
// Return Value:
// - true if we translated the key event, and it should not be processed any further.
// - false if we did not translate the key, and it should be processed into a character.
bool Terminal::SendMouseEvent(til::point viewportPos, const unsigned int uiButton, const ControlKeyStates states, const short wheelDelta, const TerminalInput::MouseButtonState state)
{
    // GH#6401: VT applications should be able to receive mouse events from outside the
    // terminal buffer. This is likely to happen when the user drags the cursor offscreen.
    // We shouldn't throw away perfectly good events when they're offscreen, so we just
    // clamp them to be within the range [(0, 0), (W, H)].
    _GetMutableViewport().ToOrigin().Clamp(viewportPos);
    return _handleTerminalInputResult(_terminalInput.HandleMouse(viewportPos, uiButton, GET_KEYSTATE_WPARAM(states.Value()), wheelDelta, state));
}

// Method Description:
// - Send this particular character to the terminal.
// - This method is the counterpart to SendKeyEvent and behaves almost identical.
//   The difference is the focus on sending characters to the terminal,
//   whereas SendKeyEvent handles the sending of keys like the arrow keys.
// Arguments:
// - ch: The UTF-16 code unit to be sent.
// - scanCode: The scan code of the last pressed key. Can be left 0.
// - states: The Microsoft::Terminal::Core::ControlKeyStates representing the modifier key states.
// Return Value:
// - true if we translated the character event, and it should not be processed any further.
// - false otherwise.
bool Terminal::SendCharEvent(const wchar_t ch, const WORD scanCode, const ControlKeyStates states)
{
    auto vkey = _TakeVirtualKeyFromLastKeyEvent(scanCode);
    if (vkey == 0 && scanCode != 0)
    {
        vkey = _VirtualKeyFromScanCode(scanCode);
    }
    if (vkey == 0)
    {
        vkey = _VirtualKeyFromCharacter(ch);
    }

    // GH#1527: When the user has auto mark prompts enabled, we're going to try
    // and heuristically detect if this was the line the prompt was on.
    // * If the key was an Enter keypress (Terminal.app also marks ^C keypresses
    //   as prompts. That's omitted for now.)
    // * AND we're not in the alt buffer
    //
    // Then treat this line like it's a prompt mark.
    if (_autoMarkPrompts && vkey == VK_RETURN && !_inAltBuffer())
    {
        // * If we have a current prompt:
        //   - Then we did know that the prompt started, (we may have also
        //     already gotten a MarkCommandStart sequence). The user has pressed
        //     enter, and we're treating that like the prompt has now ended.
        //     - Perform a FTCS_COMMAND_EXECUTED, so that we start marking this
        //       as output.
        //     - This enables CMD to have full FTCS support, even though there's
        //       no point in CMD to insert a "pre exec" hook
        // * Else: We don't have a prompt. We don't know anything else, but we
        //   can set the whole line as the prompt, no command, and start the
        //   command_executed now.
        //
        // Fortunately, MarkOutputStart will do all this logic for us!
        MarkOutputStart();
    }

    const auto keyDown = SynthesizeKeyEvent(true, 1, vkey, scanCode, ch, states.Value());
    return _handleTerminalInputResult(_terminalInput.HandleKey(keyDown));
}

// Method Description:
// - Tell the terminal input that we gained or lost focus. If the client
//   requested focus events, this will send a message to them.
// - ConPTY ALWAYS wants focus events.
// Arguments:
// - focused: true if we're focused, false otherwise.
// Return Value:
// - none
void Terminal::FocusChanged(const bool focused)
{
    _handleTerminalInputResult(_terminalInput.HandleFocus(focused));
}

// Method Description:
// - Invalidates the regions described in the given pattern tree for the rendering purposes
// Arguments:
// - The interval tree containing regions that need to be invalidated
void Terminal::_InvalidatePatternTree(const interval_tree::IntervalTree<til::point, size_t>& tree)
{
    const auto vis = _VisibleStartIndex();
    auto invalidate = [=](const PointTree::interval& interval) {
        const til::point startCoord{ interval.start.x, interval.start.y + vis };
        const til::point endCoord{ interval.stop.x, interval.stop.y + vis };
        _InvalidateFromCoords(startCoord, endCoord);
    };
    tree.visit_all(invalidate);
}

// Method Description:
// - Given start and end coords, invalidates all the regions between them
// Arguments:
// - The start and end coords
void Terminal::_InvalidateFromCoords(const til::point start, const til::point end)
{
    if (start.y == end.y)
    {
        const til::inclusive_rect region{ start.x, start.y, end.x, end.y };
        _activeBuffer().TriggerRedraw(Viewport::FromInclusive(region));
    }
    else
    {
        const auto rowSize = _activeBuffer().GetRowByOffset(0).size();

        // invalidate the first line
        til::inclusive_rect region{ start.x, start.y, rowSize - 1, start.y };
        _activeBuffer().TriggerRedraw(Viewport::FromInclusive(region));

        if ((end.y - start.y) > 1)
        {
            // invalidate the lines in between the first and last line
            region = til::inclusive_rect{ 0, start.y + 1, rowSize - 1, end.y - 1 };
            _activeBuffer().TriggerRedraw(Viewport::FromInclusive(region));
        }

        // invalidate the last line
        region = til::inclusive_rect{ 0, end.y, end.x, end.y };
        _activeBuffer().TriggerRedraw(Viewport::FromInclusive(region));
    }
}

// Method Description:
// - Returns the keyboard's scan code for the given virtual key code.
// Arguments:
// - vkey: The virtual key code.
// Return Value:
// - The keyboard's scan code.
WORD Terminal::_ScanCodeFromVirtualKey(const WORD vkey) noexcept
{
    return LOWORD(MapVirtualKeyW(vkey, MAPVK_VK_TO_VSC));
}

// Method Description:
// - Returns the virtual key code for the given keyboard's scan code.
// Arguments:
// - scanCode: The keyboard's scan code.
// Return Value:
// - The virtual key code. 0 if no mapping can be found.
WORD Terminal::_VirtualKeyFromScanCode(const WORD scanCode) noexcept
{
    return LOWORD(MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK));
}

// Method Description:
// - Returns any virtual key code that produces the given character.
// Arguments:
// - scanCode: The keyboard's scan code.
// Return Value:
// - The virtual key code. 0 if no mapping can be found.
WORD Terminal::_VirtualKeyFromCharacter(const wchar_t ch) noexcept
{
    const auto vkey = LOWORD(VkKeyScanW(ch));
    return vkey == -1 ? 0 : vkey;
}

// Method Description:
// - Translates the specified virtual key code and keyboard state to the corresponding character.
// Arguments:
// - vkey: The virtual key code that initiated this keyboard event.
// - scanCode: The scan code that initiated this keyboard event.
// - states: The current keyboard state.
// Return Value:
// - The character that would result from this virtual key code and keyboard state.
wchar_t Terminal::_CharacterFromKeyEvent(const WORD vkey, const WORD scanCode, const ControlKeyStates states) noexcept
try
{
    // We might want to use GetKeyboardState() instead of building our own keyState.
    // The question is whether that's necessary though. For now it seems to work fine as it is.
    std::array<BYTE, 256> keyState = {};
    keyState.at(VK_SHIFT) = states.IsShiftPressed() ? 0x80 : 0;
    keyState.at(VK_CONTROL) = states.IsCtrlPressed() ? 0x80 : 0;
    keyState.at(VK_MENU) = states.IsAltPressed() ? 0x80 : 0;

    // For the following use of ToUnicodeEx() please look here:
    //   https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-tounicodeex

    // Technically ToUnicodeEx() can produce arbitrarily long sequences of diacritics etc.
    // Since we only handle the case of a single UTF-16 code point, we can set the buffer size to 2 though.
    std::array<wchar_t, 2> buffer;

    // wFlags:
    // * If bit 0 is set, a menu is active.
    //   If this flag is not specified ToUnicodeEx will send us character events on certain Alt+Key combinations (e.g. Alt+Arrow-Up).
    // * If bit 2 is set, keyboard state is not changed (Windows 10, version 1607 and newer)
    const auto result = ToUnicodeEx(vkey, scanCode, keyState.data(), buffer.data(), gsl::narrow_cast<int>(buffer.size()), 0b101, nullptr);

    // TODO:GH#2853 We're only handling single UTF-16 code points right now, since that's the only thing KeyEvent supports.
    return result == 1 || result == -1 ? buffer.at(0) : 0;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return UNICODE_INVALID;
}

[[maybe_unused]] bool Terminal::_handleTerminalInputResult(TerminalInput::OutputType&& out) const
{
    if (out)
    {
        const auto& str = *out;
        if (_pfnWriteInput && !str.empty())
        {
            _pfnWriteInput(str);
        }
        return true;
    }
    return false;
}

// Method Description:
// - It's possible for a single scan code on a keyboard to
//   produce different key codes depending on the keyboard state.
//   MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK) will always chose one of the
//   possibilities no matter what though and thus can't be used in SendCharEvent.
// - This method stores the key code from a key event (SendKeyEvent).
//   If the key event contains character data, handling of the event will be
//   denied, in order to delegate the work to the character event handler.
// - The character event handler (SendCharEvent) will now pick up
//   the stored key code to restore the full key event data.
// Arguments:
// - vkey: The virtual key code.
// - scanCode: The scan code.
void Terminal::_StoreKeyEvent(const WORD vkey, const WORD scanCode) noexcept
{
    _lastKeyEventCodes.emplace(KeyEventCodes{ vkey, scanCode });
}

// Method Description:
// - This method acts as a counterpart to _StoreKeyEvent and extracts a stored
//   key code. As a safety measure it'll ensure that the given scan code
//   matches the stored scan code from the previous key event.
// - See _StoreKeyEvent for more information.
// Arguments:
// - scanCode: The scan code.
// Return Value:
// - The key code matching the given scan code. Otherwise 0.
WORD Terminal::_TakeVirtualKeyFromLastKeyEvent(const WORD scanCode) noexcept
{
    const auto codes = _lastKeyEventCodes.value_or(KeyEventCodes{});
    _lastKeyEventCodes.reset();
    return codes.ScanCode == scanCode ? codes.VirtualKey : 0;
}

// Method Description:
// - Acquire a read lock on the terminal.
// Return Value:
// - a shared_lock which can be used to unlock the terminal. The shared_lock
//      will release this lock when it's destructed.
[[nodiscard]] std::unique_lock<til::recursive_ticket_lock> Terminal::LockForReading()
{
    return std::unique_lock{ _readWriteLock };
}

// Method Description:
// - Acquire a write lock on the terminal.
// Return Value:
// - a unique_lock which can be used to unlock the terminal. The unique_lock
//      will release this lock when it's destructed.
[[nodiscard]] std::unique_lock<til::recursive_ticket_lock> Terminal::LockForWriting()
{
    return std::unique_lock{ _readWriteLock };
}

// Method Description:
// - Get a reference to the terminal's read/write lock.
// Return Value:
// - a ticket_lock which can be used to manually lock or unlock the terminal.
til::recursive_ticket_lock_suspension Terminal::SuspendLock() noexcept
{
    return _readWriteLock.suspend();
}

Viewport Terminal::_GetMutableViewport() const noexcept
{
    // GH#3493: if we're in the alt buffer, then it's possible that the mutable
    // viewport's size hasn't been updated yet. In that case, use the
    // temporarily stashed _altBufferSize instead.
    return _inAltBuffer() ? Viewport::FromDimensions(_altBufferSize) :
                            _mutableViewport;
}

til::CoordType Terminal::GetBufferHeight() const noexcept
{
    return _GetMutableViewport().BottomExclusive();
}

// ViewStartIndex is also the length of the scrollback
int Terminal::ViewStartIndex() const noexcept
{
    return _inAltBuffer() ? 0 : _mutableViewport.Top();
}

int Terminal::ViewEndIndex() const noexcept
{
    return _inAltBuffer() ? _altBufferSize.height - 1 : _mutableViewport.BottomInclusive();
}

// _VisibleStartIndex is the first visible line of the buffer
int Terminal::_VisibleStartIndex() const noexcept
{
    return _inAltBuffer() ? ViewStartIndex() :
                            std::max(0, ViewStartIndex() - _scrollOffset);
}

int Terminal::_VisibleEndIndex() const noexcept
{
    return _inAltBuffer() ? ViewEndIndex() :
                            std::max(0, ViewEndIndex() - _scrollOffset);
}

Viewport Terminal::_GetVisibleViewport() const noexcept
{
    // GH#3493: if we're in the alt buffer, then it's possible that the mutable
    // viewport's size hasn't been updated yet. In that case, use the
    // temporarily stashed _altBufferSize instead.
    const til::point origin{ 0, _VisibleStartIndex() };
    const auto size{ _inAltBuffer() ? _altBufferSize :
                                      _mutableViewport.Dimensions() };
    return Viewport::FromDimensions(origin,
                                    size);
}

void Terminal::_PreserveUserScrollOffset(const int viewportDelta) noexcept
{
    // When the mutable viewport is moved down, and there's an active selection,
    // or the visible viewport isn't already at the bottom, then we want to keep
    // the visible viewport where it is. To do this, we adjust the scroll offset
    // by the same amount that we've just moved down.
    if (viewportDelta > 0 && (IsSelectionActive() || _scrollOffset != 0))
    {
        const auto maxScrollOffset = _activeBuffer().GetSize().Height() - _mutableViewport.Height();
        _scrollOffset = std::min(_scrollOffset + viewportDelta, maxScrollOffset);
    }
}

void Terminal::UserScrollViewport(const int viewTop)
{
    if (_inAltBuffer())
    {
        return;
    }

    // we're going to modify state here that the renderer could be reading.
    auto lock = LockForWriting();

    const auto clampedNewTop = std::max(0, viewTop);
    const auto realTop = ViewStartIndex();
    const auto newDelta = realTop - clampedNewTop;
    // if viewTop > realTop, we want the offset to be 0.

    _scrollOffset = std::max(0, newDelta);

    // We can use the void variant of TriggerScroll here because
    // we adjusted the viewport so it can detect the difference
    // from the previous frame drawn.
    _activeBuffer().TriggerScroll();
}

int Terminal::GetScrollOffset() noexcept
{
    return _VisibleStartIndex();
}

void Terminal::_NotifyScrollEvent() noexcept
try
{
    if (_pfnScrollPositionChanged)
    {
        const auto visible = _GetVisibleViewport();
        const auto top = visible.Top();
        const auto height = visible.Height();
        const auto bottom = this->GetBufferHeight();
        _pfnScrollPositionChanged(top, height, bottom);
    }
}
CATCH_LOG()

void Terminal::_NotifyTerminalCursorPositionChanged() noexcept
{
    if (_pfnCursorPositionChanged)
    {
        try
        {
            _pfnCursorPositionChanged();
        }
        CATCH_LOG();
    }
}

void Terminal::SetWriteInputCallback(std::function<void(std::wstring_view)> pfn) noexcept
{
    _pfnWriteInput.swap(pfn);
}

void Terminal::SetWarningBellCallback(std::function<void()> pfn) noexcept
{
    _pfnWarningBell.swap(pfn);
}

void Terminal::SetTitleChangedCallback(std::function<void(std::wstring_view)> pfn) noexcept
{
    _pfnTitleChanged.swap(pfn);
}

void Terminal::SetCopyToClipboardCallback(std::function<void(std::wstring_view)> pfn) noexcept
{
    _pfnCopyToClipboard.swap(pfn);
}

void Terminal::SetScrollPositionChangedCallback(std::function<void(const int, const int, const int)> pfn) noexcept
{
    _pfnScrollPositionChanged.swap(pfn);
}

void Terminal::SetCursorPositionChangedCallback(std::function<void()> pfn) noexcept
{
    _pfnCursorPositionChanged.swap(pfn);
}

// Method Description:
// - Allows settings a callback for settings the taskbar progress indicator
// Arguments:
// - pfn: a function callback that takes 2 size_t parameters, one indicating the progress state
//        and the other indicating the progress value
void Microsoft::Terminal::Core::Terminal::TaskbarProgressChangedCallback(std::function<void()> pfn) noexcept
{
    _pfnTaskbarProgressChanged.swap(pfn);
}

// Method Description:
// - Propagates an incoming set window visibility call from the PTY up into our window control layers
// Arguments:
// - pfn: a function callback that accepts true as "make window visible" and false as "hide window"
void Terminal::SetShowWindowCallback(std::function<void(bool)> pfn) noexcept
{
    _pfnShowWindowChanged.swap(pfn);
}

// Method Description:
// - Allows setting a callback for playing MIDI notes.
// Arguments:
// - pfn: a function callback that takes a note number, a velocity level, and a duration
void Terminal::SetPlayMidiNoteCallback(std::function<void(const int, const int, const std::chrono::microseconds)> pfn) noexcept
{
    _pfnPlayMidiNote.swap(pfn);
}

// Method Description:
// - Sets the cursor to be currently on. On/Off is tracked independently of
//   cursor visibility (hidden/visible). On/off is controlled by the cursor
//   blinker. Visibility is usually controlled by the client application. If the
//   cursor is hidden, then the cursor will remain hidden. If the cursor is
//   Visible, then it will immediately become visible.
// Arguments:
// - isVisible: whether the cursor should be visible
void Terminal::SetCursorOn(const bool isOn)
{
    auto lock = LockForWriting();
    _activeBuffer().GetCursor().SetIsOn(isOn);
}

bool Terminal::IsCursorBlinkingAllowed() const noexcept
{
    const auto& cursor = _activeBuffer().GetCursor();
    return _selectionMode != SelectionInteractionMode::Mark && cursor.IsBlinkingAllowed();
}

// Method Description:
// - Update our internal knowledge about where regex patterns are on the screen
// - This is called by TerminalControl (through a throttled function) when the visible
//   region changes (for example by text entering the buffer or scrolling)
// - INVARIANT: this function can only be called if the caller has the writing lock on the terminal
void Terminal::UpdatePatternsUnderLock()
{
    auto oldTree = _patternIntervalTree;
    _patternIntervalTree = _activeBuffer().GetPatterns(_VisibleStartIndex(), _VisibleEndIndex());
    _InvalidatePatternTree(oldTree);
    _InvalidatePatternTree(_patternIntervalTree);
}

// Method Description:
// - Clears and invalidates the interval pattern tree
// - This is called to prevent the renderer from rendering patterns while the
//   visible region is changing
void Terminal::ClearPatternTree()
{
    auto oldTree = _patternIntervalTree;
    _patternIntervalTree = {};
    _InvalidatePatternTree(oldTree);
}

// Method Description:
// - Returns the tab color
// If the starting color exists, its value is preferred
const std::optional<til::color> Terminal::GetTabColor() const
{
    if (_startingTabColor.has_value())
    {
        return _startingTabColor;
    }
    else
    {
        const auto tabColor = _renderSettings.GetColorAlias(ColorAlias::FrameBackground);
        return tabColor == INVALID_COLOR ? std::nullopt : std::optional<til::color>{ tabColor };
    }
}

// Method Description:
// - Gets the internal taskbar state value
// Return Value:
// - The taskbar state
const size_t Microsoft::Terminal::Core::Terminal::GetTaskbarState() const noexcept
{
    return _taskbarState;
}

// Method Description:
// - Gets the internal taskbar progress value
// Return Value:
// - The taskbar progress
const size_t Microsoft::Terminal::Core::Terminal::GetTaskbarProgress() const noexcept
{
    return _taskbarProgress;
}

void Microsoft::Terminal::Core::Terminal::CompletionsChangedCallback(std::function<void(std::wstring_view, unsigned int)> pfn) noexcept
{
    _pfnCompletionsChanged.swap(pfn);
}

Scheme Terminal::GetColorScheme() const
{
    Scheme s;

    s.Foreground = til::color{ _renderSettings.GetColorAlias(ColorAlias::DefaultForeground) };
    s.Background = til::color{ _renderSettings.GetColorAlias(ColorAlias::DefaultBackground) };

    // SelectionBackground is stored in the ControlAppearance
    s.CursorColor = til::color{ _renderSettings.GetColorTableEntry(TextColor::CURSOR_COLOR) };

    s.Black = til::color{ _renderSettings.GetColorTableEntry(TextColor::DARK_BLACK) };
    s.Red = til::color{ _renderSettings.GetColorTableEntry(TextColor::DARK_RED) };
    s.Green = til::color{ _renderSettings.GetColorTableEntry(TextColor::DARK_GREEN) };
    s.Yellow = til::color{ _renderSettings.GetColorTableEntry(TextColor::DARK_YELLOW) };
    s.Blue = til::color{ _renderSettings.GetColorTableEntry(TextColor::DARK_BLUE) };
    s.Purple = til::color{ _renderSettings.GetColorTableEntry(TextColor::DARK_MAGENTA) };
    s.Cyan = til::color{ _renderSettings.GetColorTableEntry(TextColor::DARK_CYAN) };
    s.White = til::color{ _renderSettings.GetColorTableEntry(TextColor::DARK_WHITE) };
    s.BrightBlack = til::color{ _renderSettings.GetColorTableEntry(TextColor::BRIGHT_BLACK) };
    s.BrightRed = til::color{ _renderSettings.GetColorTableEntry(TextColor::BRIGHT_RED) };
    s.BrightGreen = til::color{ _renderSettings.GetColorTableEntry(TextColor::BRIGHT_GREEN) };
    s.BrightYellow = til::color{ _renderSettings.GetColorTableEntry(TextColor::BRIGHT_YELLOW) };
    s.BrightBlue = til::color{ _renderSettings.GetColorTableEntry(TextColor::BRIGHT_BLUE) };
    s.BrightPurple = til::color{ _renderSettings.GetColorTableEntry(TextColor::BRIGHT_MAGENTA) };
    s.BrightCyan = til::color{ _renderSettings.GetColorTableEntry(TextColor::BRIGHT_CYAN) };
    s.BrightWhite = til::color{ _renderSettings.GetColorTableEntry(TextColor::BRIGHT_WHITE) };
    return s;
}

void Terminal::ApplyScheme(const Scheme& colorScheme)
{
    _renderSettings.SetColorAlias(ColorAlias::DefaultForeground, TextColor::DEFAULT_FOREGROUND, til::color{ colorScheme.Foreground });
    _renderSettings.SetColorAlias(ColorAlias::DefaultBackground, TextColor::DEFAULT_BACKGROUND, til::color{ colorScheme.Background });

    _renderSettings.SetColorTableEntry(TextColor::DARK_BLACK, til::color{ colorScheme.Black });
    _renderSettings.SetColorTableEntry(TextColor::DARK_RED, til::color{ colorScheme.Red });
    _renderSettings.SetColorTableEntry(TextColor::DARK_GREEN, til::color{ colorScheme.Green });
    _renderSettings.SetColorTableEntry(TextColor::DARK_YELLOW, til::color{ colorScheme.Yellow });
    _renderSettings.SetColorTableEntry(TextColor::DARK_BLUE, til::color{ colorScheme.Blue });
    _renderSettings.SetColorTableEntry(TextColor::DARK_MAGENTA, til::color{ colorScheme.Purple });
    _renderSettings.SetColorTableEntry(TextColor::DARK_CYAN, til::color{ colorScheme.Cyan });
    _renderSettings.SetColorTableEntry(TextColor::DARK_WHITE, til::color{ colorScheme.White });
    _renderSettings.SetColorTableEntry(TextColor::BRIGHT_BLACK, til::color{ colorScheme.BrightBlack });
    _renderSettings.SetColorTableEntry(TextColor::BRIGHT_RED, til::color{ colorScheme.BrightRed });
    _renderSettings.SetColorTableEntry(TextColor::BRIGHT_GREEN, til::color{ colorScheme.BrightGreen });
    _renderSettings.SetColorTableEntry(TextColor::BRIGHT_YELLOW, til::color{ colorScheme.BrightYellow });
    _renderSettings.SetColorTableEntry(TextColor::BRIGHT_BLUE, til::color{ colorScheme.BrightBlue });
    _renderSettings.SetColorTableEntry(TextColor::BRIGHT_MAGENTA, til::color{ colorScheme.BrightPurple });
    _renderSettings.SetColorTableEntry(TextColor::BRIGHT_CYAN, til::color{ colorScheme.BrightCyan });
    _renderSettings.SetColorTableEntry(TextColor::BRIGHT_WHITE, til::color{ colorScheme.BrightWhite });

    _renderSettings.SetColorTableEntry(TextColor::CURSOR_COLOR, til::color{ colorScheme.CursorColor });

    // Tell the control that the scrollbar has somehow changed. Used as a
    // workaround to force the control to redraw any scrollbar marks whose color
    // may have changed.
    _NotifyScrollEvent();
}

bool Terminal::_inAltBuffer() const noexcept
{
    return _altBuffer != nullptr;
}

TextBuffer& Terminal::_activeBuffer() const noexcept
{
    return _inAltBuffer() ? *_altBuffer : *_mainBuffer;
}

void Terminal::_updateUrlDetection()
{
    if (_detectURLs)
    {
        // Add regex pattern recognizers to the buffer
        // For now, we only add the URI regex pattern
        _hyperlinkPatternId = _activeBuffer().AddPatternRecognizer(linkPattern);
        UpdatePatternsUnderLock();
    }
    else
    {
        ClearPatternTree();
    }
}

// NOTE: This is the version of AddMark that comes from the UI. The VT api call into this too.
void Terminal::AddMark(const ScrollMark& mark,
                       const til::point& start,
                       const til::point& end,
                       const bool fromUi)
{
    if (_inAltBuffer())
    {
        return;
    }

    ScrollMark m = mark;
    m.start = start;
    m.end = end;

    // If the mark came from the user adding a mark via the UI, don't make it the active prompt mark.
    if (fromUi)
    {
        _activeBuffer().AddMark(m);
    }
    else
    {
        _activeBuffer().StartPromptMark(m);
    }

    // Tell the control that the scrollbar has somehow changed. Used as a
    // workaround to force the control to redraw any scrollbar marks
    _NotifyScrollEvent();

    // DON'T set _currentPrompt. The VT impl will do that for you. We don't want
    // UI-driven marks to set that.
}

void Terminal::ClearMark()
{
    // Look for one where the cursor is, or where the selection is if we have
    // one. Any mark that intersects the cursor/selection, on either side
    // (inclusive), will get cleared.
    const til::point cursor{ _activeBuffer().GetCursor().GetPosition() };
    til::point start{ cursor };
    til::point end{ cursor };

    if (IsSelectionActive())
    {
        start = til::point{ GetSelectionAnchor() };
        end = til::point{ GetSelectionEnd() };
    }
    _activeBuffer().ClearMarksInRange(start, end);

    // Tell the control that the scrollbar has somehow changed. Used as a
    // workaround to force the control to redraw any scrollbar marks
    _NotifyScrollEvent();
}
void Terminal::ClearAllMarks() noexcept
{
    _activeBuffer().ClearAllMarks();
    // Tell the control that the scrollbar has somehow changed. Used as a
    // workaround to force the control to redraw any scrollbar marks
    _NotifyScrollEvent();
}

const std::vector<ScrollMark>& Terminal::GetScrollMarks() const noexcept
{
    // TODO: GH#11000 - when the marks are stored per-buffer, get rid of this.
    // We want to return _no_ marks when we're in the alt buffer, to effectively
    // hide them. We need to return a reference, so we can't just ctor an empty
    // list here just for when we're in the alt buffer.
    return _activeBuffer().GetMarks();
}

til::color Terminal::GetColorForMark(const ScrollMark& mark) const
{
    if (mark.color.has_value())
    {
        return *mark.color;
    }

    switch (mark.category)
    {
    case MarkCategory::Prompt:
    {
        return _renderSettings.GetColorAlias(ColorAlias::DefaultForeground);
    }
    case MarkCategory::Error:
    {
        return _renderSettings.GetColorTableEntry(TextColor::BRIGHT_RED);
    }
    case MarkCategory::Warning:
    {
        return _renderSettings.GetColorTableEntry(TextColor::BRIGHT_YELLOW);
    }
    case MarkCategory::Success:
    {
        return _renderSettings.GetColorTableEntry(TextColor::BRIGHT_GREEN);
    }
    default:
    case MarkCategory::Info:
    {
        return _renderSettings.GetColorAlias(ColorAlias::DefaultForeground);
    }
    }
}

void Terminal::ColorSelection(const TextAttribute& attr, winrt::Microsoft::Terminal::Core::MatchMode matchMode)
{
    for (const auto [start, end] : _GetSelectionSpans())
    {
        try
        {
            if (matchMode == winrt::Microsoft::Terminal::Core::MatchMode::None)
            {
                ColorSelection(start, end, attr);
            }
            else if (matchMode == winrt::Microsoft::Terminal::Core::MatchMode::All)
            {
                const auto textBuffer = _activeBuffer().GetPlainText(start, end);
                std::wstring_view text{ textBuffer };

                if (IsBlockSelection())
                {
                    text = Utils::TrimPaste(text);
                }

                if (!text.empty())
                {
                    Search search(*this, text, Search::Direction::Forward, Search::Sensitivity::CaseInsensitive, { 0, 0 });

                    while (search.FindNext())
                    {
                        search.Color(attr);
                    }
                }
            }
        }
        CATCH_LOG();
    }
}

// Method Description:
// - Returns the position of the cursor relative to the active viewport
til::point Terminal::GetViewportRelativeCursorPosition() const noexcept
{
    const auto absoluteCursorPosition{ GetCursorPosition() };
    const auto viewport{ _GetMutableViewport() };
    return absoluteCursorPosition - viewport.Origin();
}

// These functions are used by TerminalInput, which must build in conhost
// against OneCore compatible signatures. See the definitions in
// VtApiRedirection.hpp (which we cannot include cross-project.)
// Since we do nto run on OneCore, we can dispense with the compatibility
// shims.
extern "C" UINT OneCoreSafeMapVirtualKeyW(_In_ UINT uCode, _In_ UINT uMapType)
{
    return MapVirtualKeyW(uCode, uMapType);
}

extern "C" SHORT OneCoreSafeVkKeyScanW(_In_ WCHAR ch)
{
    return VkKeyScanW(ch);
}

extern "C" SHORT OneCoreSafeGetKeyState(_In_ int nVirtKey)
{
    return GetKeyState(nVirtKey);
}
