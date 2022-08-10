// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/IRenderData.hpp"

#include "../buffer/out/textBuffer.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;

__declspec(noinline) void RenderData::Snapshot(IRenderData* other)
{
    viewport = other->GetViewport();
    textBuffer.CopyViewport(other->GetTextBuffer(), viewport);
    fontInfo = other->GetFontInfo();
    selectionRects = other->GetSelectionRects();

    cursorPosition = other->GetCursorPosition();
    cursorVisible = other->IsCursorVisible();
    cursorOn = other->IsCursorOn();
    cursorHeight = other->GetCursorHeight();
    cursorStyle = other->GetCursorStyle();
    cursorPixelWidth = other->GetCursorPixelWidth();
    cursorDoubleWidth = other->IsCursorDoubleWidth();
    overlays = other->GetOverlays();
    gridLineDrawingAllowed = other->IsGridLineDrawingAllowed();
    consoleTitle = other->GetConsoleTitle();
    
    assert(viewport.Top() == 0);
    assert(viewport.Left() == 0);
    viewport = viewport.ToOrigin();
}

Types::Viewport RenderData::GetViewport() noexcept
{
    return viewport;
}

til::point RenderData::GetTextBufferEndPosition() const noexcept
{
    std::terminate();
}

const TextBuffer& RenderData::GetTextBuffer() const noexcept
{
    return textBuffer;
}

const FontInfo& RenderData::GetFontInfo() const noexcept
{
    return fontInfo;
}

std::vector<Types::Viewport> RenderData::GetSelectionRects() noexcept
{
    return selectionRects;
}

void RenderData::LockConsole() noexcept
{
    std::terminate();
}

void RenderData::UnlockConsole() noexcept
{
    std::terminate();
}

til::point RenderData::GetCursorPosition() const noexcept
{
    return cursorPosition;
}

bool RenderData::IsCursorVisible() const noexcept
{
    return cursorVisible;
}

bool RenderData::IsCursorOn() const noexcept
{
    return cursorOn;
}

ULONG RenderData::GetCursorHeight() const noexcept
{
    return cursorHeight;
}

CursorType RenderData::GetCursorStyle() const noexcept
{
    return cursorStyle;
}

ULONG RenderData::GetCursorPixelWidth() const noexcept
{
    return cursorPixelWidth;
}

bool RenderData::IsCursorDoubleWidth() const
{
    return cursorDoubleWidth;
}

const std::vector<RenderOverlay> RenderData::GetOverlays() const noexcept
{
    return overlays;
}

const bool RenderData::IsGridLineDrawingAllowed() noexcept
{
    return gridLineDrawingAllowed;
}

const std::wstring_view RenderData::GetConsoleTitle() const noexcept
{
    return consoleTitle;
}

const std::wstring RenderData::GetHyperlinkUri(uint16_t id) const noexcept
{
    UNREFERENCED_PARAMETER(id);
    std::terminate();
}

const std::wstring RenderData::GetHyperlinkCustomId(uint16_t id) const noexcept
{
    UNREFERENCED_PARAMETER(id);
    std::terminate();
}

const std::vector<size_t> RenderData::GetPatternId(const til::point location) const noexcept
{
    UNREFERENCED_PARAMETER(location);
    return {};
}
