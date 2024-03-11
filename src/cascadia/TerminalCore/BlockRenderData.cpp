// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BlockRenderData.hpp"
#include <DefaultSettings.h>

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;

BlockRenderData::BlockRenderData(Terminal& terminal, til::CoordType virtualTop) :
    _terminal{ terminal },
    _virtualTop{ virtualTop }
{
}

void BlockRenderData::SetBottom(til::CoordType bottom)
{
    _virtualBottom = bottom;
}

////////////////////////////////////////////////////////////////////////////////
// GetViewport is part of the IRenderData interface that we actually changed.
////////////////////////////////////////////////////////////////////////////////

Viewport BlockRenderData::GetViewport() noexcept
{
    const auto terminalViewport = _terminal.GetViewport().ToInclusive();
    const til::rect terminalExclusive{ terminalViewport };

    auto lastMutableViewportBottom = _virtualBottom.has_value() ? *_virtualBottom : _terminal.GetBufferHeight();
    auto lastMutableViewportTop = std::max(0, lastMutableViewportBottom - terminalExclusive.height());
    // auto numScrollbackRows = std::max(0, lastMutableViewportTop - _virtualTop);
    auto clampedTop = std::max(_virtualTop, lastMutableViewportTop);

    const til::inclusive_rect finalViewport{
        .left = terminalViewport.left,
        // .top = std::max(_virtualTop, terminalViewport.top),
        .top = clampedTop - _scrollOffset,
        .right = terminalViewport.right,
        .bottom = _virtualBottom.has_value() ?
                      std::min(*_virtualBottom, terminalViewport.bottom) :
                      std::max(terminalViewport.bottom, _virtualTop + terminalExclusive.height()),
    };
    return Viewport::FromInclusive(finalViewport);
}

til::CoordType BlockRenderData::GetBufferHeight() const noexcept
{
    // TODO! I think the GetTextBufferEndPosition().Y just needs a +1. It's hard
    // to say for sure. TerminalRenderData returns
    //
    //   { _GetMutableViewport().Width() - 1, ViewEndIndex() };
    //
    // and ViewEndIndex() { return _inAltBuffer() ? _altBufferSize.height - 1 : _mutableViewport.BottomInclusive(); }

    const auto bottom = _virtualBottom.has_value() ?
                            std::min(*_virtualBottom, _terminal.GetBufferHeight()) :
                            _terminal.GetBufferHeight();

    return bottom - _virtualTop;
}

void BlockRenderData::UserScrollViewport(const int viewTop)
{
    const auto terminalViewport = _terminal.GetViewport().ToInclusive();
    const til::rect terminalExclusive{ terminalViewport };

    auto lastMutableViewportBottom = _virtualBottom.has_value() ? *_virtualBottom : _terminal.GetBufferHeight();
    auto lastMutableViewportTop = std::max(0, lastMutableViewportBottom - terminalExclusive.height());
    auto numScrollbackRows = std::max(0, lastMutableViewportTop - _virtualTop);

    if (viewTop <= _virtualTop)
    {
        _scrollOffset = numScrollbackRows;
    }
    else
    {
        // scrolled somewhere below virtual top

        _scrollOffset = std::max(0, lastMutableViewportTop - viewTop);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Everything down here: blind pass-through
////////////////////////////////////////////////////////////////////////////////

til::point BlockRenderData::GetTextBufferEndPosition() const noexcept
{
    return _terminal.GetTextBufferEndPosition();
}

const TextBuffer& BlockRenderData::GetTextBuffer() const noexcept
{
    return _terminal.GetTextBuffer();
}

const FontInfo& BlockRenderData::GetFontInfo() const noexcept
{
    return _terminal.GetFontInfo();
}

// void BlockRenderData::SetFontInfo(const FontInfo& fontInfo)
// {
//     return _terminal.SetFontInfo();
// }

til::point BlockRenderData::GetCursorPosition() const noexcept
{
    return _terminal.GetCursorPosition();
}

bool BlockRenderData::IsCursorVisible() const noexcept
{
    return _terminal.IsCursorVisible();
}

bool BlockRenderData::IsCursorOn() const noexcept
{
    return _terminal.IsCursorOn();
}

ULONG BlockRenderData::GetCursorPixelWidth() const noexcept
{
    return _terminal.GetCursorPixelWidth();
}

ULONG BlockRenderData::GetCursorHeight() const noexcept
{
    return _terminal.GetCursorHeight();
}

CursorType BlockRenderData::GetCursorStyle() const noexcept
{
    return _terminal.GetCursorStyle();
}

bool BlockRenderData::IsCursorDoubleWidth() const
{
    return _terminal.IsCursorDoubleWidth();
}

const std::vector<RenderOverlay> BlockRenderData::GetOverlays() const noexcept
{
    return _terminal.GetOverlays();
}

const bool BlockRenderData::IsGridLineDrawingAllowed() noexcept
{
    return _terminal.IsGridLineDrawingAllowed();
}

const std::wstring BlockRenderData::GetHyperlinkUri(uint16_t id) const
{
    return _terminal.GetHyperlinkUri(id);
}

const std::wstring BlockRenderData::GetHyperlinkCustomId(uint16_t id) const
{
    return _terminal.GetHyperlinkCustomId(id);
}

// Method Description:
// - Gets the regex pattern ids of a location
// Arguments:
// - The location
// Return value:
// - The pattern IDs of the location
const std::vector<size_t> BlockRenderData::GetPatternId(const til::point location) const
{
    return _terminal.GetPatternId(location);
}

std::pair<COLORREF, COLORREF> BlockRenderData::GetAttributeColors(const TextAttribute& attr) const noexcept
{
    return _terminal.GetAttributeColors(attr);
}

std::vector<Microsoft::Console::Types::Viewport> BlockRenderData::GetSelectionRects() noexcept

{
    return _terminal.GetSelectionRects();
}

std::vector<Microsoft::Console::Types::Viewport> BlockRenderData::GetSearchSelectionRects() noexcept
{
    return _terminal.GetSearchSelectionRects();
}

void BlockRenderData::SelectNewRegion(const til::point coordStart, const til::point coordEnd)
{
    return _terminal.SelectNewRegion(coordStart, coordEnd);
}

void BlockRenderData::SelectSearchRegions(std::vector<til::inclusive_rect> rects)
{
    return _terminal.SelectSearchRegions(rects);
}

const std::wstring_view BlockRenderData::GetConsoleTitle() const noexcept
{
    return _terminal.GetConsoleTitle();
}

void BlockRenderData::LockConsole() noexcept
{
    return _terminal.LockConsole();
}

void BlockRenderData::UnlockConsole() noexcept
{
    return _terminal.UnlockConsole();
}

const bool BlockRenderData::IsUiaDataInitialized() const noexcept
{
    return _terminal.IsUiaDataInitialized();
}

//////////////

const bool BlockRenderData::IsSelectionActive() const noexcept
{
    return _terminal.IsSelectionActive();
}
const bool BlockRenderData::IsBlockSelection() const noexcept
{
    return _terminal.IsBlockSelection();
}
void BlockRenderData::ClearSelection()
{
    return _terminal.ClearSelection();
}
const til::point BlockRenderData::GetSelectionAnchor() const noexcept
{
    return _terminal.GetSelectionAnchor();
}
const til::point BlockRenderData::GetSelectionEnd() const noexcept
{
    return _terminal.GetSelectionEnd();
}
