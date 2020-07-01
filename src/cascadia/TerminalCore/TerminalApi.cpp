// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "../src/inc/unicode.hpp"

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

// Print puts the text in the buffer and moves the cursor
bool Terminal::PrintString(std::wstring_view stringView) noexcept
try
{
    _WriteBuffer(stringView);
    return true;
}
CATCH_LOG_RETURN_FALSE()

bool Terminal::ExecuteChar(wchar_t wch) noexcept
try
{
    _WriteBuffer({ &wch, 1 });
    return true;
}
CATCH_LOG_RETURN_FALSE()

bool Terminal::SetTextToDefaults(bool foreground, bool background) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    if (foreground)
    {
        attrs.SetDefaultForeground();
    }
    if (background)
    {
        attrs.SetDefaultBackground();
    }
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetTextForegroundIndex(BYTE colorIndex) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetIndexedForeground(colorIndex);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetTextBackgroundIndex(BYTE colorIndex) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetIndexedBackground(colorIndex);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetTextForegroundIndex256(BYTE colorIndex) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetIndexedForeground256(colorIndex);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetTextBackgroundIndex256(BYTE colorIndex) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetIndexedBackground256(colorIndex);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetTextRgbColor(COLORREF color, bool foreground) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetColor(color, foreground);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::BoldText(bool boldOn) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetBold(boldOn);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::UnderlineText(bool underlineOn) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetUnderline(underlineOn);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::ReverseText(bool reversed) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetReverseVideo(reversed);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetCursorPosition(short x, short y) noexcept
try
{
    const auto viewport = _GetMutableViewport();
    const auto viewOrigin = viewport.Origin();
    const short absoluteX = viewOrigin.X + x;
    const short absoluteY = viewOrigin.Y + y;
    COORD newPos{ absoluteX, absoluteY };
    viewport.Clamp(newPos);
    _buffer->GetCursor().SetPosition(newPos);

    return true;
}
CATCH_LOG_RETURN_FALSE()

COORD Terminal::GetCursorPosition() noexcept
{
    const auto absoluteCursorPos = _buffer->GetCursor().GetPosition();
    const auto viewport = _GetMutableViewport();
    const auto viewOrigin = viewport.Origin();
    const short relativeX = absoluteCursorPos.X - viewOrigin.X;
    const short relativeY = absoluteCursorPos.Y - viewOrigin.Y;
    COORD newPos{ relativeX, relativeY };

    // TODO assert that the coord is > (0, 0) && <(view.W, view.H)
    return newPos;
}

// Method Description:
// - Moves the cursor down one line, and possibly also to the leftmost column.
// Arguments:
// - withReturn, set to true if a carriage return should be performed as well.
// Return value:
// - true if succeeded, false otherwise
bool Terminal::CursorLineFeed(const bool withReturn) noexcept
try
{
    auto cursorPos = _buffer->GetCursor().GetPosition();

    // since we explicitly just moved down a row, clear the wrap status on the
    // row we just came from
    _buffer->GetRowByOffset(cursorPos.Y).GetCharRow().SetWrapForced(false);

    cursorPos.Y++;
    if (withReturn)
    {
        cursorPos.X = 0;
    }
    _AdjustCursorPosition(cursorPos);

    return true;
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - deletes count characters starting from the cursor's current position
// - it moves over the remaining text to 'replace' the deleted text
// - for example, if the buffer looks like this ('|' is the cursor): [abc|def]
// - calling DeleteCharacter(1) will change it to: [abc|ef],
// - i.e. the 'd' gets deleted and the 'ef' gets shifted over 1 space and **retain their previous text attributes**
// Arguments:
// - count, the number of characters to delete
// Return value:
// - true if succeeded, false otherwise
bool Terminal::DeleteCharacter(const size_t count) noexcept
try
{
    SHORT dist;
    if (!SUCCEEDED(SizeTToShort(count, &dist)))
    {
        return false;
    }
    const auto cursorPos = _buffer->GetCursor().GetPosition();
    const auto copyToPos = cursorPos;
    const COORD copyFromPos{ cursorPos.X + dist, cursorPos.Y };
    const auto sourceWidth = _mutableViewport.RightExclusive() - copyFromPos.X;
    SHORT width;
    if (!SUCCEEDED(UIntToShort(sourceWidth, &width)))
    {
        return false;
    }

    // Get a rectangle of the source
    auto source = Viewport::FromDimensions(copyFromPos, width, 1);

    // Get a rectangle of the target
    const auto target = Viewport::FromDimensions(copyToPos, source.Dimensions());
    const auto walkDirection = Viewport::DetermineWalkDirection(source, target);

    auto sourcePos = source.GetWalkOrigin(walkDirection);
    auto targetPos = target.GetWalkOrigin(walkDirection);

    // Iterate over the source cell data and copy it over to the target
    do
    {
        const auto data = OutputCell(*(_buffer->GetCellDataAt(sourcePos)));
        _buffer->Write(OutputCellIterator({ &data, 1 }), targetPos);
    } while (source.WalkInBounds(sourcePos, walkDirection) && target.WalkInBounds(targetPos, walkDirection));

    return true;
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Inserts count spaces starting from the cursor's current position, moving over the existing text
// - for example, if the buffer looks like this ('|' is the cursor): [abc|def]
// - calling InsertCharacter(1) will change it to: [abc| def],
// - i.e. the 'def' gets shifted over 1 space and **retain their previous text attributes**
// Arguments:
// - count, the number of spaces to insert
// Return value:
// - true if succeeded, false otherwise
bool Terminal::InsertCharacter(const size_t count) noexcept
try
{
    // NOTE: the code below is _extremely_ similar to DeleteCharacter
    // We will want to use this same logic and implement a helper function instead
    // that does the 'move a region from here to there' operation
    // TODO: Github issue #2163
    SHORT dist;
    if (!SUCCEEDED(SizeTToShort(count, &dist)))
    {
        return false;
    }
    const auto cursorPos = _buffer->GetCursor().GetPosition();
    const auto copyFromPos = cursorPos;
    const COORD copyToPos{ cursorPos.X + dist, cursorPos.Y };
    const auto sourceWidth = _mutableViewport.RightExclusive() - copyFromPos.X;
    SHORT width;
    if (!SUCCEEDED(UIntToShort(sourceWidth, &width)))
    {
        return false;
    }

    // Get a rectangle of the source
    auto source = Viewport::FromDimensions(copyFromPos, width, 1);
    const auto sourceOrigin = source.Origin();

    // Get a rectangle of the target
    const auto target = Viewport::FromDimensions(copyToPos, source.Dimensions());
    const auto walkDirection = Viewport::DetermineWalkDirection(source, target);

    auto sourcePos = source.GetWalkOrigin(walkDirection);
    auto targetPos = target.GetWalkOrigin(walkDirection);

    // Iterate over the source cell data and copy it over to the target
    do
    {
        const auto data = OutputCell(*(_buffer->GetCellDataAt(sourcePos)));
        _buffer->Write(OutputCellIterator({ &data, 1 }), targetPos);
    } while (source.WalkInBounds(sourcePos, walkDirection) && target.WalkInBounds(targetPos, walkDirection));
    const auto eraseIter = OutputCellIterator(UNICODE_SPACE, _buffer->GetCurrentAttributes(), dist);
    _buffer->Write(eraseIter, cursorPos);

    return true;
}
CATCH_LOG_RETURN_FALSE()

bool Terminal::EraseCharacters(const size_t numChars) noexcept
try
{
    const auto absoluteCursorPos = _buffer->GetCursor().GetPosition();
    const auto viewport = _GetMutableViewport();
    const short distanceToRight = viewport.RightExclusive() - absoluteCursorPos.X;
    const short fillLimit = std::min(static_cast<short>(numChars), distanceToRight);
    const auto eraseIter = OutputCellIterator(UNICODE_SPACE, _buffer->GetCurrentAttributes(), fillLimit);
    _buffer->Write(eraseIter, absoluteCursorPos);
    return true;
}
CATCH_LOG_RETURN_FALSE()

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
bool Terminal::EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept
try
{
    const auto cursorPos = _buffer->GetCursor().GetPosition();
    const auto viewport = _GetMutableViewport();
    COORD startPos = { 0 };
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
    case DispatchTypes::EraseType::Scrollback:
        return false;
    }

    const auto eraseIter = OutputCellIterator(UNICODE_SPACE, _buffer->GetCurrentAttributes(), nlength);

    // Explicitly turn off end-of-line wrap-flag-setting when erasing cells.
    _buffer->Write(eraseIter, startPos, false);
    return true;
}
CATCH_LOG_RETURN_FALSE()

// Method description:
// - erases text in the buffer in two ways depending on erase type
// 1. 'erases' all text visible to the user (i.e. the text in the viewport)
// 2. erases all the text in the scrollback
// Arguments:
// - the erase type
// Return Value:
// - true if succeeded, false otherwise
bool Terminal::EraseInDisplay(const DispatchTypes::EraseType eraseType) noexcept
try
{
    // Store the relative cursor position so we can restore it later after we move the viewport
    const auto cursorPos = _buffer->GetCursor().GetPosition();
#pragma warning(suppress : 26496) // This is written by ConvertToOrigin, cpp core checks is wrong saying it should be const.
    auto relativeCursor = cursorPos;
    _mutableViewport.ConvertToOrigin(&relativeCursor);

    // Initialize the new location of the viewport
    // the top and bottom parameters are determined by the eraseType
    SMALL_RECT newWin;
    newWin.Left = _mutableViewport.Left();
    newWin.Right = _mutableViewport.RightExclusive();

    if (eraseType == DispatchTypes::EraseType::All)
    {
        // In this case, we simply move the viewport down, effectively pushing whatever text was on the screen into the scrollback
        // and thus 'erasing' the text visible to the user
        const auto coordLastChar = _buffer->GetLastNonSpaceCharacter(_mutableViewport);
        if (coordLastChar.X == 0 && coordLastChar.Y == 0)
        {
            // Nothing to clear, just return
            return true;
        }

        short sNewTop = coordLastChar.Y + 1;

        // Increment the circular buffer only if the new location of the viewport would be 'below' the buffer
        const short delta = (sNewTop + _mutableViewport.Height()) - (_buffer->GetSize().Height());
        for (auto i = 0; i < delta; i++)
        {
            _buffer->IncrementCircularBuffer();
            sNewTop--;
        }

        newWin.Top = sNewTop;
        newWin.Bottom = sNewTop + _mutableViewport.Height();
    }
    else if (eraseType == DispatchTypes::EraseType::Scrollback)
    {
        // We only want to erase the scrollback, and leave everything else on the screen as it is
        // so we grab the text in the viewport and rotate it up to the top of the buffer
        COORD scrollFromPos{ 0, 0 };
        _mutableViewport.ConvertFromOrigin(&scrollFromPos);
        _buffer->ScrollRows(scrollFromPos.Y, _mutableViewport.Height(), -scrollFromPos.Y);

        // Since we only did a rotation, the text that was in the scrollback is now _below_ where we are going to move the viewport
        // and we have to make sure we erase that text
        const auto eraseStart = _mutableViewport.Height();
        const auto eraseEnd = _buffer->GetLastNonSpaceCharacter(_mutableViewport).Y;
        for (SHORT i = eraseStart; i <= eraseEnd; i++)
        {
            _buffer->GetRowByOffset(i).Reset(_buffer->GetCurrentAttributes());
        }

        // Reset the scroll offset now because there's nothing for the user to 'scroll' to
        _scrollOffset = 0;

        newWin.Top = 0;
        newWin.Bottom = _mutableViewport.Height();
    }
    else
    {
        return false;
    }

    // Move the viewport, adjust the scroll bar if needed, and restore the old cursor position
    _mutableViewport = Viewport::FromExclusive(newWin);
    Terminal::_NotifyScrollEvent();
    SetCursorPosition(relativeCursor.X, relativeCursor.Y);

    return true;
}
CATCH_LOG_RETURN_FALSE()

bool Terminal::SetWindowTitle(std::wstring_view title) noexcept
try
{
    if (!_suppressApplicationTitle)
    {
        _title.emplace(title);
        _pfnTitleChanged(_title.value());
    }
    return true;
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Updates the value in the colortable at index tableIndex to the new color
//   color. color is a COLORREF, format 0x00BBGGRR.
// Arguments:
// - tableIndex: the index of the color table to update.
// - color: the new COLORREF to use as that color table value.
// Return Value:
// - true iff we successfully updated the color table entry.
bool Terminal::SetColorTableEntry(const size_t tableIndex, const COLORREF color) noexcept
try
{
    _colorTable.at(tableIndex) = color;

    // Repaint everything - the colors might have changed
    _buffer->GetRenderTarget().TriggerRedrawAll();
    return true;
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Sets the cursor style to the given style.
// Arguments:
// - cursorStyle: the style to be set for the cursor
// Return Value:
// - true iff we successfully set the cursor style
bool Terminal::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) noexcept
{
    CursorType finalCursorType = CursorType::Legacy;
    bool shouldBlink = false;

    switch (cursorStyle)
    {
    case DispatchTypes::CursorStyle::BlinkingBlockDefault:
        [[fallthrough]];
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
        finalCursorType = CursorType::Legacy;
        shouldBlink = false;
    }

    _buffer->GetCursor().SetType(finalCursorType);
    _buffer->GetCursor().SetBlinkingAllowed(shouldBlink);

    return true;
}

// Method Description:
// - Updates the default foreground color from a COLORREF, format 0x00BBGGRR.
// Arguments:
// - color: the new COLORREF to use as the default foreground color
// Return Value:
// - true
bool Terminal::SetDefaultForeground(const COLORREF color) noexcept
try
{
    _defaultFg = color;

    // Repaint everything - the colors might have changed
    _buffer->GetRenderTarget().TriggerRedrawAll();
    return true;
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Updates the default background color from a COLORREF, format 0x00BBGGRR.
// Arguments:
// - color: the new COLORREF to use as the default background color
// Return Value:
// - true
bool Terminal::SetDefaultBackground(const COLORREF color) noexcept
try
{
    _defaultBg = color;
    _pfnBackgroundColorChanged(color);

    // Repaint everything - the colors might have changed
    _buffer->GetRenderTarget().TriggerRedrawAll();
    return true;
}
CATCH_LOG_RETURN_FALSE()

bool Terminal::EnableWin32InputMode(const bool win32InputMode) noexcept
{
    _terminalInput->ChangeWin32InputMode(win32InputMode);
    return true;
}

bool Terminal::SetCursorKeysMode(const bool applicationMode) noexcept
{
    _terminalInput->ChangeCursorKeysMode(applicationMode);
    return true;
}

bool Terminal::SetKeypadMode(const bool applicationMode) noexcept
{
    _terminalInput->ChangeKeypadMode(applicationMode);
    return true;
}

bool Terminal::EnableVT200MouseMode(const bool enabled) noexcept
{
    _terminalInput->EnableDefaultTracking(enabled);
    return true;
}

bool Terminal::EnableUTF8ExtendedMouseMode(const bool enabled) noexcept
{
    _terminalInput->SetUtf8ExtendedMode(enabled);
    return true;
}

bool Terminal::EnableSGRExtendedMouseMode(const bool enabled) noexcept
{
    _terminalInput->SetSGRExtendedMode(enabled);
    return true;
}

bool Terminal::EnableButtonEventMouseMode(const bool enabled) noexcept
{
    _terminalInput->EnableButtonEventTracking(enabled);
    return true;
}

bool Terminal::EnableAnyEventMouseMode(const bool enabled) noexcept
{
    _terminalInput->EnableAnyEventTracking(enabled);
    return true;
}

bool Terminal::EnableAlternateScrollMode(const bool enabled) noexcept
{
    _terminalInput->EnableAlternateScroll(enabled);
    return true;
}

bool Terminal::IsVtInputEnabled() const noexcept
{
    // We should never be getting this call in Terminal.
    FAIL_FAST();
}

bool Terminal::SetCursorVisibility(const bool visible) noexcept
{
    _buffer->GetCursor().SetIsVisible(visible);
    return true;
}

bool Terminal::EnableCursorBlinking(const bool enable) noexcept
{
    _buffer->GetCursor().SetBlinkingAllowed(enable);

    // GH#2642 - From what we've gathered from other terminals, when blinking is
    // disabled, the cursor should remain On always, and have the visibility
    // controlled by the IsVisible property. So when you do a printf "\e[?12l"
    // to disable blinking, the cursor stays stuck On. At this point, only the
    // cursor visibility property controls whether the user can see it or not.
    // (Yes, the cursor can be On and NOT Visible)
    _buffer->GetCursor().SetIsOn(true);
    return true;
}
