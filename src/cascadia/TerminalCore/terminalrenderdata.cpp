// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "ColorFix.hpp"
#include <DefaultSettings.h>

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;

static constexpr size_t DefaultBgIndex{ 16 };
static constexpr size_t DefaultFgIndex{ 17 };

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

const FontInfo& Terminal::GetFontInfo() noexcept
{
    return _fontInfo;
}

void Terminal::SetFontInfo(const FontInfo& fontInfo)
{
    _fontInfo = fontInfo;
}

std::pair<COLORREF, COLORREF> Terminal::GetAttributeColors(const TextAttribute& attr) const noexcept
{
    std::pair<COLORREF, COLORREF> colors;
    _blinkingState.RecordBlinkingUsage(attr);
    const auto fgTextColor = attr.GetForeground();
    const auto bgTextColor = attr.GetBackground();

    // We want to nudge the foreground color to make it more perceivable only for the
    // default color pairs within the color table
    if (_adjustIndistinguishableColors &&
        !(attr.IsFaint() || (attr.IsBlinking() && _blinkingState.IsBlinkingFaint())) &&
        (fgTextColor.IsDefault() || fgTextColor.IsLegacy()) &&
        (bgTextColor.IsDefault() || bgTextColor.IsLegacy()))
    {
        const auto bgIndex = bgTextColor.IsDefault() ? DefaultBgIndex : bgTextColor.GetIndex();
        auto fgIndex = fgTextColor.IsDefault() ? DefaultFgIndex : fgTextColor.GetIndex();

        if (fgTextColor.IsIndex16() && (fgIndex < 8) && attr.IsBold() && _intenseIsBright)
        {
            // There is a special case for bold here - we need to get the bright version of the foreground color
            fgIndex += 8;
        }

        if (attr.IsReverseVideo() ^ _screenReversed)
        {
            colors.first = _adjustedForegroundColors[fgIndex][bgIndex];
            colors.second = fgTextColor.GetColor(_colorTable, TextColor::DEFAULT_FOREGROUND);
        }
        else
        {
            colors.first = _adjustedForegroundColors[bgIndex][fgIndex];
            colors.second = bgTextColor.GetColor(_colorTable, TextColor::DEFAULT_BACKGROUND);
        }
    }
    else
    {
        colors = attr.CalculateRgbColors(_colorTable,
                                         TextColor::DEFAULT_FOREGROUND,
                                         TextColor::DEFAULT_BACKGROUND,
                                         _screenReversed,
                                         _blinkingState.IsBlinkingFaint(),
                                         _intenseIsBright);
    }
    colors.first |= 0xff000000;
    // We only care about alpha for the default BG (which enables acrylic)
    // If the bg isn't the default bg color, or reverse video is enabled, make it fully opaque.
    if (!attr.BackgroundIsDefault() || (attr.IsReverseVideo() ^ _screenReversed))
    {
        colors.second |= 0xff000000;
    }
    return colors;
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
    return _colorTable.at(TextColor::CURSOR_COLOR);
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

const std::wstring Microsoft::Terminal::Core::Terminal::GetHyperlinkUri(uint16_t id) const noexcept
{
    return _buffer->GetHyperlinkUriFromId(id);
}

const std::wstring Microsoft::Terminal::Core::Terminal::GetHyperlinkCustomId(uint16_t id) const noexcept
{
    return _buffer->GetCustomIdFromId(id);
}

// Method Description:
// - Gets the regex pattern ids of a location
// Arguments:
// - The location
// Return value:
// - The pattern IDs of the location
const std::vector<size_t> Terminal::GetPatternId(const COORD location) const noexcept
{
    // Look through our interval tree for this location
    const auto intervals = _patternIntervalTree.findOverlapping(COORD{ location.X + 1, location.Y }, location);
    if (intervals.size() == 0)
    {
        return {};
    }
    else
    {
        std::vector<size_t> result{};
        for (const auto& interval : intervals)
        {
            result.emplace_back(interval.value);
        }
        return result;
    }
    return {};
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
    SetSelectionEnd(realCoordEnd, SelectionExpansion::Char);
}

const std::wstring_view Terminal::GetConsoleTitle() const noexcept
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
    _readWriteLock.lock();
#ifndef NDEBUG
    _lastLocker = GetCurrentThreadId();
#endif
}

// Method Description:
// - Unlocks the terminal after a call to Terminal::LockConsole.
void Terminal::UnlockConsole() noexcept
{
    _readWriteLock.unlock();
}

// Method Description:
// - Returns whether the screen is inverted;
// Return Value:
// - false.
bool Terminal::IsScreenReversed() const noexcept
{
    return _screenReversed;
}

const bool Terminal::IsUiaDataInitialized() const noexcept
{
    // GH#11135: Windows Terminal needs to create and return an automation peer
    // when a screen reader requests it. However, the terminal might not be fully
    // initialized yet. So we use this to check if any crucial components of
    // UiaData are not yet initialized.
    return !!_buffer;
}

// Method Description:
// - Creates the adjusted color array, which contains the possible foreground colors,
//   adjusted for perceivability
// - The adjusted color array is 2-d, and effectively maps a background and foreground
//   color pair to the adjusted foreground for that color pair
void Terminal::_MakeAdjustedColorArray()
{
    // The color table has 16 colors, but the adjusted color table needs to be 18
    // to include the default background and default foreground colors
    std::array<COLORREF, 18> colorTableWithDefaults;
    std::copy_n(std::begin(_colorTable), 16, std::begin(colorTableWithDefaults));
    colorTableWithDefaults[DefaultBgIndex] = _colorTable.at(TextColor::DEFAULT_BACKGROUND);
    colorTableWithDefaults[DefaultFgIndex] = _colorTable.at(TextColor::DEFAULT_FOREGROUND);
    for (auto fgIndex = 0; fgIndex < 18; ++fgIndex)
    {
        const auto fg = til::at(colorTableWithDefaults, fgIndex);
        for (auto bgIndex = 0; bgIndex < 18; ++bgIndex)
        {
            if (fgIndex == bgIndex)
            {
                _adjustedForegroundColors[bgIndex][fgIndex] = fg;
            }
            else
            {
                const auto bg = til::at(colorTableWithDefaults, bgIndex);
                _adjustedForegroundColors[bgIndex][fgIndex] = ColorFix::GetPerceivableColor(fg, bg);
            }
        }
    }
}
