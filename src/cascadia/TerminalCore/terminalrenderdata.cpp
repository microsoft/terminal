// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include <DefaultSettings.h>
using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;

Viewport Terminal::GetViewport() noexcept
{
    return _GetVisibleViewport();
}

COORD Terminal::GetTextBufferEndPosition() const noexcept
{
    // We use the end line of mutableViewport as the end
    // of the text buffer, it always moves with the written
    // text
    COORD endPosition{ _GetMutableViewport().Width() - 1, gsl::narrow<short>(ViewEndIndex()) };
    return endPosition;
}

const TextBuffer& Terminal::GetTextBuffer() noexcept
{
    return *_buffer;
}

// Creating a FontInfo can technically throw (on string allocation) and this is noexcept.
// That means this will std::terminate. We could come back and make there be a default constructor
// backup to FontInfo that throws no exceptions and allocates a default FontInfo structure.
#pragma warning(push)
#pragma warning(disable : 26447)
const FontInfo& Terminal::GetFontInfo() noexcept
{
    // TODO: This font value is only used to check if the font is a raster font.
    // Otherwise, the font is changed with the renderer via TriggerFontChange.
    // The renderer never uses any of the other members from the value returned
    //      by this method.
    // We could very likely replace this with just an IsRasterFont method
    //      (which would return false)
    static const FontInfo _fakeFontInfo(DEFAULT_FONT_FACE, TMPF_TRUETYPE, 10, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false);
    return _fakeFontInfo;
}
#pragma warning(pop)

const TextAttribute Terminal::GetDefaultBrushColors() noexcept
{
    return TextAttribute{};
}

const COLORREF Terminal::GetForegroundColor(const TextAttribute& attr) const noexcept
{
    return 0xff000000 | attr.CalculateRgbForeground({ _colorTable.data(), _colorTable.size() }, _defaultFg, _defaultBg);
}

const COLORREF Terminal::GetBackgroundColor(const TextAttribute& attr) const noexcept
{
    const auto bgColor = attr.CalculateRgbBackground({ _colorTable.data(), _colorTable.size() }, _defaultFg, _defaultBg);
    // We only care about alpha for the default BG (which enables acrylic)
    // If the bg isn't the default bg color, or reverse video is enabled, make it fully opaque.
    if (!attr.BackgroundIsDefault() || attr.IsReverseVideo())
    {
        return 0xff000000 | bgColor;
    }
    return bgColor;
}

COORD Terminal::GetCursorPosition() const noexcept
{
    const auto& cursor = _buffer->GetCursor();
    return cursor.GetPosition();
}

bool Terminal::IsCursorVisible() const noexcept
{
    const auto& cursor = _buffer->GetCursor();
    return cursor.IsVisible() && !cursor.IsPopupShown();
}

bool Terminal::IsCursorOn() const noexcept
{
    const auto& cursor = _buffer->GetCursor();
    return cursor.IsOn();
}

ULONG Terminal::GetCursorPixelWidth() const noexcept
{
    return 1;
}

ULONG Terminal::GetCursorHeight() const noexcept
{
    return _buffer->GetCursor().GetSize();
}

CursorType Terminal::GetCursorStyle() const noexcept
{
    return _buffer->GetCursor().GetType();
}

COLORREF Terminal::GetCursorColor() const noexcept
{
    return _buffer->GetCursor().GetColor();
}

bool Terminal::IsCursorDoubleWidth() const
{
    const auto position = _buffer->GetCursor().GetPosition();
    TextBufferTextIterator it(TextBufferCellIterator(*_buffer, position));
    return IsGlyphFullWidth(*it);
}

const std::vector<RenderOverlay> Terminal::GetOverlays() const noexcept
{
    return {};
}

const bool Terminal::IsGridLineDrawingAllowed() noexcept
{
    return true;
}

std::vector<Microsoft::Console::Types::Viewport> Terminal::GetSelectionRects() noexcept
try
{
    std::vector<Viewport> result;

    for (const auto& lineRect : _GetSelectionRects())
    {
        result.emplace_back(Viewport::FromInclusive(lineRect));
    }

    return result;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
}

void Terminal::SelectNewRegion(const COORD coordStart, const COORD coordEnd)
{
#pragma warning(push)
#pragma warning(disable : 26496) // cpp core checks wants these const, but they're decremented below.
    COORD realCoordStart = coordStart;
    COORD realCoordEnd = coordEnd;
#pragma warning(pop)

    bool notifyScrollChange = false;
    if (coordStart.Y < _VisibleStartIndex())
    {
        // recalculate the scrollOffset
        _scrollOffset = ViewStartIndex() - coordStart.Y;
        notifyScrollChange = true;
    }
    else if (coordEnd.Y > _VisibleEndIndex())
    {
        // recalculate the scrollOffset, note that if the found text is
        // beneath the current visible viewport, it may be within the
        // current mutableViewport and the scrollOffset will be smaller
        // than 0
        _scrollOffset = std::max(0, ViewStartIndex() - coordStart.Y);
        notifyScrollChange = true;
    }

    if (notifyScrollChange)
    {
        _buffer->GetRenderTarget().TriggerScroll();
        _NotifyScrollEvent();
    }

    realCoordStart.Y -= gsl::narrow<short>(_VisibleStartIndex());
    realCoordEnd.Y -= gsl::narrow<short>(_VisibleStartIndex());

    SetSelectionAnchor(realCoordStart);
    SetSelectionEnd(realCoordEnd, SelectionExpansionMode::Cell);
}

const std::wstring Terminal::GetConsoleTitle() const noexcept
try
{
    if (_title.has_value())
    {
        return _title.value();
    }
    return _startingTitle;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
}

// Method Description:
// - Lock the terminal for reading the contents of the buffer. Ensures that the
//      contents of the terminal won't be changed in the middle of a paint
//      operation.
//   Callers should make sure to also call Terminal::UnlockConsole once
//      they're done with any querying they need to do.
void Terminal::LockConsole() noexcept
{
    _readWriteLock.lock_shared();
}

// Method Description:
// - Unlocks the terminal after a call to Terminal::LockConsole.
void Terminal::UnlockConsole() noexcept
{
    _readWriteLock.unlock_shared();
}

// Method Description:
// - Returns whether the screen is inverted;
//   This state is not currently known to Terminal.
// Return Value:
// - false.
bool Terminal::IsScreenReversed() const noexcept
{
    return false;
}
