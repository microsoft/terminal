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
