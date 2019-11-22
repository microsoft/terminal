// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "../../terminal/parser/OutputStateMachineEngine.hpp"
#include "TerminalDispatch.hpp"
#include "../../inc/unicode.hpp"
#include "../../inc/DefaultSettings.h"
#include "../../inc/argb.h"
#include "../../types/inc/utils.hpp"

#include "winrt/Microsoft.Terminal.Settings.h"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

static std::wstring _KeyEventsToText(std::deque<std::unique_ptr<IInputEvent>>& inEventsToWrite)
{
    std::wstring wstr = L"";
    for (auto& ev : inEventsToWrite)
    {
        if (ev->EventType() == InputEventType::KeyEvent)
        {
            auto& k = static_cast<KeyEvent&>(*ev);
            auto wch = k.GetCharData();
            wstr += wch;
        }
    }
    return wstr;
}

Terminal::Terminal() :
    _mutableViewport{ Viewport::Empty() },
    _title{ L"" },
    _colorTable{},
    _defaultFg{ RGB(255, 255, 255) },
    _defaultBg{ ARGB(0, 0, 0, 0) },
    _pfnWriteInput{ nullptr },
    _scrollOffset{ 0 },
    _snapOnInput{ true },
    _boxSelection{ false },
    _selectionActive{ false },
    _allowSingleCharSelection{ true },
    _copyOnSelect{ false },
    _selectionAnchor{ 0, 0 },
    _endSelectionPosition{ 0, 0 }
{
    _stateMachine = std::make_unique<StateMachine>(new OutputStateMachineEngine(new TerminalDispatch(*this)));

    auto passAlongInput = [&](std::deque<std::unique_ptr<IInputEvent>>& inEventsToWrite) {
        if (!_pfnWriteInput)
        {
            return;
        }
        std::wstring wstr = _KeyEventsToText(inEventsToWrite);
        _pfnWriteInput(wstr);
    };

    _terminalInput = std::make_unique<TerminalInput>(passAlongInput);

    _InitializeColorTable();
}

void Terminal::Create(COORD viewportSize, SHORT scrollbackLines, IRenderTarget& renderTarget)
{
    _mutableViewport = Viewport::FromDimensions({ 0, 0 }, viewportSize);
    _scrollbackLines = scrollbackLines;
    const COORD bufferSize{ viewportSize.X,
                            Utils::ClampToShortMax(viewportSize.Y + scrollbackLines, 1) };
    const TextAttribute attr{};
    const UINT cursorSize = 12;
    _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, renderTarget);
}

// Method Description:
// - Initializes the Terminal from the given set of settings.
// Arguments:
// - settings: the set of CoreSettings we need to use to initialize the terminal
// - renderTarget: A render target the terminal can use for paint invalidation.
void Terminal::CreateFromSettings(winrt::Microsoft::Terminal::Settings::ICoreSettings settings,
                                  Microsoft::Console::Render::IRenderTarget& renderTarget)
{
    const COORD viewportSize{ Utils::ClampToShortMax(settings.InitialCols(), 1),
                              Utils::ClampToShortMax(settings.InitialRows(), 1) };
    // TODO:MSFT:20642297 - Support infinite scrollback here, if HistorySize is -1
    Create(viewportSize, Utils::ClampToShortMax(settings.HistorySize(), 0), renderTarget);

    UpdateSettings(settings);
}

// Method Description:
// - Update our internal properties to match the new values in the provided
//   CoreSettings object.
// Arguments:
// - settings: an ICoreSettings with new settings values for us to use.
void Terminal::UpdateSettings(winrt::Microsoft::Terminal::Settings::ICoreSettings settings)
{
    _defaultFg = settings.DefaultForeground();
    _defaultBg = settings.DefaultBackground();

    CursorType cursorShape = CursorType::VerticalBar;
    switch (settings.CursorShape())
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
    default:
    case CursorStyle::Bar:
        cursorShape = CursorType::VerticalBar;
        break;
    }

    _buffer->GetCursor().SetStyle(settings.CursorHeight(),
                                  settings.CursorColor(),
                                  cursorShape);

    for (int i = 0; i < 16; i++)
    {
        _colorTable[i] = settings.GetColorTableEntry(i);
    }

    _snapOnInput = settings.SnapOnInput();

    _wordDelimiters = settings.WordDelimiters();

    _copyOnSelect = settings.CopyOnSelect();

    _suppressApplicationTitle = settings.SuppressApplicationTitle();

    _startingTitle = settings.StartingTitle();

    // TODO:MSFT:21327402 - if HistorySize has changed, resize the buffer so we
    // have a smaller scrollback. We should do this carefully - if the new buffer
    // size is smaller than where the mutable viewport currently is, we'll want
    // to make sure to rotate the buffer contents upwards, so the mutable viewport
    // remains at the bottom of the buffer.
}

// Method Description:
// - Resize the terminal as the result of some user interaction.
// Arguments:
// - viewportSize: the new size of the viewport, in chars
// Return Value:
// - S_OK if we successfully resized the terminal, S_FALSE if there was
//      nothing to do (the viewportSize is the same as our current size), or an
//      appropriate HRESULT for failing to resize.
[[nodiscard]] HRESULT Terminal::UserResize(const COORD viewportSize) noexcept
{
    const auto oldDimensions = _mutableViewport.Dimensions();
    if (viewportSize == oldDimensions)
    {
        return S_FALSE;
    }

    const auto oldTop = _mutableViewport.Top();

    const short newBufferHeight = viewportSize.Y + _scrollbackLines;
    COORD bufferSize{ viewportSize.X, newBufferHeight };
    RETURN_IF_FAILED(_buffer->ResizeTraditional(bufferSize));

    auto proposedTop = oldTop;
    const auto newView = Viewport::FromDimensions({ 0, proposedTop }, viewportSize);
    const auto proposedBottom = newView.BottomExclusive();
    // If the new bottom would be below the bottom of the buffer, then slide the
    // top up so that we'll still fit within the buffer.
    if (proposedBottom > bufferSize.Y)
    {
        proposedTop -= (proposedBottom - bufferSize.Y);
    }

    _mutableViewport = Viewport::FromDimensions({ 0, proposedTop }, viewportSize);
    _scrollOffset = 0;
    _NotifyScrollEvent();

    return S_OK;
}

void Terminal::Write(std::wstring_view stringView)
{
    auto lock = LockForWriting();

    _stateMachine->ProcessString(stringView.data(), stringView.size());
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

// Method Description:
// - Send this particular key event to the terminal. The terminal will translate
//   the key and the modifiers pressed into the appropriate VT sequence for that
//   key chord. If we do translate the key, we'll return true. In that case, the
//   event should NOT be processed any further. If we return false, the event
//   was NOT translated, and we should instead use the event to try and get the
//   real character out of the event.
// Arguments:
// - vkey: The vkey of the key pressed.
// - states: The Microsoft::Terminal::Core::ControlKeyStates representing the modifier key states.
// Return Value:
// - true if we translated the key event, and it should not be processed any further.
// - false if we did not translate the key, and it should be processed into a character.
bool Terminal::SendKeyEvent(const WORD vkey, const WORD scanCode, const ControlKeyStates states)
{
    TrySnapOnInput();

    // Alt key sequences _require_ the char to be in the keyevent. If alt is
    // pressed, manually get the character that's being typed, and put it in the
    // KeyEvent.
    // DON'T manually handle Alt+Space - the system will use this to bring up
    // the system menu for restore, min/maximimize, size, move, close
    wchar_t ch = UNICODE_NULL;
    if (states.IsAltPressed() && vkey != VK_SPACE)
    {
        ch = _CharacterFromKeyEvent(vkey, scanCode, states);
    }

    if (states.IsCtrlPressed())
    {
        switch (vkey)
        {
        case 0x48:
            // Manually handle Ctrl+H. Ctrl+H should be handled as Backspace. To do this
            // correctly, the keyEvents's char needs to be set to Backspace.
            // 0x48 is the VKEY for 'H', which isn't named
            ch = UNICODE_BACKSPACE;
            break;
        case VK_SPACE:
            // Manually handle Ctrl+Space here. The terminalInput translator requires
            // the char to be set to Space for space handling to work correctly.
            ch = UNICODE_SPACE;
            break;
        }
    }

    // Manually handle Escape here. If we let it fall through, it'll come
    // back up through the character handler. It's registered as a translation
    // in TerminalInput, so we'll let TerminalInput control it.
    if (vkey == VK_ESCAPE)
    {
        ch = UNICODE_ESC;
    }

    const bool manuallyHandled = ch != UNICODE_NULL;

    KeyEvent keyEv{ true, 0, vkey, scanCode, ch, states.Value() };
    const bool translated = _terminalInput->HandleKey(&keyEv);

    return translated && manuallyHandled;
}

bool Terminal::SendCharEvent(const wchar_t ch)
{
    return _terminalInput->HandleChar(ch);
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
// - Translates the specified virtual key code and keyboard state to the corresponding character.
// Arguments:
// - vkey: The virtual key code that initiated this keyboard event.
// - scanCode: The scan code that initiated this keyboard event.
// - states: The current keyboard state.
// Return Value:
// - The character that would result from this virtual key code and keyboard state.
wchar_t Terminal::_CharacterFromKeyEvent(const WORD vkey, const WORD scanCode, const ControlKeyStates states) noexcept
{
    const auto sc = scanCode != 0 ? scanCode : _ScanCodeFromVirtualKey(vkey);

    // We might want to use GetKeyboardState() instead of building our own keyState.
    // The question is whether that's necessary though. For now it seems to work fine as it is.
    BYTE keyState[256] = {};
    keyState[VK_SHIFT] = states.IsShiftPressed() ? 0x80 : 0;
    keyState[VK_CONTROL] = states.IsCtrlPressed() ? 0x80 : 0;
    keyState[VK_MENU] = states.IsAltPressed() ? 0x80 : 0;

    // For the following use of ToUnicodeEx() please look here:
    //   https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-tounicodeex

    // Technically ToUnicodeEx() can produce arbitrarily long sequences of diacritics etc.
    // Since we only handle the case of a single UTF-16 code point, we can set the buffer size to 2 though.
    constexpr size_t bufferSize = 2;
    wchar_t buffer[bufferSize];

    // wFlags:
    // * If bit 0 is set, a menu is active.
    //   If this flag is not specified ToUnicodeEx will send us character events on certain Alt+Key combinations (e.g. Alt+Arrow-Up).
    // * If bit 2 is set, keyboard state is not changed (Windows 10, version 1607 and newer)
    const auto result = ToUnicodeEx(vkey, sc, keyState, buffer, bufferSize, 0b101, nullptr);

    // TODO:GH#2853 We're only handling single UTF-16 code points right now, since that's the only thing KeyEvent supports.
    return result == 1 || result == -1 ? buffer[0] : 0;
}

// Method Description:
// - Aquire a read lock on the terminal.
// Return Value:
// - a shared_lock which can be used to unlock the terminal. The shared_lock
//      will release this lock when it's destructed.
[[nodiscard]] std::shared_lock<std::shared_mutex> Terminal::LockForReading()
{
    return std::shared_lock<std::shared_mutex>(_readWriteLock);
}

// Method Description:
// - Aquire a write lock on the terminal.
// Return Value:
// - a unique_lock which can be used to unlock the terminal. The unique_lock
//      will release this lock when it's destructed.
[[nodiscard]] std::unique_lock<std::shared_mutex> Terminal::LockForWriting()
{
    return std::unique_lock<std::shared_mutex>(_readWriteLock);
}

Viewport Terminal::_GetMutableViewport() const noexcept
{
    return _mutableViewport;
}

short Terminal::GetBufferHeight() const noexcept
{
    return _mutableViewport.BottomExclusive();
}

// _ViewStartIndex is also the length of the scrollback
int Terminal::_ViewStartIndex() const noexcept
{
    return _mutableViewport.Top();
}

// _VisibleStartIndex is the first visible line of the buffer
int Terminal::_VisibleStartIndex() const noexcept
{
    return std::max(0, _ViewStartIndex() - _scrollOffset);
}

Viewport Terminal::_GetVisibleViewport() const noexcept
{
    const COORD origin{ 0, gsl::narrow<short>(_VisibleStartIndex()) };
    return Viewport::FromDimensions(origin,
                                    _mutableViewport.Dimensions());
}

// Writes a string of text to the buffer, then moves the cursor (and viewport)
//      in accordance with the written text.
// This method is our proverbial `WriteCharsLegacy`, and great care should be made to
//      keep it minimal and orderly, lest it become WriteCharsLegacy2ElectricBoogaloo
// TODO: MSFT 21006766
//       This needs to become stream logic on the buffer itself sooner rather than later
//       because it's otherwise impossible to avoid the Electric Boogaloo-ness here.
//       I had to make a bunch of hacks to get Japanese and emoji to work-ish.
void Terminal::_WriteBuffer(const std::wstring_view& stringView)
{
    auto& cursor = _buffer->GetCursor();
    const Viewport bufferSize = _buffer->GetSize();

    // Defer the cursor drawing while we are iterating the string, for a better performance.
    // We can not waste time displaying a cursor event when we know more text is coming right behind it.
    cursor.StartDeferDrawing();

    for (size_t i = 0; i < stringView.size(); i++)
    {
        wchar_t wch = stringView[i];
        const COORD cursorPosBefore = cursor.GetPosition();
        COORD proposedCursorPosition = cursorPosBefore;
        bool notifyScroll = false;

        if (wch == UNICODE_LINEFEED)
        {
            proposedCursorPosition.Y++;
        }
        else if (wch == UNICODE_CARRIAGERETURN)
        {
            proposedCursorPosition.X = 0;
        }
        else if (wch == UNICODE_BACKSPACE)
        {
            if (cursorPosBefore.X == 0)
            {
                proposedCursorPosition.X = bufferSize.Width() - 1;
                proposedCursorPosition.Y--;
            }
            else
            {
                proposedCursorPosition.X--;
            }
        }
        else if (wch == UNICODE_BEL)
        {
            // TODO: GitHub #1883
            // For now its empty just so we don't try to write the BEL character
        }
        else
        {
            // TODO: MSFT 21006766
            // This is not great but I need it demoable. Fix by making a buffer stream writer.
            if (wch >= 0xD800 && wch <= 0xDFFF)
            {
                OutputCellIterator it{ stringView.substr(i, 2), _buffer->GetCurrentAttributes() };
                const auto end = _buffer->Write(it);
                const auto cellDistance = end.GetCellDistance(it);
                i += cellDistance - 1;
                proposedCursorPosition.X += gsl::narrow<SHORT>(cellDistance);
            }
            else
            {
                OutputCellIterator it{ stringView.substr(i, 1), _buffer->GetCurrentAttributes() };
                const auto end = _buffer->Write(it);
                const auto cellDistance = end.GetCellDistance(it);
                proposedCursorPosition.X += gsl::narrow<SHORT>(cellDistance);
            }
        }

        // If we're about to scroll past the bottom of the buffer, instead cycle the buffer.
        const auto newRows = proposedCursorPosition.Y - bufferSize.Height() + 1;
        if (newRows > 0)
        {
            for (auto dy = 0; dy < newRows; dy++)
            {
                _buffer->IncrementCircularBuffer();
                proposedCursorPosition.Y--;
            }
            notifyScroll = true;
        }

        // This section is essentially equivalent to `AdjustCursorPosition`
        // Update Cursor Position
        cursor.SetPosition(proposedCursorPosition);

        const COORD cursorPosAfter = cursor.GetPosition();

        // Move the viewport down if the cursor moved below the viewport.
        if (cursorPosAfter.Y > _mutableViewport.BottomInclusive())
        {
            const auto newViewTop = std::max(0, cursorPosAfter.Y - (_mutableViewport.Height() - 1));
            if (newViewTop != _mutableViewport.Top())
            {
                _mutableViewport = Viewport::FromDimensions({ 0, gsl::narrow<short>(newViewTop) }, _mutableViewport.Dimensions());
                notifyScroll = true;
            }
        }

        if (notifyScroll)
        {
            _buffer->GetRenderTarget().TriggerRedrawAll();
            _NotifyScrollEvent();
        }
    }

    cursor.EndDeferDrawing();
}

void Terminal::UserScrollViewport(const int viewTop)
{
    const auto clampedNewTop = std::max(0, viewTop);
    const auto realTop = _ViewStartIndex();
    const auto newDelta = realTop - clampedNewTop;
    // if viewTop > realTop, we want the offset to be 0.

    _scrollOffset = std::max(0, newDelta);
    _buffer->GetRenderTarget().TriggerRedrawAll();
}

int Terminal::GetScrollOffset()
{
    return _VisibleStartIndex();
}

void Terminal::_NotifyScrollEvent()
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

void Terminal::SetWriteInputCallback(std::function<void(std::wstring&)> pfn) noexcept
{
    _pfnWriteInput = pfn;
}

void Terminal::SetTitleChangedCallback(std::function<void(const std::wstring_view&)> pfn) noexcept
{
    _pfnTitleChanged = pfn;
}

void Terminal::SetScrollPositionChangedCallback(std::function<void(const int, const int, const int)> pfn) noexcept
{
    _pfnScrollPositionChanged = pfn;
}

// Method Description:
// - Allows setting a callback for when the background color is changed
// Arguments:
// - pfn: a function callback that takes a uint32 (DWORD COLORREF) color in the format 0x00BBGGRR
void Terminal::SetBackgroundCallback(std::function<void(const uint32_t)> pfn) noexcept
{
    _pfnBackgroundColorChanged = pfn;
}

void Terminal::_InitializeColorTable()
{
    gsl::span<COLORREF> tableView = { &_colorTable[0], gsl::narrow<ptrdiff_t>(_colorTable.size()) };
    // First set up the basic 256 colors
    Utils::Initialize256ColorTable(tableView);
    // Then use fill the first 16 values with the Campbell scheme
    Utils::InitializeCampbellColorTable(tableView);
    // Then make sure all the values have an alpha of 255
    Utils::SetColorTableAlpha(tableView, 0xff);
}

// Method Description:
// - Sets the visibility of the text cursor.
// Arguments:
// - isVisible: whether the cursor should be visible
void Terminal::SetCursorVisible(const bool isVisible) noexcept
{
    auto& cursor = _buffer->GetCursor();
    cursor.SetIsVisible(isVisible);
}

bool Terminal::IsCursorBlinkingAllowed() const noexcept
{
    const auto& cursor = _buffer->GetCursor();
    return cursor.IsBlinkingAllowed();
}
