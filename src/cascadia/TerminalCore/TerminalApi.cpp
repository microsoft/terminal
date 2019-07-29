// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

// Print puts the text in the buffer and moves the cursor
bool Terminal::PrintString(std::wstring_view stringView)
{
    _WriteBuffer(stringView);
    return true;
}

bool Terminal::ExecuteChar(wchar_t wch)
{
    std::wstring_view view{ &wch, 1 };
    _WriteBuffer(view);
    return true;
}

bool Terminal::SetTextToDefaults(bool foreground, bool background)
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

bool Terminal::SetTextForegroundIndex(BYTE colorIndex)
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetIndexedAttributes({ colorIndex }, {});
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetTextBackgroundIndex(BYTE colorIndex)
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetIndexedAttributes({}, { colorIndex });
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetTextRgbColor(COLORREF color, bool foreground)
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    attrs.SetColor(color, foreground);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::BoldText(bool boldOn)
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

bool Terminal::UnderlineText(bool underlineOn)
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    WORD metaAttrs = attrs.GetMetaAttributes();

    WI_UpdateFlag(metaAttrs, COMMON_LVB_UNDERSCORE, underlineOn);

    attrs.SetMetaAttributes(metaAttrs);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::ReverseText(bool reversed)
{
    TextAttribute attrs = _buffer->GetCurrentAttributes();
    WORD metaAttrs = attrs.GetMetaAttributes();

    WI_UpdateFlag(metaAttrs, COMMON_LVB_REVERSE_VIDEO, reversed);

    attrs.SetMetaAttributes(metaAttrs);
    _buffer->SetCurrentAttributes(attrs);
    return true;
}

bool Terminal::SetCursorPosition(short x, short y)
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

COORD Terminal::GetCursorPosition()
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

bool Terminal::DeleteCharacter(const unsigned int uiCount)
{
    SHORT dist;
    if (!SUCCEEDED(UIntToShort(uiCount, &dist)))
    {
        return false;
    }
    const auto cursorPos = _buffer->GetCursor().GetPosition();

    // We are going to call EraseInLine, so first we copy what we don't want to delete
    COORD keepFromPos{ cursorPos.X + dist, cursorPos.Y };
    auto keepText = _buffer->GetTextLineDataAt(keepFromPos);
    std::wstring copied;
    while (keepText)
    {
        copied.append(*keepText);
        ++keepText;
    }
    EraseInLine(DispatchTypes::EraseType::ToEnd);

    // Now we rewrite what we deleted
    auto rewriteIter = OutputCellIterator(copied, _buffer->GetCurrentAttributes());
    _buffer->Write(rewriteIter, cursorPos);
    return true;
}

bool Terminal::EraseCharacters(const unsigned int numChars)
{
    const auto absoluteCursorPos = _buffer->GetCursor().GetPosition();
    const auto viewport = _GetMutableViewport();
    const short distanceToRight = viewport.RightExclusive() - absoluteCursorPos.X;
    const short fillLimit = std::min(static_cast<short>(numChars), distanceToRight);
    auto eraseIter = OutputCellIterator(L' ', _buffer->GetCurrentAttributes(), fillLimit);
    _buffer->Write(eraseIter, absoluteCursorPos);
    return true;
}

bool Terminal::EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType)
{
    const auto cursorPos = _buffer->GetCursor().GetPosition();
    const auto viewport = _GetMutableViewport();
    COORD startPos = { 0 };
    startPos.Y = cursorPos.Y;
    DWORD nlength = 0;

    // Determine startPos.X and nlength by the eraseType
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
    {
        nlength = cursorPos.X - viewport.Left() + 1;
        break;
    }
    case DispatchTypes::EraseType::ToEnd:
        startPos.X = cursorPos.X;
        nlength = viewport.RightInclusive() - startPos.X;
        break;
    case DispatchTypes::EraseType::All:
        startPos.X = viewport.Left();
        nlength = viewport.RightInclusive() - startPos.X;
        break;
    }

    auto eraseIter = OutputCellIterator(L' ', _buffer->GetCurrentAttributes(), nlength);
    _buffer->Write(eraseIter, startPos);
    return true;
}

bool Terminal::EraseInDisplay(const DispatchTypes::EraseType eraseType)
{
    // Store the relative cursor position so we can restore it later after we move the viewport
    const auto cursorPos = _buffer->GetCursor().GetPosition();
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
        const auto coordLastChar = _buffer->GetLastNonSpaceCharacter(_mutableViewport);
        if (coordLastChar.X == 0 && coordLastChar.Y == 0)
        {
            // Nothing to clear, just return
            return true;
        }

        short sNewTop = coordLastChar.Y + 1;

        // Increment the circular buffer only if the new location of the viewport would be 'below' the buffer
        short delta = (sNewTop + _mutableViewport.Height()) - (_buffer->GetSize().Height());
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
        auto eraseIter = OutputCellIterator(L' ', _buffer->GetCurrentAttributes(), _mutableViewport.RightInclusive());
        auto eraseStart = _mutableViewport.Height();
        auto eraseEnd = _buffer->GetLastNonSpaceCharacter(_mutableViewport).Y;
        for (SHORT i = eraseStart; i <= eraseEnd; i++)
        {
            COORD erasePos{ 0, i };
            _buffer->Write(eraseIter, erasePos);
        }

        newWin.Top = 0;
        newWin.Bottom = _mutableViewport.Height();
    }

    // Move the viewport, adjust the scoll bar if needed, and restore the old cursor position
    _mutableViewport = Viewport::FromExclusive(newWin);
    Terminal::_NotifyScrollEvent();
    SetCursorPosition(relativeCursor.X, relativeCursor.Y);

    return true;
}

bool Terminal::SetWindowTitle(std::wstring_view title)
{
    _title = title;

    if (_pfnTitleChanged)
    {
        _pfnTitleChanged(title);
    }

    return true;
}

// Method Description:
// - Updates the value in the colortable at index tableIndex to the new color
//   dwColor. dwColor is a COLORREF, format 0x00BBGGRR.
// Arguments:
// - tableIndex: the index of the color table to update.
// - dwColor: the new COLORREF to use as that color table value.
// Return Value:
// - true iff we successfully updated the color table entry.
bool Terminal::SetColorTableEntry(const size_t tableIndex, const COLORREF dwColor)
{
    if (tableIndex > _colorTable.size())
    {
        return false;
    }
    _colorTable.at(tableIndex) = dwColor;

    // Repaint everything - the colors might have changed
    _buffer->GetRenderTarget().TriggerRedrawAll();
    return true;
}

// Method Description:
// - Sets the cursor style to the given style.
// Arguments:
// - cursorStyle: the style to be set for the cursor
// Return Value:
// - true iff we successfully set the cursor style
bool Terminal::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle)
{
    CursorType finalCursorType;
    bool fShouldBlink;

    switch (cursorStyle)
    {
    case DispatchTypes::CursorStyle::BlinkingBlockDefault:
        [[fallthrough]];
    case DispatchTypes::CursorStyle::BlinkingBlock:
        finalCursorType = CursorType::FullBox;
        fShouldBlink = true;
        break;
    case DispatchTypes::CursorStyle::SteadyBlock:
        finalCursorType = CursorType::FullBox;
        fShouldBlink = false;
        break;
    case DispatchTypes::CursorStyle::BlinkingUnderline:
        finalCursorType = CursorType::Underscore;
        fShouldBlink = true;
        break;
    case DispatchTypes::CursorStyle::SteadyUnderline:
        finalCursorType = CursorType::Underscore;
        fShouldBlink = false;
        break;
    case DispatchTypes::CursorStyle::BlinkingBar:
        finalCursorType = CursorType::VerticalBar;
        fShouldBlink = true;
        break;
    case DispatchTypes::CursorStyle::SteadyBar:
        finalCursorType = CursorType::VerticalBar;
        fShouldBlink = false;
        break;
    default:
        finalCursorType = CursorType::Legacy;
        fShouldBlink = false;
    }

    _buffer->GetCursor().SetType(finalCursorType);
    _buffer->GetCursor().SetBlinkingAllowed(fShouldBlink);

    return true;
}

// Method Description:
// - Updates the default foreground color from a COLORREF, format 0x00BBGGRR.
// Arguments:
// - dwColor: the new COLORREF to use as the default foreground color
// Return Value:
// - true
bool Terminal::SetDefaultForeground(const COLORREF dwColor)
{
    _defaultFg = dwColor;

    // Repaint everything - the colors might have changed
    _buffer->GetRenderTarget().TriggerRedrawAll();
    return true;
}

// Method Description:
// - Updates the default background color from a COLORREF, format 0x00BBGGRR.
// Arguments:
// - dwColor: the new COLORREF to use as the default background color
// Return Value:
// - true
bool Terminal::SetDefaultBackground(const COLORREF dwColor)
{
    _defaultBg = dwColor;
    _pfnBackgroundColorChanged(dwColor);

    // Repaint everything - the colors might have changed
    _buffer->GetRenderTarget().TriggerRedrawAll();
    return true;
}
