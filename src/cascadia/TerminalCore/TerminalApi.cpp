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
    attrs.SetIndexedAttributes({ colorIndex }, {});
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetTextBackgroundIndex(BYTE colorIndex) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetIndexedAttributes({}, { colorIndex });
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
    if (boldOn)
    {
        attrs.Embolden();
    }
    else
    {
        attrs.Debolden();
    }
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::UnderlineText(bool underlineOn) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    WORD metaAttrs = attrs.GetMetaAttributes();

    WI_UpdateFlag(metaAttrs, COMMON_LVB_UNDERSCORE, underlineOn);

    attrs.SetMetaAttributes(metaAttrs);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::ReverseText(bool reversed) noexcept
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    WORD metaAttrs = attrs.GetMetaAttributes();

    WI_UpdateFlag(metaAttrs, COMMON_LVB_REVERSE_VIDEO, reversed);

    attrs.SetMetaAttributes(metaAttrs);
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

    // Move the viewport, adjust the scoll bar if needed, and restore the old cursor position
    _mutableViewport = Viewport::FromExclusive(newWin);
    Terminal::_NotifyScrollEvent();
    SetCursorPosition(relativeCursor.X, relativeCursor.Y);

    return true;
}
CATCH_LOG_RETURN_FALSE()

bool Terminal::SetWindowTitle(std::wstring_view title) noexcept
try
{
    _title = _suppressApplicationTitle ? _startingTitle : title;

    _pfnTitleChanged(_title);

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
