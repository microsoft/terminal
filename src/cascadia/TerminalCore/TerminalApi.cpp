// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "../src/inc/unicode.hpp"

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

// Print puts the text in the buffer and moves the cursor
void Terminal::PrintString(std::wstring_view stringView)
{
    _WriteBuffer(stringView);
}

TextAttribute Terminal::GetTextAttributes() const
{
    return _activeBuffer().GetCurrentAttributes();
}

bool Terminal::ReturnResponse(std::wstring_view responseString)
{
    if (!_pfnWriteInput)
    {
        return false;
    }
    _pfnWriteInput(responseString);
    return true;
}

void Terminal::SetTextAttributes(const TextAttribute& attrs)
{
    _activeBuffer().SetCurrentAttributes(attrs);
}

Viewport Terminal::GetBufferSize()
{
    return _activeBuffer().GetSize();
}

void Terminal::SetCursorPosition(til::point pos)
{
    const auto viewport = _GetMutableViewport();
    const til::point viewOrigin{ viewport.Origin() };
    auto newPos = til::unwrap_coord(viewOrigin + pos);
    viewport.Clamp(newPos);
    _activeBuffer().GetCursor().SetPosition(newPos);
}

til::point Terminal::GetCursorPosition()
{
    const til::point absoluteCursorPos{ _activeBuffer().GetCursor().GetPosition() };
    const auto viewport = _GetMutableViewport();
    const til::point viewOrigin{ viewport.Origin() };
    // TODO assert that the coord is > (0, 0) && <(view.W, view.H)
    return absoluteCursorPos - viewOrigin;
}

// Method Description:
// - Moves the cursor down one line, and possibly also to the leftmost column.
// Arguments:
// - withReturn, set to true if a carriage return should be performed as well.
// Return value:
// - <none>
void Terminal::CursorLineFeed(const bool withReturn)
{
    auto cursorPos = _activeBuffer().GetCursor().GetPosition();

    // since we explicitly just moved down a row, clear the wrap status on the
    // row we just came from
    _activeBuffer().GetRowByOffset(cursorPos.Y).SetWrapForced(false);

    cursorPos.Y++;
    if (withReturn)
    {
        cursorPos.X = 0;
    }
    _AdjustCursorPosition(cursorPos);
}

// Method Description:
// - deletes count characters starting from the cursor's current position
// - it moves over the remaining text to 'replace' the deleted text
// - for example, if the buffer looks like this ('|' is the cursor): [abc|def]
// - calling DeleteCharacter(1) will change it to: [abc|ef],
// - i.e. the 'd' gets deleted and the 'ef' gets shifted over 1 space and **retain their previous text attributes**
// Arguments:
// - count, the number of characters to delete
// Return value:
// - <none>
void Terminal::DeleteCharacter(const til::CoordType count)
{
    const auto cursorPos = _activeBuffer().GetCursor().GetPosition();
    const auto copyToPos = cursorPos;
    const til::point copyFromPos{ cursorPos.X + count, cursorPos.Y };
    const auto viewport = _GetMutableViewport();

    const auto sourceWidth = viewport.RightExclusive() - copyFromPos.X;

    // Get a rectangle of the source
    auto source = Viewport::FromDimensions(til::unwrap_coord(copyFromPos), gsl::narrow<short>(sourceWidth), 1);

    // Get a rectangle of the target
    const auto target = Viewport::FromDimensions(copyToPos, source.Dimensions());
    const auto walkDirection = Viewport::DetermineWalkDirection(source, target);

    auto sourcePos = source.GetWalkOrigin(walkDirection);
    auto targetPos = target.GetWalkOrigin(walkDirection);

    // Iterate over the source cell data and copy it over to the target
    do
    {
        const auto data = OutputCell(*(_activeBuffer().GetCellDataAt(sourcePos)));
        _activeBuffer().Write(OutputCellIterator({ &data, 1 }), targetPos);
    } while (source.WalkInBounds(sourcePos, walkDirection) && target.WalkInBounds(targetPos, walkDirection));
}

// Method Description:
// - Inserts count spaces starting from the cursor's current position, moving over the existing text
// - for example, if the buffer looks like this ('|' is the cursor): [abc|def]
// - calling InsertCharacter(1) will change it to: [abc| def],
// - i.e. the 'def' gets shifted over 1 space and **retain their previous text attributes**
// Arguments:
// - count, the number of spaces to insert
// Return value:
// - <none>
void Terminal::InsertCharacter(const til::CoordType count)
{
    // NOTE: the code below is _extremely_ similar to DeleteCharacter
    // We will want to use this same logic and implement a helper function instead
    // that does the 'move a region from here to there' operation
    // TODO: GitHub issue #2163
    const auto cursorPos = _activeBuffer().GetCursor().GetPosition();
    const auto copyFromPos = cursorPos;
    const til::point copyToPos{ cursorPos.X + count, cursorPos.Y };
    const auto viewport = _GetMutableViewport();
    const auto sourceWidth = viewport.RightExclusive() - copyFromPos.X;

    // Get a rectangle of the source
    auto source = Viewport::FromDimensions(copyFromPos, gsl::narrow<short>(sourceWidth), 1);

    // Get a rectangle of the target
    const auto target = Viewport::FromDimensions(til::unwrap_coord(copyToPos), source.Dimensions());
    const auto walkDirection = Viewport::DetermineWalkDirection(source, target);

    auto sourcePos = source.GetWalkOrigin(walkDirection);
    auto targetPos = target.GetWalkOrigin(walkDirection);

    // Iterate over the source cell data and copy it over to the target
    do
    {
        const auto data = OutputCell(*(_activeBuffer().GetCellDataAt(sourcePos)));
        _activeBuffer().Write(OutputCellIterator({ &data, 1 }), targetPos);
    } while (source.WalkInBounds(sourcePos, walkDirection) && target.WalkInBounds(targetPos, walkDirection));
    const auto eraseIter = OutputCellIterator(UNICODE_SPACE, _activeBuffer().GetCurrentAttributes(), count);
    _activeBuffer().Write(eraseIter, cursorPos);
}

void Terminal::EraseCharacters(const til::CoordType numChars)
{
    const auto absoluteCursorPos = _activeBuffer().GetCursor().GetPosition();
    const auto viewport = _GetMutableViewport();
    const auto distanceToRight = viewport.RightExclusive() - absoluteCursorPos.X;
    const auto fillLimit = std::min(numChars, distanceToRight);
    const auto eraseIter = OutputCellIterator(UNICODE_SPACE, _activeBuffer().GetCurrentAttributes(), fillLimit);
    _activeBuffer().Write(eraseIter, absoluteCursorPos);
}

// Method description:
// - erases a line of text, either from
// 1. beginning to the cursor's position
// 2. cursor's position to end
// 3. beginning to end
// - depending on the erase type
// Arguments:
// - the erase type
// Return value:
// - true if succeeded, false otherwise
bool Terminal::EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType)
{
    const auto cursorPos = _activeBuffer().GetCursor().GetPosition();
    const auto viewport = _GetMutableViewport();
    til::point startPos;
    startPos.Y = cursorPos.Y;
    // nlength determines the number of spaces we need to write
    DWORD nlength = 0;

    // Determine startPos.X and nlength by the eraseType
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        nlength = cursorPos.X - viewport.Left() + 1;
        break;
    case DispatchTypes::EraseType::ToEnd:
        startPos.X = cursorPos.X;
        nlength = viewport.RightExclusive() - startPos.X;
        break;
    case DispatchTypes::EraseType::All:
        startPos.X = viewport.Left();
        nlength = viewport.RightExclusive() - startPos.X;
        break;
    default:
        return false;
    }

    const auto eraseIter = OutputCellIterator(UNICODE_SPACE, _activeBuffer().GetCurrentAttributes(), nlength);

    // Explicitly turn off end-of-line wrap-flag-setting when erasing cells.
    _activeBuffer().Write(eraseIter, til::unwrap_coord(startPos), false);
    return true;
}

// Method description:
// - erases text in the buffer in two ways depending on erase type
// 1. 'erases' all text visible to the user (i.e. the text in the viewport)
// 2. erases all the text in the scrollback
// Arguments:
// - the erase type
// Return Value:
// - true if succeeded, false otherwise
bool Terminal::EraseInDisplay(const DispatchTypes::EraseType eraseType)
{
    // Store the relative cursor position so we can restore it later after we move the viewport
    const auto cursorPos = _activeBuffer().GetCursor().GetPosition();
#pragma warning(suppress : 26496) // This is written by ConvertToOrigin, cpp core checks is wrong saying it should be const.
    auto relativeCursor = cursorPos;
    const auto viewport = _GetMutableViewport();

    viewport.ConvertToOrigin(&relativeCursor);

    // Initialize the new location of the viewport
    // the top and bottom parameters are determined by the eraseType
    SMALL_RECT newWin;
    newWin.Left = viewport.Left();
    newWin.Right = viewport.RightExclusive();

    if (eraseType == DispatchTypes::EraseType::All)
    {
        // If we're in the alt buffer, take a shortcut. Just increment the buffer enough to cycle the whole thing out and start fresh. This
        if (_inAltBuffer())
        {
            for (auto i = 0; i < viewport.Height(); i++)
            {
                _activeBuffer().IncrementCircularBuffer();
            }
            return true;
        }
        // In this case, we simply move the viewport down, effectively pushing whatever text was on the screen into the scrollback
        // and thus 'erasing' the text visible to the user
        const auto coordLastChar = _activeBuffer().GetLastNonSpaceCharacter(viewport);
        if (coordLastChar.X == 0 && coordLastChar.Y == 0)
        {
            // Nothing to clear, just return
            return true;
        }

        short sNewTop = coordLastChar.Y + 1;

        // Increment the circular buffer only if the new location of the viewport would be 'below' the buffer
        const auto delta = (sNewTop + viewport.Height()) - (_activeBuffer().GetSize().Height());
        for (auto i = 0; i < delta; i++)
        {
            _activeBuffer().IncrementCircularBuffer();
            sNewTop--;
        }

        newWin.Top = sNewTop;
        newWin.Bottom = sNewTop + viewport.Height();
    }
    else if (eraseType == DispatchTypes::EraseType::Scrollback)
    {
        // We only want to erase the scrollback, and leave everything else on the screen as it is
        // so we grab the text in the viewport and rotate it up to the top of the buffer
        COORD scrollFromPos{ 0, 0 };
        viewport.ConvertFromOrigin(&scrollFromPos);
        _activeBuffer().ScrollRows(scrollFromPos.Y, viewport.Height(), -scrollFromPos.Y);

        // Since we only did a rotation, the text that was in the scrollback is now _below_ where we are going to move the viewport
        // and we have to make sure we erase that text
        const auto eraseStart = viewport.Height();
        const auto eraseEnd = _activeBuffer().GetLastNonSpaceCharacter(viewport).Y;
        for (auto i = eraseStart; i <= eraseEnd; i++)
        {
            _activeBuffer().GetRowByOffset(i).Reset(_activeBuffer().GetCurrentAttributes());
        }

        // Reset the scroll offset now because there's nothing for the user to 'scroll' to
        _scrollOffset = 0;

        newWin.Top = 0;
        newWin.Bottom = viewport.Height();
    }
    else
    {
        return false;
    }

    // Move the viewport, adjust the scroll bar if needed, and restore the old cursor position
    _mutableViewport = Viewport::FromExclusive(newWin);
    Terminal::_NotifyScrollEvent();
    SetCursorPosition(til::point{ relativeCursor });

    return true;
}

void Terminal::WarningBell()
{
    _pfnWarningBell();
}

void Terminal::SetWindowTitle(std::wstring_view title)
{
    if (!_suppressApplicationTitle)
    {
        _title.emplace(title);
        _pfnTitleChanged(_title.value());
    }
}

// Method Description:
// - Retrieves the value in the colortable at the specified index.
// Arguments:
// - tableIndex: the index of the color table to retrieve.
// Return Value:
// - the COLORREF value for the color at that index in the table.
COLORREF Terminal::GetColorTableEntry(const size_t tableIndex) const
{
    return _renderSettings.GetColorTableEntry(tableIndex);
}

// Method Description:
// - Updates the value in the colortable at index tableIndex to the new color
//   color. color is a COLORREF, format 0x00BBGGRR.
// Arguments:
// - tableIndex: the index of the color table to update.
// - color: the new COLORREF to use as that color table value.
// Return Value:
// - <none>
void Terminal::SetColorTableEntry(const size_t tableIndex, const COLORREF color)
{
    _renderSettings.SetColorTableEntry(tableIndex, color);

    if (tableIndex == _renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground))
    {
        _pfnBackgroundColorChanged(color);
    }

    // Repaint everything - the colors might have changed
    _activeBuffer().TriggerRedrawAll();
}

// Method Description:
// - Sets the position in the color table for the given color alias.
// Arguments:
// - alias: the color alias to update.
// - tableIndex: the new position of the alias in the color table.
// Return Value:
// - <none>
void Terminal::SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex)
{
    _renderSettings.SetColorAliasIndex(alias, tableIndex);
}

// Method Description:
// - Sets the cursor style to the given style.
// Arguments:
// - cursorStyle: the style to be set for the cursor
// Return Value:
// - <none>
void Terminal::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle)
{
    auto finalCursorType = CursorType::Legacy;
    auto shouldBlink = false;

    switch (cursorStyle)
    {
    case DispatchTypes::CursorStyle::UserDefault:
        finalCursorType = _defaultCursorShape;
        shouldBlink = true;
        break;
    case DispatchTypes::CursorStyle::BlinkingBlock:
        finalCursorType = CursorType::FullBox;
        shouldBlink = true;
        break;
    case DispatchTypes::CursorStyle::SteadyBlock:
        finalCursorType = CursorType::FullBox;
        shouldBlink = false;
        break;
    case DispatchTypes::CursorStyle::BlinkingUnderline:
        finalCursorType = CursorType::Underscore;
        shouldBlink = true;
        break;
    case DispatchTypes::CursorStyle::SteadyUnderline:
        finalCursorType = CursorType::Underscore;
        shouldBlink = false;
        break;
    case DispatchTypes::CursorStyle::BlinkingBar:
        finalCursorType = CursorType::VerticalBar;
        shouldBlink = true;
        break;
    case DispatchTypes::CursorStyle::SteadyBar:
        finalCursorType = CursorType::VerticalBar;
        shouldBlink = false;
        break;

    default:
        // Invalid argument should be ignored.
        return;
    }

    _activeBuffer().GetCursor().SetType(finalCursorType);
    _activeBuffer().GetCursor().SetBlinkingAllowed(shouldBlink);
}

void Terminal::SetInputMode(const TerminalInput::Mode mode, const bool enabled)
{
    _terminalInput->SetInputMode(mode, enabled);
}

void Terminal::SetRenderMode(const RenderSettings::Mode mode, const bool enabled)
{
    _renderSettings.SetRenderMode(mode, enabled);

    // Repaint everything - the colors will have changed
    _activeBuffer().TriggerRedrawAll();
}

void Terminal::EnableXtermBracketedPasteMode(const bool enabled)
{
    _bracketedPasteMode = enabled;
}

bool Terminal::IsXtermBracketedPasteModeEnabled() const
{
    return _bracketedPasteMode;
}

bool Terminal::IsVtInputEnabled() const
{
    // We should never be getting this call in Terminal.
    FAIL_FAST();
}

void Terminal::SetCursorVisibility(const bool visible)
{
    _activeBuffer().GetCursor().SetIsVisible(visible);
}

void Terminal::EnableCursorBlinking(const bool enable)
{
    _activeBuffer().GetCursor().SetBlinkingAllowed(enable);

    // GH#2642 - From what we've gathered from other terminals, when blinking is
    // disabled, the cursor should remain On always, and have the visibility
    // controlled by the IsVisible property. So when you do a printf "\e[?12l"
    // to disable blinking, the cursor stays stuck On. At this point, only the
    // cursor visibility property controls whether the user can see it or not.
    // (Yes, the cursor can be On and NOT Visible)
    _activeBuffer().GetCursor().SetIsOn(true);
}

void Terminal::CopyToClipboard(std::wstring_view content)
{
    _pfnCopyToClipboard(content);
}

// Method Description:
// - Updates the buffer's current text attributes to start a hyperlink
// Arguments:
// - The hyperlink URI
// - The customID provided (if there was one)
// Return Value:
// - <none>
void Terminal::AddHyperlink(std::wstring_view uri, std::wstring_view params)
{
    auto attr = _activeBuffer().GetCurrentAttributes();
    const auto id = _activeBuffer().GetHyperlinkId(uri, params);
    attr.SetHyperlinkId(id);
    _activeBuffer().SetCurrentAttributes(attr);
    _activeBuffer().AddHyperlinkToMap(uri, id);
}

// Method Description:
// - Updates the buffer's current text attributes to end a hyperlink
// Return Value:
// - <none>
void Terminal::EndHyperlink()
{
    auto attr = _activeBuffer().GetCurrentAttributes();
    attr.SetHyperlinkId(0);
    _activeBuffer().SetCurrentAttributes(attr);
}

// Method Description:
// - Updates the taskbar progress indicator
// Arguments:
// - state: indicates the progress state
// - progress: indicates the progress value
// Return Value:
// - <none>
void Terminal::SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress)
{
    _taskbarState = static_cast<size_t>(state);

    switch (state)
    {
    case DispatchTypes::TaskbarState::Clear:
        // Always set progress to 0 in this case
        _taskbarProgress = 0;
        break;
    case DispatchTypes::TaskbarState::Set:
        // Always set progress to the value given in this case
        _taskbarProgress = progress;
        break;
    case DispatchTypes::TaskbarState::Indeterminate:
        // Leave the progress value unchanged in this case
        break;
    case DispatchTypes::TaskbarState::Error:
    case DispatchTypes::TaskbarState::Paused:
        // In these 2 cases, if the given progress value is 0, then
        // leave the progress value unchanged, unless the current progress
        // value is 0, in which case set it to a 'minimum' value (10 in our case);
        // if the given progress value is greater than 0, then set the progress value
        if (progress == 0)
        {
            if (_taskbarProgress == 0)
            {
                _taskbarProgress = TaskbarMinProgress;
            }
        }
        else
        {
            _taskbarProgress = progress;
        }
        break;
    }

    if (_pfnTaskbarProgressChanged)
    {
        _pfnTaskbarProgressChanged();
    }
}

void Terminal::SetWorkingDirectory(std::wstring_view uri)
{
    _workingDirectory = uri;
}

std::wstring_view Terminal::GetWorkingDirectory()
{
    return _workingDirectory;
}

// Method Description:
// - Saves the current text attributes to an internal stack.
// Arguments:
// - options, cOptions: if present, specify which portions of the current text attributes
//   should be saved. Only a small subset of GraphicsOptions are actually supported;
//   others are ignored. If no options are specified, all attributes are stored.
// Return Value:
// - <none>
void Terminal::PushGraphicsRendition(const VTParameters options)
{
    _sgrStack.Push(_activeBuffer().GetCurrentAttributes(), options);
}

// Method Description:
// - Restores text attributes from the internal stack. If only portions of text attributes
//   were saved, combines those with the current attributes.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Terminal::PopGraphicsRendition()
{
    const auto current = _activeBuffer().GetCurrentAttributes();
    _activeBuffer().SetCurrentAttributes(_sgrStack.Pop(current));
}

void Terminal::UseAlternateScreenBuffer()
{
    // the new alt buffer is exactly the size of the viewport.
    _altBufferSize = til::size{ _mutableViewport.Dimensions() };

    const auto cursorSize = _mainBuffer->GetCursor().GetSize();

    ClearSelection();
    _mainBuffer->ClearPatternRecognizers();

    // Create a new alt buffer
    _altBuffer = std::make_unique<TextBuffer>(_altBufferSize.to_win32_coord(),
                                              TextAttribute{},
                                              cursorSize,
                                              true,
                                              _mainBuffer->GetRenderer());
    _mainBuffer->SetAsActiveBuffer(false);

    // Copy our cursor state to the new buffer's cursor
    {
        // Update the alt buffer's cursor style, visibility, and position to match our own.
        auto& myCursor = _mainBuffer->GetCursor();
        auto& tgtCursor = _altBuffer->GetCursor();
        tgtCursor.SetStyle(myCursor.GetSize(), myCursor.GetType());
        tgtCursor.SetIsVisible(myCursor.IsVisible());
        tgtCursor.SetBlinkingAllowed(myCursor.IsBlinkingAllowed());

        // The new position should match the viewport-relative position of the main buffer.
        auto tgtCursorPos = myCursor.GetPosition();
        tgtCursorPos.Y -= _mutableViewport.Top();
        tgtCursor.SetPosition(tgtCursorPos);
    }

    // update all the hyperlinks on the screen
    _updateUrlDetection();

    // GH#3321: Make sure we let the TerminalInput know that we switched
    // buffers. This might affect how we interpret certain mouse events.
    _terminalInput->UseAlternateScreenBuffer();

    // Update scrollbars
    _NotifyScrollEvent();

    // redraw the screen
    try
    {
        _activeBuffer().TriggerRedrawAll();
    }
    CATCH_LOG();
}
void Terminal::UseMainScreenBuffer()
{
    // Short-circuit: do nothing.
    if (!_inAltBuffer())
    {
        return;
    }

    ClearSelection();

    // Copy our cursor state back to the main buffer's cursor
    {
        // Update the alt buffer's cursor style, visibility, and position to match our own.
        auto& myCursor = _altBuffer->GetCursor();
        auto& tgtCursor = _mainBuffer->GetCursor();
        tgtCursor.SetStyle(myCursor.GetSize(), myCursor.GetType());
        tgtCursor.SetIsVisible(myCursor.IsVisible());
        tgtCursor.SetBlinkingAllowed(myCursor.IsBlinkingAllowed());

        // The new position should match the viewport-relative position of the main buffer.
        // This is the equal and opposite effect of what we did in UseAlternateScreenBuffer
        auto tgtCursorPos = myCursor.GetPosition();
        tgtCursorPos.Y += _mutableViewport.Top();
        tgtCursor.SetPosition(tgtCursorPos);
    }

    _mainBuffer->SetAsActiveBuffer(true);
    // destroy the alt buffer
    _altBuffer = nullptr;

    if (_deferredResize.has_value())
    {
        LOG_IF_FAILED(UserResize(_deferredResize.value().to_win32_coord()));
        _deferredResize = std::nullopt;
    }

    // update all the hyperlinks on the screen
    _mainBuffer->ClearPatternRecognizers();
    _updateUrlDetection();

    // GH#3321: Make sure we let the TerminalInput know that we switched
    // buffers. This might affect how we interpret certain mouse events.
    _terminalInput->UseMainScreenBuffer();

    // Update scrollbars
    _NotifyScrollEvent();

    // redraw the screen
    try
    {
        _activeBuffer().TriggerRedrawAll();
    }
    CATCH_LOG();
}

// Method Description:
// - Reacts to a client asking us to show or hide the window.
// Arguments:
// - showOrHide - True for show. False for hide.
// Return Value:
// - <none>
void Terminal::ShowWindow(bool showOrHide)
{
    if (_pfnShowWindowChanged)
    {
        _pfnShowWindowChanged(showOrHide);
    }
}
