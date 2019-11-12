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

const TextBuffer& Terminal::GetTextBuffer() noexcept
{
    return *_buffer;
}

const FontInfo& Terminal::GetFontInfo() noexcept
{
    // TODO: This font value is only used to check if the font is a raster font.
    // Otherwise, the font is changed with the renderer via TriggerFontChange.
    // The renderer never uses any of the other members from the value returned
    //      by this method.
    // We could very likely replace this with just an IsRasterFont method
    //      (which would return false)
    static const FontInfo _fakeFontInfo(DEFAULT_FONT_FACE.c_str(), TMPF_TRUETYPE, 10, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false);
    return _fakeFontInfo;
}

const TextAttribute Terminal::GetDefaultBrushColors() noexcept
{
    return TextAttribute{};
}

const COLORREF Terminal::GetForegroundColor(const TextAttribute& attr) const noexcept
{
    return 0xff000000 | attr.CalculateRgbForeground({ &_colorTable[0], _colorTable.size() }, _defaultFg, _defaultBg);
}

const COLORREF Terminal::GetBackgroundColor(const TextAttribute& attr) const noexcept
{
    const auto bgColor = attr.CalculateRgbBackground({ &_colorTable[0], _colorTable.size() }, _defaultFg, _defaultBg);
    // We only care about alpha for the default BG (which enables acrylic)
    // If the bg isn't the default bg color, then make it fully opaque.
    if (!attr.BackgroundIsDefault())
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

bool Terminal::IsCursorDoubleWidth() const noexcept
{
    return false;
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
{
    std::vector<Viewport> result;

    for (const auto& lineRect : _GetSelectionRects())
    {
        result.emplace_back(Viewport::FromInclusive(lineRect));
    }

    return result;
}

void Terminal::SelectNewRegion(const COORD coordStart, const COORD coordEnd)
{
    SetSelectionAnchor(coordStart);
    SetEndSelectionPosition(coordEnd);
}

const std::wstring Terminal::GetConsoleTitle() const noexcept
{
    return _title;
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
